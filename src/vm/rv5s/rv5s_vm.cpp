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

static inline bool is_stall_mode() {
	return vm_config::config.getPipelineMode() == vm_config::PipelineMode::PIPE_STALL;
}

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
		// If we must stall IF/ID, do not advance PC; else advance sequentially
		if (!stall_if_id_) {
			UpdateProgramCounter(4);
		}
	}
	// Apply one-cycle IF flush if requested (control hazard from ID)
	if (flush_if_once_) {
		out = {}; // discard fetched instruction this cycle
		flush_if_once_ = false;
	}
	// Commit IF/ID only if not stalling; otherwise freeze previous IF/ID
	if (!stall_if_id_) {
		if_id_ = out;
	}
}

void RV5SVM::stageID() {
	// Hazard detection (stall-only): compute stalls and IF flush request
	stall_if_id_ = false;
	if (is_stall_mode()) {
		// If we are in the middle of a stall burst, continue stalling
		if (stall_counter_ > 0) {
			stall_if_id_ = true;
			id_ex_ = {}; // bubble
			stall_counter_--;
			return;
		}
		// Fresh computation from current pipeline state
		HazardDecision h = hazard_.Compute(if_id_, id_ex_, ex_mem_, mem_wb_);
		if (h.flush_if) {
			flush_if_once_ = true;
		}
		if (h.stall_cycles > 0) {
			stall_counter_ = h.stall_cycles - 1; // we consume one stall this cycle
			stall_if_id_ = true;
			id_ex_ = {}; // bubble
			return;
		}
	}

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
		switch (ex_mem_.funct3) {
	    case 0b000: memory_controller_.WriteByte(ex_mem_.alu_result, static_cast<uint8_t>(ex_mem_.rs2_val)); break; // SB
	    case 0b001: memory_controller_.WriteHalfWord(ex_mem_.alu_result, static_cast<uint16_t>(ex_mem_.rs2_val)); break; // SH
	    case 0b010: memory_controller_.WriteWord(ex_mem_.alu_result, static_cast<uint32_t>(ex_mem_.rs2_val)); break; // SW
	    case 0b011: memory_controller_.WriteDoubleWord(ex_mem_.alu_result, ex_mem_.rs2_val); break; // SD
			default: break;
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
		uint64_t value = mem_wb_.mem_to_reg ? mem_wb_.mem_data : mem_wb_.alu_result;
		registers_.WriteGpr(mem_wb_.rd, value);
	}
	// Count any non-bubble WB as a retired instruction
	instructions_retired_++;
}

void RV5SVM::Step() {
	// One cycle: propagate from back to front to avoid persistent next-state members
	// 1) WB uses current MEM/WB
	stageWB();
	stageMEM();
	stageEX();
	stageID();
	stageIF();

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
        if (change.reg_type == 0)
            registers_.WriteGpr(change.reg_index, change.old_value);
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

