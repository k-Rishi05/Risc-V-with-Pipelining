#include "vm/rv5s/predictors/gshare_btb.h"
#include <iostream>

GshareBTB::GshareBTB(size_t entries, uint32_t history_length) {
  // Create modular BHT and BTB components
  bht_ = std::make_unique<GshareBHT>(entries, history_length);
  btb_ = std::make_unique<DirectMappedBTB>(entries);
}

PredictResult GshareBTB::predict(uint64_t pc, uint32_t /*instr*/) {
  PredictResult r;
  
  // Query BTB for target
  auto btb_result = btb_->lookup(pc);
  
  if (btb_result.hit) {
    // BTB hit: query Gshare BHT for direction
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

void GshareBTB::update(uint64_t pc, bool is_branch, bool taken, uint64_t target) {
  if (!is_branch) return;
  
  // Update both BHT (direction) and BTB (target)
  bht_->update(pc, taken);
  btb_->update(pc, target);
}

void GshareBTB::reset() {
  bht_->reset();
  btb_->reset();
}

void GshareBTB::debugDump(std::ostream& os) const {
  os << "=== GshareBTB Predictor ===\n";
  bht_->debugDump(os);
  btb_->debugDump(os);
}
