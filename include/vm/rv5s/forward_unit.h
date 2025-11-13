/**
 * @file forward_unit.h
 * @brief Forwarding unit for RV5S pipeline (EX-stage operand and store-data forwarding).
 */
#ifndef RV5S_FORWARD_UNIT_H
#define RV5S_FORWARD_UNIT_H

#include <cstdint>

struct IDEX;
struct EXMEM;
struct MEMWB;

enum class ForwardSel : uint8_t {
    REG = 0,
    MEM = 1,
    EX  = 2
};

struct ForwardDecision {
    ForwardSel selA{ForwardSel::REG};
    ForwardSel selB{ForwardSel::REG}; 
    ForwardSel storeSel{ForwardSel::REG};
};

class ForwardUnit {
public:
    ForwardDecision Compute(const IDEX& id_ex,
                            const EXMEM& ex_mem,
                            const MEMWB& mem_wb) const;
};

#endif // RV5S_FORWARD_UNIT_H
