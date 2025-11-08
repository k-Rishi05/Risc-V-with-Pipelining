#include "vm/rv5s/predictors/one_bit_btb.h"
#include <cstdint>
#include <ostream>

PredictResult OneBitBTB::predict(uint64_t pc, uint32_t /*instr*/) {
  PredictResult r; // defaults to invalid
  size_t idx = index(pc);
  const auto &e = table_[idx];
  if (e.valid && e.tag == tag(pc)) {
    r.valid = true;
    r.taken = e.bit;
    r.target = e.target;
  } else {
    r.valid = true; // we still provide a prediction (default not taken)
    r.taken = false;
    r.target = pc + 4;
  }
  return r;
}

void OneBitBTB::update(uint64_t pc, bool is_branch, bool taken, uint64_t target) {
  if (!is_branch) return;
  size_t idx = index(pc);
  auto &e = table_[idx];
  e.valid = true;
  e.tag = tag(pc);
  e.target = target;
  e.bit = taken; // 1-bit: next prediction = last outcome
}

void OneBitBTB::reset() {
  for (auto &e : table_) {
    e.valid = false;
    e.tag = 0;
    e.target = 0;
    e.bit = false;
  }
}

void OneBitBTB::debugDump(std::ostream& os) const {
  os << "BTB[entries=" << static_cast<unsigned>(size_) << "]\n";
  for (size_t i = 0; i < size_; ++i) {
    const auto &e = table_[i];
    if (!e.valid) continue;
    os << "  [" << i << "] tag=0x" << std::hex << e.tag
       << " target=0x" << e.target
       << std::dec << " bit=" << (e.bit ? 1 : 0) << "\n";
  }
}
