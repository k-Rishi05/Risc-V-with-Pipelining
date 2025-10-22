#include "vm/rv5s/hazard_unit.h"
#include "vm/rv5s/rv5s_vm.h" // for IFID/IDEX/EXMEM/MEMWB definitions

static inline void decode_rs(uint32_t instr, uint8_t& opcode, uint8_t& rs1, uint8_t& rs2) {
	opcode = instr & 0x7F;
	rs1 = (instr >> 15) & 0x1F;
	rs2 = (instr >> 20) & 0x1F;
}

static inline bool id_uses_rs1(uint8_t opcode) {
	switch (opcode) {
		case 0b0110011: // R
		case 0b0010011: // I-ALU
		case 0b0000011: // LOAD
		case 0b0100011: // STORE
		case 0b1100011: // BRANCH
		case 0b1100111: // JALR
			return true;
		default: return false;
	}
}
static inline bool id_uses_rs2(uint8_t opcode) {
	switch (opcode) {
		case 0b0110011: // R
		case 0b0100011: // STORE
		case 0b1100011: // BRANCH
			return true;
		default: return false;
	}
}

HazardDecision HazardUnit::Compute(const IFID& if_id,
								   const IDEX& id_ex,
								   const EXMEM& ex_mem,
								   const MEMWB& mem_wb) const {
	HazardDecision d{};
	if (!if_id.valid) return d;

	// Control hazard: branch/jump in ID -> flush IF and 1 stall
	uint8_t opcode = 0, rs1 = 0, rs2 = 0;
	decode_rs(if_id.instr, opcode, rs1, rs2);
	const bool is_branch = (opcode == 0b1100011);
	const bool is_jump = (opcode == 0b1101111) || (opcode == 0b1100111); // JAL/JALR
	if (is_branch || is_jump) {
		d.flush_if = true; // one-shot IF flush creates the 1-cycle bubble behind the branch
		// Do NOT stall ID; the branch should advance to EX next cycle
	}

	auto consider_dep = [&](uint8_t rd, bool writes, int stall_if_dep){
		if (writes && rd != 0) {
			if ((id_uses_rs1(opcode) && rs1 == rd) || (id_uses_rs2(opcode) && rs2 == rd)) {
				d.stall_cycles = std::max(d.stall_cycles, stall_if_dep);
			}
		}
	};

	// Data hazards: ID.rs1/rs2 vs EX/MEM/WB rd
	consider_dep(id_ex.rd, id_ex.valid && id_ex.reg_write, 2); // EX -> 2 stalls
	consider_dep(ex_mem.rd, ex_mem.valid && ex_mem.reg_write, 1); // MEM -> 1 stall
	consider_dep(mem_wb.rd, mem_wb.valid && mem_wb.reg_write, 0); // WB -> 0 stalls

	return d;
}

