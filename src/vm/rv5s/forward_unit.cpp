#include "vm/rv5s/forward_unit.h"
#include "vm/rv5s/rv5s_vm.h" // for IDEX/EXMEM/MEMWB definitions

static inline bool writes_reg(bool reg_write, uint8_t rd) {
    return reg_write && rd != 0;
}

ForwardDecision ForwardUnit::Compute(const IDEX& id_ex,
                                     const EXMEM& ex_mem,
                                     const MEMWB& mem_wb) const {
    ForwardDecision d{};

    // Operand A (rs1)
    if (writes_reg(ex_mem.reg_write, ex_mem.rd) && ex_mem.rd == id_ex.rs1 && !ex_mem.mem_read) {
        // EX hazard: forward from EX/MEM if it is an ALU result (not a load)
        d.selA = ForwardSel::EX;
    } else if (writes_reg(mem_wb.reg_write, mem_wb.rd) && mem_wb.rd == id_ex.rs1) {
        // MEM hazard: forward from MEM/WB if EX/MEM did not claim it
        d.selA = ForwardSel::MEM;
    }

    // Operand B (rs2) used by ALU only when alu_src == 0, but the select is still computed here
    if (writes_reg(ex_mem.reg_write, ex_mem.rd) && ex_mem.rd == id_ex.rs2 && !ex_mem.mem_read) {
        d.selB = ForwardSel::EX;
    } else if (writes_reg(mem_wb.reg_write, mem_wb.rd) && mem_wb.rd == id_ex.rs2) {
        d.selB = ForwardSel::MEM;
    }

    // Store data forwarding (rs2 used during MEM stage for stores)
    // Priority EX/MEM over MEM/WB, same as operands; loads in EX/MEM are not ready.
    if (writes_reg(ex_mem.reg_write, ex_mem.rd) && ex_mem.rd == id_ex.rs2 && !ex_mem.mem_read) {
        d.storeSel = ForwardSel::EX;
    } else if (writes_reg(mem_wb.reg_write, mem_wb.rd) && mem_wb.rd == id_ex.rs2) {
        d.storeSel = ForwardSel::MEM;
    }

    return d;
}
