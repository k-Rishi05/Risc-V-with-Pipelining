#include "vm/rv5s/predictors/two_bit_btb.h"
#include <iostream>

TwoBitBTB::TwoBitBTB(size_t entries) {
  bht_ = std::make_unique<TwoBitBHT>(entries);
  btb_ = std::make_unique<DirectMappedBTB>(entries);
}

PredictResult TwoBitBTB::predict(uint64_t pc, uint32_t) {
  PredictResult r;
  auto btb_result = btb_->lookup(pc);
  
  if (btb_result.hit) {
    r.valid = true;
    r.taken = bht_->predict(pc);
    r.target = btb_result.target;
  } else {
    r.valid = true;
    r.taken = false;
    r.target = pc + 4;
  }
  
  return r;
}

void TwoBitBTB::update(uint64_t pc, bool is_branch, bool taken, uint64_t target) {
  if (!is_branch) return;
  bht_->update(pc, taken);
  btb_->update(pc, target);
}

void TwoBitBTB::reset() {
  bht_->reset();
  btb_->reset();
}

void TwoBitBTB::debugDump(std::ostream& os) const {
  os << "=== TwoBitBTB Predictor ===\n";
  bht_->debugDump(os);
  btb_->debugDump(os);
}
