#pragma once

#include "vm/rv5s/predictors/bht.h"
#include <vector>
#include <cstdint>
#include <ostream>

// 2-bit saturating counter states
enum class TwoBitState : uint8_t {
  STRONGLY_NOT_TAKEN = 0b00,  // 00
  WEAKLY_NOT_TAKEN = 0b01,    // 01
  WEAKLY_TAKEN = 0b10,        // 10
  STRONGLY_TAKEN = 0b11       // 11
};

// 2-bit saturating counter Branch History Table
class TwoBitBHT : public BHT {
public:
  explicit TwoBitBHT(size_t entries);
  
  bool predict(uint64_t pc) override;
  void update(uint64_t pc, bool taken) override;
  void reset() override;
  void debugDump(std::ostream& os) const override;

private:
  std::vector<TwoBitState> table_;
  size_t size_;
  
  size_t index(uint64_t pc) const { return (pc >> 2) & (size_ - 1); }
  static size_t normalize(size_t n);
};
