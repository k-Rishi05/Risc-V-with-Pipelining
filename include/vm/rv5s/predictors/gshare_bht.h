#pragma once

#include "vm/rv5s/predictors/bht.h"
#include "vm/rv5s/predictors/two_bit_bht.h"  // Reuse TwoBitState enum
#include <vector>
#include <cstdint>
#include <ostream>

/**
 * @brief Gshare Branch Predictor
 * Uses global history with XOR hashing for index calculation
 * 
 * Key features:
 * - Global History Register (GHR) tracks last h branch outcomes
 * - Pattern History Table (PHT) with 2-bit saturating counters
 * - Index = (PC >> 2) XOR GHR to reduce aliasing
 * 
 * Configuration (read from config):
 * - history_length: Number of bits in GHR (1-16)
 * - pht_size: 2^history_length entries
 * - Counter type: 2-bit saturating (SN, WN, WT, ST)
 * - Initial state: Weakly Not-Taken (WN)
 */
class GshareBHT : public BHT {
public:
  explicit GshareBHT(size_t entries, uint32_t history_length);
  
  bool predict(uint64_t pc) override;
  void update(uint64_t pc, bool taken) override;
  void reset() override;
  void debugDump(std::ostream& os) const override;

private:
  // Configuration (set at construction)
  const uint32_t history_length_;  // Number of bits in GHR
  const size_t pht_size_;          // 2^history_length entries
  const uint16_t history_mask_;    // Mask for GHR bits
  
  // Pattern History Table (PHT) with 2-bit saturating counters
  std::vector<TwoBitState> pht_;
  
  // Global History Register (GHR) - stores last h branch outcomes
  uint16_t ghr_;  // Use 16-bit to store up to 16 bits of history
  
  // Helper functions
  
  /**
   * @brief Calculate PHT index using XOR hashing
   * @param pc Program counter
   * @return Index into PHT
   */
  size_t index(uint64_t pc) const {
    size_t pc_bits = (pc >> 2) & history_mask_;
    size_t ghr_bits = ghr_ & history_mask_;
    return pc_bits ^ ghr_bits;  // XOR hash
  }
  
  /**
   * @brief Update GHR with new branch outcome
   * @param taken Branch outcome (true=taken, false=not-taken)
   */
  void updateGHR(bool taken) {
    // Shift left and add new outcome at LSB
    ghr_ = ((ghr_ << 1) | (taken ? 1 : 0)) & history_mask_;
  }
  
  /**
   * @brief Get prediction from 2-bit counter
   */
  bool predictFromCounter(TwoBitState state) const {
    return (state == TwoBitState::WEAKLY_TAKEN || state == TwoBitState::STRONGLY_TAKEN);
  }
  
  /**
   * @brief Update 2-bit saturating counter
   */
  void updateCounter(TwoBitState& state, bool taken);
};
