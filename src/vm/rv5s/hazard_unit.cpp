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
								   const MEMWB& mem_wb,
								   bool forwarding_enabled) const {
	HazardDecision d{};
	if (!if_id.valid) return d;

	// Data hazard detection only (control hazards handled in stageEX)
	uint8_t opcode = 0, rs1 = 0, rs2 = 0;
	decode_rs(if_id.instr, opcode, rs1, rs2);

	auto consider_dep = [&](uint8_t rd, bool writes, bool is_bubble, int stall_if_dep){
		// Don't consider dependencies from stall bubbles
		if (writes && rd != 0 && !is_bubble) {
			if ((id_uses_rs1(opcode) && rs1 == rd) || (id_uses_rs2(opcode) && rs2 == rd)) {
				d.stall_cycles = std::max(d.stall_cycles, stall_if_dep);
			}
		}
	};

	if (!forwarding_enabled) {
		// Stall-only mode (Mode 3): conservative stalls on RAW against EX/MEM producers
		consider_dep(id_ex.rd, id_ex.valid && id_ex.reg_write, id_ex.is_bubble, 2); // EX -> 2 stalls
		consider_dep(ex_mem.rd, ex_mem.valid && ex_mem.reg_write, ex_mem.is_bubble, 1); // MEM -> 1 stall
		// No stall for WB (0-cycle)
	} else {
		// Forwarding mode (Mode 4): only unavoidable load-use stall
		// If ID depends on a load currently in EX (id_ex.mem_read), stall one cycle.
		if (id_ex.valid && id_ex.mem_read && id_ex.rd != 0 && !id_ex.is_bubble) {
			const bool dep_rs1 = id_uses_rs1(opcode) && (rs1 == id_ex.rd);
			const bool dep_rs2 = id_uses_rs2(opcode) && (rs2 == id_ex.rd);
			if (dep_rs1 || dep_rs2) {
				d.stall_cycles = std::max(d.stall_cycles, 1);
			}
		}
		// No stalls for EX/MEM ALU producers; forwarding resolves those.
	}

	return d;
}

