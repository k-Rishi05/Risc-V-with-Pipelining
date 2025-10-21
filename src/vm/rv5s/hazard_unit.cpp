#include "vm/rv5s/hazard_unit.h"
#include "vm/rv5s/rv5s_vm.h" // for IFID/IDEX definitions

// Load-use hazard if: id_ex is a load and if_id needs its rd as rs1/rs2
bool HazardUnit::ShouldStall(const IFID& if_id, const IDEX& id_ex) const {
    if (!if_id.valid || !id_ex.valid) return false;
    // id_ex.mem_read is true for loads; id_ex.rd is the destination
    if (id_ex.mem_read && id_ex.rd != 0) {
        uint32_t instr = if_id.instr;
        uint8_t opcode = instr & 0x7F;
        uint8_t rs1 = (instr >> 15) & 0x1F;
        uint8_t rs2 = (instr >> 20) & 0x1F;

        bool uses_rs1 = false, uses_rs2 = false;
        switch (opcode) {
            // R-type uses rs1 and rs2
            case 0b0110011: uses_rs1 = true; uses_rs2 = true; break;
            // I-type ALU/JALR/loads use rs1
            case 0b0010011: // I-ALU
            case 0b0000011: // LOAD
            case 0b1100111: // JALR
                uses_rs1 = true; break;
            // S-type stores use rs2 as store data and rs1 as base
            case 0b0100011: uses_rs1 = true; uses_rs2 = true; break; // STORE
            // B-type branches use rs1 and rs2
            case 0b1100011: uses_rs1 = true; uses_rs2 = true; break; // BRANCH
            default: break;
        }

        if ((uses_rs1 && rs1 == id_ex.rd) || (uses_rs2 && rs2 == id_ex.rd)) {
            return true; // need a stall
        }
    }
    return false;
}
