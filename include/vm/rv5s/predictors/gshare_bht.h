#pragma once

#include "vm/rv5s/predictors/bht.h"
#include "vm/rv5s/predictors/two_bit_bht.h" 
#include <vector>
#include <cstdint>
#include <ostream>

class GshareBHT : public BHT {
public:
  explicit GshareBHT(size_t entries, uint32_t history_length);
  
  bool predict(uint64_t pc) override;
  void update(uint64_t pc, bool taken) override;
  void reset() override;
  void debugDump(std::ostream& os) const override;

private:
  const uint32_t history_length_;  
  const size_t pht_size_;          
  const uint32_t history_mask_;   
  
  std::vector<TwoBitState> pht_;   
  uint32_t ghr_;                   

  size_t index(uint64_t pc) const {
    size_t pc_bits = (pc >> 2) & history_mask_;
    size_t ghr_bits = ghr_ & history_mask_;
    return pc_bits ^ ghr_bits;
  }
  
  void updateGHR(bool taken) {
    ghr_ = ((ghr_ << 1) | (taken ? 1 : 0)) & history_mask_;
  }
  
  bool predictFromCounter(TwoBitState state) const {
    return (state == TwoBitState::WEAKLY_TAKEN || state == TwoBitState::STRONGLY_TAKEN);
  }

  void updateCounter(TwoBitState& state, bool taken);
};
