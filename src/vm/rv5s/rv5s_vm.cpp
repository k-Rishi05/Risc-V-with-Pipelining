/**
 * @file rv5s_vm.cpp
 * @brief RV5S VM implementation (5-stage pipeline, minimal, no hazards)
 */

#include "vm/rv5s/rv5s_vm.h"

#include "utils.h"
#include "globals.h"
#include "common/instructions.h"

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
	cycle_s_ = 0; instructions_retired_ = 0; stall_cycles_ = 0; branch_mispredictions_ = 0;
	cpi_ = 0; ipc_ = 0;

	control_.Reset();
	if_id_ = {}; id_ex_ = {}; ex_mem_ = {}; mem_wb_ = {};
}

bool RV5SVM::pipelineEmpty() const {
	return !if_id_.valid && !id_ex_.valid && !ex_mem_.valid && !mem_wb_.valid;
}

void RV5SVM::stageIF(IFID &next_ifid) {
	next_ifid = {};
	if (program_counter_ < program_size_) {
		next_ifid.instr = memory_controller_.ReadWord(program_counter_);
		next_ifid.pc = program_counter_;
		next_ifid.valid = true;
		UpdateProgramCounter(4);
	}
}

void RV5SVM::stageID(const IFID &cur_ifid, IDEX &next_idex) {
	next_idex = {};
	if (!cur_ifid.valid) return;

	const uint32_t instr = cur_ifid.instr;
	uint8_t opcode = instr & 0x7F;
	uint8_t funct3 = (instr >> 12) & 0x7;
	uint8_t rs1 = (instr >> 15) & 0x1F;
	uint8_t rs2 = (instr >> 20) & 0x1F;
	uint8_t rd  = (instr >> 7)  & 0x1F;
	int32_t imm = ImmGenerator(instr);

	control_.SetControlSignals(instr);

	next_idex.valid = true;
	next_idex.instr = instr;
	next_idex.pc = cur_ifid.pc;
	next_idex.opcode = opcode;
	next_idex.funct3 = funct3;
	next_idex.rs1 = rs1;
	next_idex.rs2 = rs2;
	next_idex.rd = rd;
	next_idex.imm = imm;
	next_idex.rs1_val = registers_.ReadGpr(rs1);
	next_idex.rs2_val = registers_.ReadGpr(rs2);

	next_idex.alu_src = control_.GetAluSrc();
	next_idex.mem_to_reg = control_.GetMemToReg();
	next_idex.reg_write = control_.GetRegWrite();
	next_idex.mem_read = control_.GetMemRead();
	next_idex.mem_write = control_.GetMemWrite();
	next_idex.branch = control_.GetBranch();
	next_idex.alu_signal = control_.GetAluSignal(instr, control_.GetAluOp());
}

void RV5SVM::stageEX(const IDEX &cur_idex, EXMEM &next_exmem) {
	next_exmem = {};
	if (!cur_idex.valid) return;

	uint64_t op1 = cur_idex.rs1_val;
	uint64_t op2 = cur_idex.alu_src ? static_cast<uint64_t>(cur_idex.imm) : cur_idex.rs2_val;

	// Special cases
	uint64_t res = 0;
	bool overflow = false; (void)overflow;
	switch (cur_idex.opcode) {
		case 0b0110111: // LUI
			// ImmGenerator returns upper 20 bits (not shifted). LUI writes imm << 12.
			res = static_cast<uint64_t>(static_cast<int64_t>(cur_idex.imm) << 12);
			break;
		case 0b0010111: // AUIPC
			// AUIPC adds (imm << 12) to PC
			res = static_cast<uint64_t>(static_cast<int64_t>(cur_idex.pc) + (static_cast<int64_t>(cur_idex.imm) << 12));
			break;
		case 0b1101111: // JAL
			res = static_cast<uint64_t>(cur_idex.pc + 4); // return address
			break;
		case 0b1100111: // JALR
			res = static_cast<uint64_t>(cur_idex.pc + 4); // return address
			break;
		default: {
			auto [r, of] = alu_.execute(cur_idex.alu_signal, op1, op2);
			res = static_cast<uint64_t>(r);
			overflow = of;
			break;
		}
	}

	// Branch decision (simple, no prediction) for BEQ/BNE/BLT/BGE/...
		bool take = false;
		if (cur_idex.opcode == 0b1101111) { // JAL
			take = true;
		} else if (cur_idex.opcode == 0b1100111) { // JALR
			take = true;
		} else if (cur_idex.branch) {
		switch (cur_idex.funct3) {
			case 0b000: take = (res == 0); break; // BEQ: rs1-rs2==0
			case 0b001: take = (res != 0); break; // BNE
			case 0b100: take = (res != 0); break; // BLT: kSlt -> res!=0 means rs1<rs2
			case 0b101: take = (res == 0); break; // BGE: kSlt -> res==0 means rs1>=rs2
			case 0b110: take = (res != 0); break; // BLTU
			case 0b111: take = (res == 0); break; // BGEU
			default: break;
		}
	}

	next_exmem.valid = true;
	next_exmem.instr = cur_idex.instr;
	next_exmem.pc = cur_idex.pc;
	next_exmem.opcode = cur_idex.opcode;
	next_exmem.funct3 = cur_idex.funct3;
	next_exmem.rd = cur_idex.rd;
	next_exmem.rs2_val = cur_idex.rs2_val;
	next_exmem.mem_to_reg = cur_idex.mem_to_reg;
	next_exmem.reg_write = cur_idex.reg_write;
	next_exmem.mem_read = cur_idex.mem_read;
	next_exmem.mem_write = cur_idex.mem_write;
		next_exmem.alu_result = res;
		next_exmem.branch_taken = take;
		if (cur_idex.opcode == 0b1100111) { // JALR
			uint64_t target = (cur_idex.rs1_val + static_cast<uint64_t>(cur_idex.imm)) & ~static_cast<uint64_t>(1);
			next_exmem.branch_target = target;
		} else {
			next_exmem.branch_target = static_cast<uint64_t>(cur_idex.pc + cur_idex.imm);
		}
}

void RV5SVM::stageMEM(const EXMEM &cur_exmem, MEMWB &next_memwb) {
	next_memwb = {};
	if (!cur_exmem.valid) return;

	uint64_t mem_data = 0;
	if (cur_exmem.mem_read) {
		switch (cur_exmem.funct3) {
			case 0b000: mem_data = static_cast<int8_t>(memory_controller_.ReadByte(cur_exmem.alu_result)); break; // LB
			case 0b001: mem_data = static_cast<int16_t>(memory_controller_.ReadHalfWord(cur_exmem.alu_result)); break; // LH
			case 0b010: mem_data = static_cast<int32_t>(memory_controller_.ReadWord(cur_exmem.alu_result)); break; // LW
			case 0b011: mem_data = memory_controller_.ReadDoubleWord(cur_exmem.alu_result); break; // LD
			case 0b100: mem_data = memory_controller_.ReadByte(cur_exmem.alu_result); break; // LBU
			case 0b101: mem_data = memory_controller_.ReadHalfWord(cur_exmem.alu_result); break; // LHU
			case 0b110: mem_data = memory_controller_.ReadWord(cur_exmem.alu_result); break; // LWU
			default: break;
		}
	}
	if (cur_exmem.mem_write) {
		switch (cur_exmem.funct3) {
			case 0b000: memory_controller_.WriteByte(cur_exmem.alu_result, static_cast<uint8_t>(cur_exmem.rs2_val)); break; // SB
			case 0b001: memory_controller_.WriteHalfWord(cur_exmem.alu_result, static_cast<uint16_t>(cur_exmem.rs2_val)); break; // SH
			case 0b010: memory_controller_.WriteWord(cur_exmem.alu_result, static_cast<uint32_t>(cur_exmem.rs2_val)); break; // SW
			case 0b011: memory_controller_.WriteDoubleWord(cur_exmem.alu_result, cur_exmem.rs2_val); break; // SD
			default: break;
		}
	}

	next_memwb.valid = true;
	next_memwb.instr = cur_exmem.instr;
	next_memwb.rd = cur_exmem.rd;
	next_memwb.mem_to_reg = cur_exmem.mem_to_reg;
	next_memwb.reg_write = cur_exmem.reg_write;
	next_memwb.alu_result = cur_exmem.alu_result;
	next_memwb.mem_data = mem_data;

	// simple control hazard handling: if branch taken, flush IF/ID
	if (cur_exmem.branch_taken) {
		program_counter_ = cur_exmem.branch_target;
		// flush IF stage in the next cycle by not propagating a valid IFID
		if_id_ = {};
	}
}

void RV5SVM::stageWB(const MEMWB &cur_memwb) {
	if (!cur_memwb.valid) return;
	if (cur_memwb.reg_write && cur_memwb.rd != 0) {
		uint64_t value = cur_memwb.mem_to_reg ? cur_memwb.mem_data : cur_memwb.alu_result;
		registers_.WriteGpr(cur_memwb.rd, value);
	}
	// Count any non-bubble WB as a retired instruction
	instructions_retired_++;
}

void RV5SVM::Step() {
	// One cycle: compute next pipeline regs from current, then commit
	MEMWB n_memwb{}; EXMEM n_exmem{}; IDEX n_idex{}; IFID n_ifid{};

	stageWB(mem_wb_);
	stageMEM(ex_mem_, n_memwb);
	stageEX(id_ex_, n_exmem);
	stageID(if_id_, n_idex);
	stageIF(n_ifid);

	mem_wb_ = n_memwb;
	ex_mem_ = n_exmem;
	id_ex_ = n_idex;
	// if branch taken in MEM stage, we already flushed IF; otherwise take computed IF
	if (!if_id_.valid) {
		if_id_ = n_ifid;
	} else {
		// normal advance
		if_id_ = n_ifid;
	}

	cycle_s_++;
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
	// Minimal: no per-instruction undo for pipeline yet
	// Could be enhanced by logging register/memory changes per cycle.
}

void RV5SVM::Redo() {
	// Minimal: no redo
}

