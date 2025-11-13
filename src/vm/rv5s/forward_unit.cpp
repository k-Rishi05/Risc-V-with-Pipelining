#include "vm/rv5s/forward_unit.h"
#include "vm/rv5s/rv5s_vm.h" // for IDEX/EXMEM/MEMWB definitions

static inline bool writes_reg(bool reg_write, uint8_t rd) {
    return reg_write && rd != 0;
}

ForwardDecision ForwardUnit::Compute(const IDEX& id_ex,const EXMEM& ex_mem,const MEMWB& mem_wb) const {
    ForwardDecision d{};

    // rs1 forwarding
    if (writes_reg(ex_mem.reg_write, ex_mem.rd) && ex_mem.rd == id_ex.rs1 && !ex_mem.mem_read) {
        d.selA = ForwardSel::EX;
    } else if (writes_reg(mem_wb.reg_write, mem_wb.rd) && mem_wb.rd == id_ex.rs1) {
        d.selA = ForwardSel::MEM;
    }

    // rs2 forwarding
    if (writes_reg(ex_mem.reg_write, ex_mem.rd) && ex_mem.rd == id_ex.rs2 && !ex_mem.mem_read) {
        d.selB = ForwardSel::EX;
    } else if (writes_reg(mem_wb.reg_write, mem_wb.rd) && mem_wb.rd == id_ex.rs2) {
        d.selB = ForwardSel::MEM;
    }

    // Store data forwarding 
    if (writes_reg(ex_mem.reg_write, ex_mem.rd) && ex_mem.rd == id_ex.rs2 && !ex_mem.mem_read) {
        d.storeSel = ForwardSel::EX;
    } else if (writes_reg(mem_wb.reg_write, mem_wb.rd) && mem_wb.rd == id_ex.rs2) {
        d.storeSel = ForwardSel::MEM;
    }

    return d;
}
