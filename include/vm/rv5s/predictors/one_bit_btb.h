#ifndef RV5S_PREDICTORS_ONE_BIT_BTB_H
#define RV5S_PREDICTORS_ONE_BIT_BTB_H

#include <cstddef>
#include <cstdint>
#include <vector>
#include "vm/rv5s/predictor.h"

class OneBitBTB : public Predictor {
public:
  explicit OneBitBTB(size_t entries = 32) : size_(normalize(entries)) {
    table_.resize(size_);
    reset();
  }

  PredictResult predict(uint64_t pc, uint32_t instr) override;
  void update(uint64_t pc, bool is_branch, bool taken, uint64_t target) override;
  void reset() override;
  void debugDump(std::ostream& os) const override;

private:
  struct Entry {
    bool valid{false};
    uint64_t tag{0};
    uint64_t target{0};
    bool bit{false}; // last outcome
  };

  size_t size_;
  std::vector<Entry> table_;

  static size_t normalize(size_t n) {
    // force to power of two >= 8
    size_t p = 8; while (p < n) p <<= 1; return p;
  }
  inline size_t index(uint64_t pc) const { return (pc >> 2) & (size_ - 1); }
  inline uint64_t tag(uint64_t pc) const { unsigned shift = 2; while ((1ull << shift) < size_) ++shift; return pc >> shift; }
};

#endif // RV5S_PREDICTORS_ONE_BIT_BTB_H
