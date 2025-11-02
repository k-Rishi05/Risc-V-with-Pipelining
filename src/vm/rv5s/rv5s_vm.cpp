/**
 * @file rv5s_vm.cpp
 * @brief RV5S VM implementation (5-stage pipeline, minimal, no hazards)
 */

#include "vm/rv5s/rv5s_vm.h"

#include "utils.h"
#include "globals.h"
#include "common/instructions.h"
#include "config.h"
#include <iomanip>
#include <sstream>

using instruction_set::Instruction;
using instruction_set::get_instr_encoding;

static inline bool is_stall_mode() {
	return vm_config::config.getPipelineMode() == vm_config::PipelineMode::PIPE_STALL;
}

static inline bool is_forward_mode() {
	return vm_config::config.getPipelineMode() == vm_config::PipelineMode::PIPE_FWD;
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
	breakpoints_.clear();  // Clear breakpoints on reset
}

bool RV5SVM::pipelineEmpty() const {
	return !if_id_.valid && !id_ex_.valid && !ex_mem_.valid && !mem_wb_.valid;
}

std::string RV5SVM::DisassembleInstruction(uint32_t instr) const {
	// Don't convert to "nop" here - let the caller decide based on is_bubble flag
	// if (instr == 0 || instr == 0x00000013) return "nop";
	
	uint8_t opcode = instr & 0x7F;
	uint8_t funct3 = (instr >> 12) & 0x7;
	uint8_t funct7 = (instr >> 25) & 0x7F;
	uint8_t rd  = (instr >> 7)  & 0x1F;
	uint8_t rs1 = (instr >> 15) & 0x1F;
	uint8_t rs2 = (instr >> 20) & 0x1F;
	int32_t imm_i = static_cast<int32_t>(instr) >> 20;
	int32_t imm_s = ((static_cast<int32_t>(instr) >> 20) & ~0x1F) | rd;
	
	std::stringstream ss;
	
	switch (opcode) {
		case 0b0110011: // R-type
			if (funct7 == 0 && funct3 == 0) ss << "add";
			else if (funct7 == 0x20 && funct3 == 0) ss << "sub";
			else if (funct7 == 0 && funct3 == 1) ss << "sll";
			else if (funct7 == 0 && funct3 == 2) ss << "slt";
			else if (funct7 == 0 && funct3 == 3) ss << "sltu";
			else if (funct7 == 0 && funct3 == 4) ss << "xor";
			else if (funct7 == 0 && funct3 == 5) ss << "srl";
			else if (funct7 == 0x20 && funct3 == 5) ss << "sra";
			else if (funct7 == 0 && funct3 == 6) ss << "or";
			else if (funct7 == 0 && funct3 == 7) ss << "and";
			else ss << "r-type";
			ss << " x" << (int)rd << ",x" << (int)rs1 << ",x" << (int)rs2;
			break;
		case 0b0010011: // I-type ALU
			if (funct3 == 0) ss << "addi";
			else if (funct3 == 4) ss << "xori";
			else if (funct3 == 6) ss << "ori";
			else if (funct3 == 7) ss << "andi";
			else if (funct3 == 1) ss << "slli";
			else if (funct3 == 5 && (funct7 == 0)) ss << "srli";
			else if (funct3 == 5 && (funct7 == 0x20)) ss << "srai";
			else if (funct3 == 2) ss << "slti";
			else if (funct3 == 3) ss << "sltiu";
			else ss << "i-alu";
			ss << " x" << (int)rd << ",x" << (int)rs1 << "," << imm_i;
			break;
		case 0b0000011: // LOAD
			if (funct3 == 0) ss << "lb";
			else if (funct3 == 1) ss << "lh";
			else if (funct3 == 2) ss << "lw";
			else if (funct3 == 3) ss << "ld";
			else if (funct3 == 4) ss << "lbu";
			else if (funct3 == 5) ss << "lhu";
			else if (funct3 == 6) ss << "lwu";
			else ss << "load";
			ss << " x" << (int)rd << "," << imm_i << "(x" << (int)rs1 << ")";
			break;
		case 0b0100011: // STORE
			if (funct3 == 0) ss << "sb";
			else if (funct3 == 1) ss << "sh";
			else if (funct3 == 2) ss << "sw";
			else if (funct3 == 3) ss << "sd";
			else ss << "store";
			ss << " x" << (int)rs2 << "," << imm_s << "(x" << (int)rs1 << ")";
			break;
		case 0b1100011: // BRANCH
			if (funct3 == 0) ss << "beq";
			else if (funct3 == 1) ss << "bne";
			else if (funct3 == 4) ss << "blt";
			else if (funct3 == 5) ss << "bge";
			else if (funct3 == 6) ss << "bltu";
			else if (funct3 == 7) ss << "bgeu";
			else ss << "branch";
			ss << " x" << (int)rs1 << ",x" << (int)rs2 << ",<off>";
			break;
		case 0b0110111: // LUI
			ss << "lui x" << (int)rd << ",<imm>";
			break;
		case 0b0010111: // AUIPC
			ss << "auipc x" << (int)rd << ",<imm>";
			break;
		case 0b1101111: // JAL
			ss << "jal x" << (int)rd << ",<off>";
			break;
		case 0b1100111: // JALR
			ss << "jalr x" << (int)rd << "," << imm_i << "(x" << (int)rs1 << ")";
			break;
		default:
			ss << "unknown";
	}
	return ss.str();
}

void RV5SVM::PrintPipelineState() {
	std::cout << "┌────────────────────────────────────────────────┐" << std::endl;
	std::cout << "│ Pipeline State (Cycle " << std::setw(4) << cycle_s_ << ")                   │" << std::endl;
	std::cout << "├────────────────────────────────────────────────┤" << std::endl;
	
	auto print_stage = [](const std::string& name, const std::string& instr, bool valid, bool is_bubble) {
		std::cout << "│ " << std::setw(4) << std::left << name << " │ ";
		if (!valid) {
			// Pipeline stage not valid (empty during fill/drain)
			std::cout << std::setw(38) << "none";
		} else if (is_bubble) {
			// Valid stage but it's a stall bubble
			std::cout << std::setw(38) << "nop";
		} else {
			// Valid stage with real instruction
			std::cout << std::setw(38) << instr;
		}
		std::cout << " │" << std::endl;
	};
	
	// Show the current state of the pipeline AFTER this cycle's execution
	// IF stage: next instruction to be fetched (at current PC)
	std::string if_instr = "";
	bool if_valid = false;
	if (program_counter_ < program_size_) {
		uint32_t fetch_instr = memory_controller_.ReadWord(program_counter_);
		if_instr = DisassembleInstruction(fetch_instr);
		if_valid = true;
	}
	
	// ID stage: instruction in IF/ID register
	std::string id_instr = if_id_.valid ? DisassembleInstruction(if_id_.instr) : "";
	
	// EX stage: instruction in ID/EX register  
	std::string ex_instr = id_ex_.valid ? DisassembleInstruction(id_ex_.instr) : "";
	
	// MEM stage: instruction in EX/MEM register
	std::string mem_instr = ex_mem_.valid ? DisassembleInstruction(ex_mem_.instr) : "";
	
	// WB stage: instruction in MEM/WB register
	std::string wb_instr = mem_wb_.valid ? DisassembleInstruction(mem_wb_.instr) : "";
	
	print_stage("IF", if_instr, if_valid, false); // IF stage never has bubbles
	print_stage("ID", id_instr, if_id_.valid, if_id_.is_bubble); // ID can have bubbles from control hazards
	print_stage("EX", ex_instr, id_ex_.valid, id_ex_.is_bubble);
	print_stage("MEM", mem_instr, ex_mem_.valid, ex_mem_.is_bubble);
	print_stage("WB", wb_instr, mem_wb_.valid, mem_wb_.is_bubble);
	
	std::cout << "└────────────────────────────────────────────────┘" << std::endl;
}

// Removed applyModeFromConfig: basic pipeline does not read dynamic config flags.

void RV5SVM::stageIF() {
	IFID out{};
	
	// If flush requested, insert bubble instead of fetching
	if (flush_if_once_) {
		out.valid = true;
		out.is_bubble = true;
		out.instr = 0x00000013; // NOP
		flush_if_once_ = false;
		if_id_ = out;
		return;
	}
	
	if (program_counter_ < program_size_) {
		out.instr = memory_controller_.ReadWord(program_counter_);
		out.pc = program_counter_;
		out.valid = true;
		// If we must stall IF/ID, do not advance PC; else advance sequentially
		if (!stall_if_id_) {
			UpdateProgramCounter(4);
		}
	}
	
	// Commit IF/ID only if not stalling; otherwise freeze previous IF/ID
	if (!stall_if_id_) {
		if_id_ = out;
	}
}

void RV5SVM::stageID() {
	// Handle control hazard flush or bubble propagation
	if (flush_id_once_ || (if_id_.valid && if_id_.is_bubble)) {
		// Insert bubble into ID/EX (stageIF will handle IF/ID flush if needed)
		IDEX bubble{};
		bubble.valid = true;
		bubble.is_bubble = true;
		bubble.instr = 0x00000013; // NOP
		id_ex_ = bubble;
		flush_id_once_ = false;
		return;
	}

	// Hazard detection: compute stalls (Mode 3 and 4)
	stall_if_id_ = false;
	if (is_stall_mode() || is_forward_mode()) {
		// If we are in the middle of a stall burst, continue stalling
		if (stall_counter_ > 0) {
			stall_if_id_ = true;
			// Insert NOP bubble (valid=true, is_bubble=true)
			IDEX bubble{};
			bubble.valid = true;
			bubble.is_bubble = true;
			bubble.instr = 0x00000013; // NOP instruction (addi x0, x0, 0)
			id_ex_ = bubble;
			stall_counter_--;
			return;
		}
		// Fresh computation from current pipeline state
		// Use OLD pipeline state (before stages execute) to check hazards
		HazardDecision h = hazard_.Compute(if_id_, current_delta_.idex, current_delta_.exmem, current_delta_.memwb, /*forwarding_enabled=*/is_forward_mode());
		if (h.stall_cycles > 0) {
			stall_counter_ = h.stall_cycles - 1; // we consume one stall this cycle
			stall_if_id_ = true;
			// Insert NOP bubble (valid=true, is_bubble=true)
			IDEX bubble{};
			bubble.valid = true;
			bubble.is_bubble = true;
			bubble.instr = 0x00000013; // NOP instruction (addi x0, x0, 0)
			id_ex_ = bubble;
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

	// Default operands from register file; optionally override via forwarding in PIPE_FWD
	uint64_t srcA = id_ex_.rs1_val;
	uint64_t srcB_reg = id_ex_.rs2_val;
	uint64_t store_data = id_ex_.rs2_val;

	if (is_forward_mode()) {
		// Use OLD pipeline state for forwarding decision (before stages execute)
		const auto fwd = forward_.Compute(id_ex_, current_delta_.exmem, current_delta_.memwb);

		// Resolve operand A (rs1)
		switch (fwd.selA) {
			case ForwardSel::EX:
				srcA = current_delta_.exmem.alu_result; // Forward from OLD EX/MEM
				break;
			case ForwardSel::MEM:
				srcA = current_delta_.memwb.mem_to_reg ? current_delta_.memwb.mem_data : current_delta_.memwb.alu_result;
				break;
			case ForwardSel::REG:
			default:
				break;
		}

		// Resolve operand B (rs2) for ALU when alu_src==0
		switch (fwd.selB) {
			case ForwardSel::EX:
				srcB_reg = current_delta_.exmem.alu_result;
				break;
			case ForwardSel::MEM:
				srcB_reg = current_delta_.memwb.mem_to_reg ? current_delta_.memwb.mem_data : current_delta_.memwb.alu_result;
				break;
			case ForwardSel::REG:
			default:
				break;
		}

		// Store data forwarding value (rs2), carried into EX/MEM.rs2_val
		switch (fwd.storeSel) {
			case ForwardSel::EX:
				store_data = current_delta_.exmem.alu_result;
				break;
			case ForwardSel::MEM:
				store_data = current_delta_.memwb.mem_to_reg ? current_delta_.memwb.mem_data : current_delta_.memwb.alu_result;
				break;
			case ForwardSel::REG:
			default:
				break;
		}
	}

	// Final ALU operands
	const uint64_t op1 = srcA;
	const uint64_t op2 = id_ex_.alu_src ? static_cast<uint64_t>(id_ex_.imm) : srcB_reg;

	// Compute result according to opcode
	uint64_t res = 0;
	bool overflow = false; (void)overflow;
	switch (id_ex_.opcode) {
		case 0b0110111: // LUI
			res = static_cast<uint64_t>(static_cast<int64_t>(id_ex_.imm) << 12);
			break;
		case 0b0010111: // AUIPC
			res = static_cast<uint64_t>(static_cast<int64_t>(id_ex_.pc) + (static_cast<int64_t>(id_ex_.imm) << 12));
			break;
		case 0b1101111: // JAL -> write return address
			res = static_cast<uint64_t>(id_ex_.pc + 4);
			break;
		case 0b1100111: // JALR -> write return address
			res = static_cast<uint64_t>(id_ex_.pc + 4);
			break;
		default: {
			auto [r, of] = alu_.execute(id_ex_.alu_signal, op1, op2);
			res = static_cast<uint64_t>(r);
			overflow = of;
			break;
		}
	}

	// Branch decision (simple)
	bool take = false;
	if (id_ex_.opcode == 0b1101111) { // JAL
		take = true;
	} else if (id_ex_.opcode == 0b1100111) { // JALR
		take = true;
	} else if (id_ex_.branch) {
		switch (id_ex_.funct3) {
			case 0b000: take = (res == 0); break; // BEQ: rs1-rs2==0
			case 0b001: take = (res != 0); break; // BNE
			case 0b100: take = (res != 0); break; // BLT via SLT
			case 0b101: take = (res == 0); break; // BGE via SLT
			case 0b110: take = (res != 0); break; // BLTU
			case 0b111: take = (res == 0); break; // BGEU
			default: break;
		}
	}

	// Fill EX/MEM
	out.valid = true;
	out.is_bubble = id_ex_.is_bubble; // Propagate bubble flag
	out.instr = id_ex_.instr;
	out.pc = id_ex_.pc;
	out.opcode = id_ex_.opcode;
	out.funct3 = id_ex_.funct3;
	out.rd = id_ex_.rd;
	out.rs2_val = store_data; // forward store data if needed
	out.mem_to_reg = id_ex_.mem_to_reg;
	out.reg_write = id_ex_.reg_write;
	out.mem_read = id_ex_.mem_read;
	out.mem_write = id_ex_.mem_write;
	out.alu_result = res;
	out.branch_taken = take;
	if (id_ex_.opcode == 0b1100111) { // JALR target uses forwarded rs1
		uint64_t target = (srcA + static_cast<uint64_t>(id_ex_.imm)) & ~static_cast<uint64_t>(1);
		out.branch_target = target;
	} else {
		out.branch_target = static_cast<uint64_t>(id_ex_.pc + id_ex_.imm);
	}

	if (out.branch_taken) {
		program_counter_ = out.branch_target;
		// Flush IF and ID stages on next cycle (instructions fetched before branch resolved are invalid)
		flush_if_once_ = true;
		flush_id_once_ = true;
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
    out.is_bubble = ex_mem_.is_bubble; // Propagate bubble flag
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
		//std::cout << "WB: x" << mem_wb_.rd << " old=" << old_val << " new=" << value << std::endl;
	}
	// Count only non-bubble instructions as retired
	if (!mem_wb_.is_bubble) {
		instructions_retired_++;
	}
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
	
	// Display pipeline state after each step
	PrintPipelineState();
}

void RV5SVM::Run() {
	stop_requested_ = false;
	
	// Print initial pipeline state (cycle 0)
	PrintPipelineState();
	
	while (!stop_requested_) {
		if (program_counter_ >= program_size_ && pipelineEmpty()) break;
		Step();
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

