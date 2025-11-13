#pragma once

#include "vm/rv5s/predictors/bht.h"
#include <vector>
#include <cstdint>
#include <ostream>
class OneBitBHT : public BHT {
public:
  explicit OneBitBHT(size_t entries = 128);
  
  bool predict(uint64_t pc) override;
  void update(uint64_t pc, bool taken) override;
  void reset() override;
  void debugDump(std::ostream& os) const override;
  
private:
  size_t size_;
  std::vector<bool> table_; // 1-bit per entry
  
  static size_t normalize(size_t n);
  inline size_t index(uint64_t pc) const { return (pc >> 2) & (size_ - 1); }
};
