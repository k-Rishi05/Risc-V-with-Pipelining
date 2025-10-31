/**
 * @file forward_unit.h
 * @brief Forwarding unit for RV5S pipeline (EX-stage operand and store-data forwarding).
 */
#ifndef RV5S_FORWARD_UNIT_H
#define RV5S_FORWARD_UNIT_H

#include <cstdint>

// Forward declarations of pipeline register bundles
struct IDEX;
struct EXMEM;
struct MEMWB;

// Forwarding select encoding matches common textbook convention:
// 00 -> use register file value (no forwarding)
// 01 -> forward from MEM/WB
// 10 -> forward from EX/MEM (ALU result of previous instr)
enum class ForwardSel : uint8_t {
    REG = 0,
    MEM = 1,
    EX  = 2
};

struct ForwardDecision {
    ForwardSel selA{ForwardSel::REG}; // for EX operand A (rs1)
    ForwardSel selB{ForwardSel::REG}; // for EX operand B (rs2 when alu_src==0)
    ForwardSel storeSel{ForwardSel::REG}; // for store data path (always rs2), independent of alu_src
};

class ForwardUnit {
public:
    // Compute forwarding controls based on current EX consumer (ID/EX)
    // and the two older producers (EX/MEM and MEM/WB).
    // Notes:
    // - EX/MEM forwarding is not used for loads (data not ready yet).
    // - MEM/WB forwarding is masked if a valid EX/MEM ALU result also matches.
    ForwardDecision Compute(const IDEX& id_ex,
                            const EXMEM& ex_mem,
                            const MEMWB& mem_wb) const;
};

#endif // RV5S_FORWARD_UNIT_H
