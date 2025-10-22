/**
 * @file rv5s_vm.cpp
 * @brief RV5S VM implementation (5-stage pipeline, minimal, no hazards)
 */

#include "vm/rv5s/rv5s_vm.h"

#include "utils.h"
#include "globals.h"
#include "common/instructions.h"
#include "config.h"

using instruction_set::Instruction;
using instruction_set::get_instr_encoding;

RV5SVM::RV5SVM() : VmBase() {
	DumpRegisters(globals::registers_dump_file_path, registers_);
	DumpState(globals::vm_state_dump_file_path);
}

void RV5SVM::Reset() {
	memory_controller_.Reset();
	registers_.Reset();

	program_counter_ = 0;
	current_instruction_ = 0;
	cycle_s_ = 0; 
	instructions_retired_ = 0; 
	stall_cycles_ = 0; 
	mispredictions_ = 0;
	cpi_ = 0; 
	ipc_ = 0;

	control_.Reset();
	if_id_ = {}; id_ex_ = {}; ex_mem_ = {}; mem_wb_ = {};
}

bool RV5SVM::pipelineEmpty() const {
	return !if_id_.valid && !id_ex_.valid && !ex_mem_.valid && !mem_wb_.valid;
}

// Removed applyModeFromConfig: basic pipeline does not read dynamic config flags.

void RV5SVM::stageIF() {
	IFID out{};
	if (program_counter_ < program_size_) {
		out.instr = memory_controller_.ReadWord(program_counter_);
		out.pc = program_counter_;
		out.valid = true;
		UpdateProgramCounter(4);
	}
	// Commit fetch output into IF/ID every cycle (no control-hazard handling here)
	if_id_ = out;
}

void RV5SVM::stageID() {
	IDEX out{};
	if (!if_id_.valid) { id_ex_ = out; return; }
	const uint32_t instr = if_id_.instr;
	uint8_t opcode = instr & 0x7F;
	uint8_t funct3 = (instr >> 12) & 0x7;
	uint8_t rs1 = (instr >> 15) & 0x1F;
	uint8_t rs2 = (instr >> 20) & 0x1F;
	uint8_t rd  = (instr >> 7)  & 0x1F;
	int32_t imm = ImmGenerator(instr);

	control_.SetControlSignals(instr);

	out.valid = true;
	out.instr = instr;
	out.pc = if_id_.pc;
	out.opcode = opcode;
	out.funct3 = funct3;
	out.rs1 = rs1;
	out.rs2 = rs2;
	out.rd = rd;
	out.imm = imm;
	out.rs1_val = registers_.ReadGpr(rs1);
	out.rs2_val = registers_.ReadGpr(rs2);

	out.alu_src = control_.GetAluSrc();
	out.mem_to_reg = control_.GetMemToReg();
	out.reg_write = control_.GetRegWrite();
	out.mem_read = control_.GetMemRead();
	out.mem_write = control_.GetMemWrite();
	out.branch = control_.GetBranch();
	out.alu_signal = control_.GetAluSignal(instr, control_.GetAluOp());
	// Commit
	id_ex_ = out;
}

void RV5SVM::stageEX() {
	EXMEM out{};
	if (!id_ex_.valid) { ex_mem_ = out; return; }
	uint64_t op1 = id_ex_.rs1_val;
	uint64_t op2 = id_ex_.alu_src ? static_cast<uint64_t>(id_ex_.imm) : id_ex_.rs2_val;

	// Special cases
	uint64_t res = 0;
	bool overflow = false; (void)overflow;
	switch (id_ex_.opcode) {
		case 0b0110111: // LUI
			// ImmGenerator returns upper 20 bits (not shifted). LUI writes imm << 12.
			res = static_cast<uint64_t>(static_cast<int64_t>(id_ex_.imm) << 12);
			break;
		case 0b0010111: // AUIPC
			// AUIPC adds (imm << 12) to PC
			res = static_cast<uint64_t>(static_cast<int64_t>(id_ex_.pc) + (static_cast<int64_t>(id_ex_.imm) << 12));
			break;
		case 0b1101111: // JAL
			res = static_cast<uint64_t>(id_ex_.pc + 4); // return address
			break;
		case 0b1100111: // JALR
			res = static_cast<uint64_t>(id_ex_.pc + 4); // return address
			break;
		default: {
			auto [r, of] = alu_.execute(id_ex_.alu_signal, op1, op2);
			res = static_cast<uint64_t>(r);
			overflow = of;
			break;
		}
	}

	// Branch decision (simple, no prediction) for BEQ/BNE/BLT/BGE/...
		bool take = false;
		if (id_ex_.opcode == 0b1101111) { // JAL
			take = true;
		} else if (id_ex_.opcode == 0b1100111) { // JALR
			take = true;
		} else if (id_ex_.branch) {
		switch (id_ex_.funct3) {
			case 0b000: take = (res == 0); break; // BEQ: rs1-rs2==0
			case 0b001: take = (res != 0); break; // BNE
			case 0b100: take = (res != 0); break; // BLT: kSlt -> res!=0 means rs1<rs2
			case 0b101: take = (res == 0); break; // BGE: kSlt -> res==0 means rs1>=rs2
			case 0b110: take = (res != 0); break; // BLTU
			case 0b111: take = (res == 0); break; // BGEU
			default: break;
		}
	}
	out.valid = true;
	out.instr = id_ex_.instr;
	out.pc = id_ex_.pc;
	out.opcode = id_ex_.opcode;
	out.funct3 = id_ex_.funct3;
	out.rd = id_ex_.rd;
	out.rs2_val = id_ex_.rs2_val;
	out.mem_to_reg = id_ex_.mem_to_reg;
	out.reg_write = id_ex_.reg_write;
	out.mem_read = id_ex_.mem_read;
	out.mem_write = id_ex_.mem_write;
	out.alu_result = res;
	out.branch_taken = take;
	if (id_ex_.opcode == 0b1100111) { // JALR
		uint64_t target = (id_ex_.rs1_val + static_cast<uint64_t>(id_ex_.imm)) & ~static_cast<uint64_t>(1);
		out.branch_target = target;
	} else {
		out.branch_target = static_cast<uint64_t>(id_ex_.pc + id_ex_.imm);
	}
	// Redirect PC immediately on taken branch/jump; do not flush earlier stages.
	if (out.branch_taken) {
		program_counter_ = out.branch_target;
	}
	ex_mem_ = out;
}

void RV5SVM::stageMEM() {
    MEMWB out{};
    if (!ex_mem_.valid) { mem_wb_ = out; return; }

	uint64_t mem_data = 0;
	if (ex_mem_.mem_read) {
		switch (ex_mem_.funct3) {
	    case 0b000: mem_data = static_cast<int8_t>(memory_controller_.ReadByte(ex_mem_.alu_result)); break; // LB
	    case 0b001: mem_data = static_cast<int16_t>(memory_controller_.ReadHalfWord(ex_mem_.alu_result)); break; // LH
	    case 0b010: mem_data = static_cast<int32_t>(memory_controller_.ReadWord(ex_mem_.alu_result)); break; // LW
	    case 0b011: mem_data = memory_controller_.ReadDoubleWord(ex_mem_.alu_result); break; // LD
	    case 0b100: mem_data = memory_controller_.ReadByte(ex_mem_.alu_result); break; // LBU
	    case 0b101: mem_data = memory_controller_.ReadHalfWord(ex_mem_.alu_result); break; // LHU
	    case 0b110: mem_data = memory_controller_.ReadWord(ex_mem_.alu_result); break; // LWU
			default: break;
		}
	}
	if (ex_mem_.mem_write) {
		std::vector<uint8_t> old_bytes,new_bytes;
		size_t size=0;
		switch (ex_mem_.funct3) {
	    case 0b000: size=1; break; // SB
	    case 0b001: size=2; break; // SH
	    case 0b010: size=4; break; // SW
	    case 0b011: size=8; break; // SD
			default: break;
		}
		for (size_t i=0;i<size;++i) 
			old_bytes.push_back(memory_controller_.ReadByte(ex_mem_.alu_result + i));
		
		switch (ex_mem_.funct3) {
		    case 0b000: memory_controller_.WriteByte(ex_mem_.alu_result, static_cast<uint8_t>(ex_mem_.rs2_val)); break; // SB
	    	case 0b001: memory_controller_.WriteHalfWord(ex_mem_.alu_result, static_cast<uint16_t>(ex_mem_.rs2_val)); break; // SH
	    	case 0b010: memory_controller_.WriteWord(ex_mem_.alu_result, static_cast<uint32_t>(ex_mem_.rs2_val)); break; // SW
	    	case 0b011: memory_controller_.WriteDoubleWord(ex_mem_.alu_result, ex_mem_.rs2_val); break; // SD
			default: break;
		}
		for (size_t i=0;i<size;++i) 
			new_bytes.push_back(memory_controller_.ReadByte(ex_mem_.alu_result + i));
		if(size>0 && old_bytes != new_bytes) {
			current_delta_.memory_changes.push_back({ex_mem_.alu_result, old_bytes, new_bytes});
		}

	}

    out.valid = true;
    out.instr = ex_mem_.instr;
    out.rd = ex_mem_.rd;
    out.mem_to_reg = ex_mem_.mem_to_reg;
    out.reg_write = ex_mem_.reg_write;
    out.alu_result = ex_mem_.alu_result;
    out.mem_data = mem_data;

	// No control-hazard handling in basic pipeline; branch effects are ignored here.
	mem_wb_ = out;
}

void RV5SVM::stageWB() {
	if (!mem_wb_.valid) return;
	if (mem_wb_.reg_write && mem_wb_.rd != 0) {
		uint64_t old_val = registers_.ReadGpr(mem_wb_.rd);
		uint64_t value = mem_wb_.mem_to_reg ? mem_wb_.mem_data : mem_wb_.alu_result;
		registers_.WriteGpr(mem_wb_.rd, value);
		current_delta_.register_changes.push_back({mem_wb_.rd, 0, old_val, value});
		std::cout << "WB: x" << mem_wb_.rd << " old=" << old_val << " new=" << value << std::endl;
	}
	// Always log register changes for WB, even if the same register is written in consecutive cycles
	// Count any non-bubble WB as a retired instruction
	instructions_retired_++;
}
void RV5SVM::Step() {
	current_delta_.old_pc = program_counter_;
	current_delta_.ifid = if_id_;
	current_delta_.idex = id_ex_;
	current_delta_.exmem = ex_mem_;
	current_delta_.memwb = mem_wb_;
	current_delta_.cycle_s = cycle_s_;
	current_delta_.instructions_retired = instructions_retired_;
	current_delta_.register_changes.clear();
	current_delta_.memory_changes.clear();
	// One cycle: propagate from back to front to avoid persistent next-state members
	// 1) WB uses current MEM/WB
	stageWB();
	stageMEM();
	stageEX();
	stageID();
	stageIF();

	// Debug print: show all register changes logged for this cycle
	if (!current_delta_.register_changes.empty()) {
		//std::cout << "Step cycle=" << cycle_s_ << " Register changes: ";
		for (const auto& change : current_delta_.register_changes) {
			//std::cout << "x" << change.reg_index << "(" << change.old_value << "->" << change.new_value << ") ";
		}
		//std::cout << std::endl;
	}

	cycle_s_++;

	current_delta_.new_pc = program_counter_;

	// After all changes, push to undo stack and clear
	undo_stack_.push(current_delta_);
	while (redo_stack_.size() > 0) redo_stack_.pop();
	current_delta_ = StepDelta5();
}

void RV5SVM::Run() {
	stop_requested_ = false;
	while (!stop_requested_) {
		if (program_counter_ >= program_size_ && pipelineEmpty()) break;
		Step();
	std::cout << "Program Counter: " << program_counter_ << std::endl;
	}
	std::cout << "VM_PROGRAM_END" << std::endl;
	output_status_ = "VM_PROGRAM_END";
	// Compute CPI/IPCs for proof of pipelining
	if (instructions_retired_ > 0) {
		cpi_ = static_cast<float>(cycle_s_) / static_cast<float>(instructions_retired_);
		ipc_ = static_cast<float>(instructions_retired_) / static_cast<float>(cycle_s_ == 0 ? 1 : cycle_s_);
	}
	std::cout << "VM_STATS cycles=" << cycle_s_
			  << " retired=" << instructions_retired_
			  << " cpi=" << cpi_
			  << " ipc=" << ipc_ << std::endl;
	// Assume ideal 5x higher clock for 5-stage pipeline: period_units = 1
	unsigned int period_units = 1; // relative time unit for pipeline
	unsigned long long time_units = static_cast<unsigned long long>(cycle_s_) * period_units;
	std::cout << "VM_TIME time_units=" << time_units << " period_units=" << period_units << std::endl;
	DumpRegisters(globals::registers_dump_file_path, registers_);
	DumpState(globals::vm_state_dump_file_path);
}

void RV5SVM::DebugRun() {
	Run();
}

void RV5SVM::Undo() {
    if (undo_stack_.empty()) {
        std::cout << "VM_NO_MORE_UNDO" << std::endl;
        return;
    }
    StepDelta5 last = undo_stack_.top();
    undo_stack_.pop();

    // Restore pipeline registers and global state
    if_id_ = last.ifid;
    id_ex_ = last.idex;
    ex_mem_ = last.exmem;
    mem_wb_ = last.memwb;
    program_counter_ = last.old_pc;
    cycle_s_ = last.cycle_s;
    instructions_retired_ = last.instructions_retired;

	// Restore registers
	for (const auto& change : last.register_changes) {
		if (change.reg_type == 0) {
			//std::cout << "UNDO: x" << change.reg_index << " restore=" << change.old_value << std::endl;
			registers_.WriteGpr(change.reg_index, change.old_value);
		}
		// Add CSR/FPR if needed
	}
    // Restore memory
    for (const auto& change : last.memory_changes) {
        for (size_t i = 0; i < change.old_bytes_vec.size(); ++i)
            memory_controller_.WriteByte(change.address + i, change.old_bytes_vec[i]);
    }

    redo_stack_.push(last);
    std::cout << "VM_UNDO_COMPLETED" << std::endl;
}

void RV5SVM::Redo() {
    if (redo_stack_.empty()) {
        std::cout << "VM_NO_MORE_REDO" << std::endl;
        return;
    }
    StepDelta5 next = redo_stack_.top();
    redo_stack_.pop();

    // Restore pipeline registers and global state
    if_id_ = next.ifid;
    id_ex_ = next.idex;
    ex_mem_ = next.exmem;
    mem_wb_ = next.memwb;
    program_counter_ = next.new_pc;
    cycle_s_ = next.cycle_s;
    instructions_retired_ = next.instructions_retired;

    // Restore registers
    for (const auto& change : next.register_changes) {
        if (change.reg_type == 0)
            registers_.WriteGpr(change.reg_index, change.new_value);
        // Add CSR/FPR if needed
    }
    // Restore memory
    for (const auto& change : next.memory_changes) {
        for (size_t i = 0; i < change.new_bytes_vec.size(); ++i)
            memory_controller_.WriteByte(change.address + i, change.new_bytes_vec[i]);
    }

    undo_stack_.push(next);
    std::cout << "VM_REDO_COMPLETED" << std::endl;
}

