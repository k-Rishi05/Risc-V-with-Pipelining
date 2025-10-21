/**
 * @file hazard_unit.h
 * @brief Minimal data hazard detection (stall-only) for RV5S pipeline.
 */
#ifndef RV5S_HAZARD_UNIT_H
#define RV5S_HAZARD_UNIT_H

#include <cstdint>

struct IFID;
struct IDEX;

class HazardUnit {
public:
    // Returns true if we must stall due to a load-use hazard between ID and EX
    bool ShouldStall(const IFID& if_id, const IDEX& id_ex) const;
};

#endif // RV5S_HAZARD_UNIT_H
