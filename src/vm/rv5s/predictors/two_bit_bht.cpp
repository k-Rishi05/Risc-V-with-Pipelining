#include "vm/rv5s/predictors/two_bit_bht.h"
#include <iostream>

TwoBitBHT::TwoBitBHT(size_t entries) : size_(normalize(entries)) {
  // Initialize all to strongly not-taken (00)
  table_.resize(size_, TwoBitState::STRONGLY_NOT_TAKEN);
}

bool TwoBitBHT::predict(uint64_t pc) {
  size_t idx = index(pc);
  TwoBitState state = table_[idx];
  
  // Predict taken if state is 10 (weakly taken) or 11 (strongly taken)
  return (state == TwoBitState::WEAKLY_TAKEN || state == TwoBitState::STRONGLY_TAKEN);
}

void TwoBitBHT::update(uint64_t pc, bool taken) {
  size_t idx = index(pc);
  TwoBitState& state = table_[idx];
  
  // 2-bit saturating counter state machine
  if (taken) {
    // Move toward strongly taken
    switch (state) {
      case TwoBitState::STRONGLY_NOT_TAKEN:
        state = TwoBitState::WEAKLY_NOT_TAKEN;
        break;
      case TwoBitState::WEAKLY_NOT_TAKEN:
        state = TwoBitState::WEAKLY_TAKEN;
        break;
      case TwoBitState::WEAKLY_TAKEN:
        state = TwoBitState::STRONGLY_TAKEN;
        break;
      case TwoBitState::STRONGLY_TAKEN:
        // Already at maximum, stay
        break;
    }
  } else {
    // Move toward strongly not-taken
    switch (state) {
      case TwoBitState::STRONGLY_TAKEN:
        state = TwoBitState::WEAKLY_TAKEN;
        break;
      case TwoBitState::WEAKLY_TAKEN:
        state = TwoBitState::WEAKLY_NOT_TAKEN;
        break;
      case TwoBitState::WEAKLY_NOT_TAKEN:
        state = TwoBitState::STRONGLY_NOT_TAKEN;
        break;
      case TwoBitState::STRONGLY_NOT_TAKEN:
        // Already at minimum, stay
        break;
    }
  }
}

void TwoBitBHT::reset() {
  for (size_t i = 0; i < table_.size(); ++i) {
    table_[i] = TwoBitState::STRONGLY_NOT_TAKEN;
  }
}

void TwoBitBHT::debugDump(std::ostream& os) const {
  os << "TwoBitBHT[entries=" << size_ << "]\n";
  
  size_t strongly_not_taken = 0;
  size_t weakly_not_taken = 0;
  size_t weakly_taken = 0;
  size_t strongly_taken = 0;
  
  for (size_t i = 0; i < size_; ++i) {
    switch (table_[i]) {
      case TwoBitState::STRONGLY_NOT_TAKEN: ++strongly_not_taken; break;
      case TwoBitState::WEAKLY_NOT_TAKEN: ++weakly_not_taken; break;
      case TwoBitState::WEAKLY_TAKEN: ++weakly_taken; break;
      case TwoBitState::STRONGLY_TAKEN: ++strongly_taken; break;
    }
  }
  
//   os << "  State distribution:\n"
//      << "    Strongly Not-Taken (00): " << strongly_not_taken << "\n"
//      << "    Weakly Not-Taken (01):   " << weakly_not_taken << "\n"
//      << "    Weakly Taken (10):       " << weakly_taken << "\n"
//      << "    Strongly Taken (11):     " << strongly_taken << "\n"
//      << "  Predict Taken: " << (weakly_taken + strongly_taken) << "/" << size_ << "\n";
  
  // Print actual table contents (only non-default entries for readability)
  os << "  Table contents (non-SN entries):\n";
  bool found_any = false;
  for (size_t i = 0; i < size_; ++i) {
    if (table_[i] != TwoBitState::STRONGLY_NOT_TAKEN) {
      found_any = true;
      os << "    [" << i << "] = ";
      switch (table_[i]) {
        case TwoBitState::STRONGLY_NOT_TAKEN: os << "SN(00)"; break;
        case TwoBitState::WEAKLY_NOT_TAKEN: os << "WN(01)"; break;
        case TwoBitState::WEAKLY_TAKEN: os << "WT(10)"; break;
        case TwoBitState::STRONGLY_TAKEN: os << "ST(11)"; break;
      }
      os << "\n";
    }
  }
  if (!found_any) {
    os << "    (all entries are SN)\n";
  }
}

size_t TwoBitBHT::normalize(size_t n) {
  // Force to power of two >= 8
  size_t p = 8;
  while (p < n) p <<= 1;
  return p;
}
