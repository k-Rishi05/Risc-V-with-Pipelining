/**
 * @file rv5s_control_unit.cpp
 * @brief RV5S Control Unit implementation (minimal)
 */

#include "vm/rv5s/rv5s_control_unit.h"
#include "vm/alu.h"
#include "common/instructions.h"

using instruction_set::Instruction;
using instruction_set::get_instr_encoding;

void RV5SControlUnit::SetControlSignals(uint32_t instruction) {
	uint8_t opcode = instruction & 0b1111111;

	alu_src_ = mem_to_reg_ = reg_write_ = mem_read_ = mem_write_ = branch_ = false;
	alu_op_ = false;

	switch (opcode) {
		case 0b0110011: // R-type
			reg_write_ = true; alu_op_ = true; break;
		case 0b0000011: // Load
			alu_src_ = true; mem_to_reg_ = true; reg_write_ = true; mem_read_ = true; break;
		case 0b0100011: // Store
			alu_src_ = true; alu_op_ = true; mem_write_ = true; break;
		case 0b1100011: // Branch
			alu_op_ = true; branch_ = true; break;
		case 0b0010011: // I-type ALU
			alu_src_ = true; reg_write_ = true; alu_op_ = true; break;
		case 0b0110111: // LUI
			alu_src_ = true; reg_write_ = true; alu_op_ = true; break;
		case 0b0010111: // AUIPC
			alu_src_ = true; reg_write_ = true; alu_op_ = true; break;
		case 0b1101111: // JAL
			reg_write_ = true; branch_ = true; break;
		case 0b1100111: // JALR
			alu_src_ = true; reg_write_ = true; branch_ = true; break;
		default: break;
	}
}

alu::AluOp RV5SControlUnit::GetAluSignal(uint32_t instruction, bool ALUOp) {
	(void)ALUOp;
	uint8_t opcode = instruction & 0b1111111;
	uint8_t funct3 = (instruction >> 12) & 0b111;
	uint8_t funct7 = (instruction >> 25) & 0b1111111;

	switch (opcode) {
		case 0b0110011: // R-type
			switch (funct3) {
				case 0b000: return (funct7 == 0b0100000) ? alu::AluOp::kSub : alu::AluOp::kAdd;
				case 0b001: return alu::AluOp::kSll;
				case 0b010: return alu::AluOp::kSlt;
				case 0b011: return alu::AluOp::kSltu;
				case 0b100: return alu::AluOp::kXor;
				case 0b101: return (funct7 == 0b0100000) ? alu::AluOp::kSra : alu::AluOp::kSrl;
				case 0b110: return alu::AluOp::kOr;
				case 0b111: return alu::AluOp::kAnd;
			}
			break;
		case 0b0010011: // I-type ALU
			switch (funct3) {
				case 0b000: return alu::AluOp::kAdd; // ADDI
				case 0b001: return alu::AluOp::kSll; // SLLI
				case 0b010: return alu::AluOp::kSlt; // SLTI
				case 0b011: return alu::AluOp::kSltu; // SLTIU
				case 0b100: return alu::AluOp::kXor; // XORI
				case 0b101: return (funct7 == 0b0100000) ? alu::AluOp::kSra : alu::AluOp::kSrl; // SRAI/SRLI
				case 0b110: return alu::AluOp::kOr; // ORI
				case 0b111: return alu::AluOp::kAnd; // ANDI
			}
			break;
		case 0b1100011: // Branch
			switch (funct3) {
				case 0b000: // BEQ
				case 0b001: // BNE
					return alu::AluOp::kSub;
				case 0b100: // BLT
				case 0b101: // BGE
					return alu::AluOp::kSlt;
				case 0b110: // BLTU
				case 0b111: // BGEU
					return alu::AluOp::kSltu;
			}
			break;
		case 0b0000011: // Load
		case 0b0100011: // Store
		case 0b1100111: // JALR
		case 0b1101111: // JAL
		case 0b0110111: // LUI
		case 0b0010111: // AUIPC
			return alu::AluOp::kAdd;
		default: break;
	}
	return alu::AluOp::kNone;
}

