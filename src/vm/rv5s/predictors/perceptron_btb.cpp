#include "vm/rv5s/predictors/perceptron_btb.h"
#include <iostream>

PerceptronBTB::PerceptronBTB(size_t entries) {
  // Create modular BHT and BTB components
  bht_ = std::make_unique<PerceptronBHT>(entries);
  btb_ = std::make_unique<DirectMappedBTB>(entries);
}

PredictResult PerceptronBTB::predict(uint64_t pc, uint32_t /*instr*/) {
  PredictResult r;
  
  // Query BTB for target
  auto btb_result = btb_->lookup(pc);
  
  if (btb_result.hit) {
    // BTB hit: query perceptron BHT for direction
    r.valid = true;
    r.taken = bht_->predict(pc);
    r.target = btb_result.target;
  } else {
    // BTB miss: provide default prediction (not-taken, sequential)
    r.valid = true;
    r.taken = false;
    r.target = pc + 4;
  }
  
  return r;
}

void PerceptronBTB::update(uint64_t pc, bool is_branch, bool taken, uint64_t target) {
  if (!is_branch) return;
  
  // Update both BHT (direction) and BTB (target)
  bht_->update(pc, taken);
  btb_->update(pc, target);
}

void PerceptronBTB::reset() {
  bht_->reset();
  btb_->reset();
}

void PerceptronBTB::debugDump(std::ostream& os) const {
  os << "=== PerceptronBTB Predictor ===\n";
  bht_->debugDump(os);
  btb_->debugDump(os);
}
