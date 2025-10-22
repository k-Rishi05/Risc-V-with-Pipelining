/**
 * @file hazard_unit.h
 * @brief Stall-only hazard detection for RV5S pipeline.
 */
#ifndef RV5S_HAZARD_UNIT_H
#define RV5S_HAZARD_UNIT_H

#include <cstdint>

struct IFID;
struct IDEX;
struct EXMEM;
struct MEMWB;

struct HazardDecision {
	int stall_cycles{0}; // number of cycles to stall starting now
	bool flush_if{false}; // flush the current IF fetch (discard IF/ID update once)
};

class HazardUnit {
public:
	// Compute data and control hazards for stall-only handling.
	HazardDecision Compute(const IFID& if_id,
						   const IDEX& id_ex,
						   const EXMEM& ex_mem,
						   const MEMWB& mem_wb) const;
};

#endif // RV5S_HAZARD_UNIT_H

