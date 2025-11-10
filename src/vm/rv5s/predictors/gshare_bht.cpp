#include "vm/rv5s/predictors/gshare_bht.h"
#include <iostream>
#include <iomanip>
#include <bitset>
#include <cmath>

GshareBHT::GshareBHT(size_t entries, uint32_t history_length)
  : history_length_(history_length),
    pht_size_(1 << history_length),  // 2^history_length
    history_mask_((1 << history_length) - 1) {  // Mask for h bits
  
  // Initialize PHT with 2^h entries
  pht_.resize(pht_size_, TwoBitState::WEAKLY_NOT_TAKEN);
  
  // Initialize GHR to 0 (all not-taken history)
  ghr_ = 0;
}

bool GshareBHT::predict(uint64_t pc) {
  // Calculate index using XOR hash
  size_t idx = index(pc);
  
  // Get prediction from 2-bit counter
  bool prediction = predictFromCounter(pht_[idx]);
  
  // Don't update GHR speculatively - wait for actual outcome
  
  return prediction;
}

void GshareBHT::update(uint64_t pc, bool taken) {
  // Calculate index using XOR hash
  size_t idx = index(pc);
  
  // Update 2-bit saturating counter
  updateCounter(pht_[idx], taken);
  
  // Update GHR with actual branch outcome
  updateGHR(taken);
}

void GshareBHT::updateCounter(TwoBitState& state, bool taken) {
  // 2-bit saturating counter state machine (reuse logic from TwoBitBHT)
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

void GshareBHT::reset() {
  // Reset all PHT entries to weakly not-taken
  for (auto& entry : pht_) {
    entry = TwoBitState::WEAKLY_NOT_TAKEN;
  }
  
  // Clear GHR
  ghr_ = 0;
}

void GshareBHT::debugDump(std::ostream& os) const {
  os << "GshareBHT[entries=" << pht_size_ << ", history_len=" << history_length_ << "]\n";
  
  // Display GHR state with dynamic bitset size
  os << "  GHR: 0b";
  for (int i = history_length_ - 1; i >= 0; --i) {
    os << ((ghr_ >> i) & 1);
  }
  os << " (0x" << std::hex << ghr_ << std::dec << ")\n";
  
  // Count PHT state distribution
  size_t strongly_not_taken = 0;
  size_t weakly_not_taken = 0;
  size_t weakly_taken = 0;
  size_t strongly_taken = 0;
  
  for (const auto& state : pht_) {
    switch (state) {
      case TwoBitState::STRONGLY_NOT_TAKEN: ++strongly_not_taken; break;
      case TwoBitState::WEAKLY_NOT_TAKEN: ++weakly_not_taken; break;
      case TwoBitState::WEAKLY_TAKEN: ++weakly_taken; break;
      case TwoBitState::STRONGLY_TAKEN: ++strongly_taken; break;
    }
  }
  
  os << "  PHT state distribution:\n"
     << "    Strongly Not-Taken (00): " << strongly_not_taken << "\n"
     << "    Weakly Not-Taken (01):   " << weakly_not_taken << "\n"
     << "    Weakly Taken (10):       " << weakly_taken << "\n"
     << "    Strongly Taken (11):     " << strongly_taken << "\n"
     << "  Predict Taken: " << (weakly_taken + strongly_taken) << "/" << pht_size_ << "\n";
  
  // Show sample PHT entries (non-WN entries)
  os << "  Sample PHT entries (first 5 non-WN):\n";
  int shown = 0;
  for (size_t i = 0; i < pht_.size() && shown < 5; ++i) {
    if (pht_[i] != TwoBitState::WEAKLY_NOT_TAKEN) {
      os << "    PHT[" << i << "] = ";
      switch (pht_[i]) {
        case TwoBitState::STRONGLY_NOT_TAKEN: os << "SN(00)"; break;
        case TwoBitState::WEAKLY_NOT_TAKEN: os << "WN(01)"; break;
        case TwoBitState::WEAKLY_TAKEN: os << "WT(10)"; break;
        case TwoBitState::STRONGLY_TAKEN: os << "ST(11)"; break;
      }
      os << "\n";
      shown++;
    }
  }
  
  if (shown == 0) {
    os << "    (all entries are WN)\n";
  }
}
