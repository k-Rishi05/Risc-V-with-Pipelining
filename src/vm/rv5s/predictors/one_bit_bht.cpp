#include "vm/rv5s/predictors/one_bit_bht.h"
#include <iostream>

OneBitBHT::OneBitBHT(size_t entries) : size_(normalize(entries)) {
  table_.resize(size_, false);
}

bool OneBitBHT::predict(uint64_t pc) {
  size_t idx = index(pc);
  return table_[idx];
}

void OneBitBHT::update(uint64_t pc, bool taken) {
  size_t idx = index(pc);
  table_[idx] = taken;
}

void OneBitBHT::reset() {
  for (size_t i = 0; i < table_.size(); ++i) {
    table_[i] = false;
  }
}

void OneBitBHT::debugDump(std::ostream& os) const {
  os << "OneBitBHT[entries=" << size_ << "]\n";
  size_t taken_count = 0;
  for (size_t i = 0; i < size_; ++i) {
    if (table_[i]) ++taken_count;
  }
  os << "  Taken predictions: " << taken_count << "/" << size_ << "\n";
  
  // Print actual table contents (only taken entries for readability)
  os << "  Table contents (taken entries):\n";
  bool found_any = false;
  for (size_t i = 0; i < size_; ++i) {
    if (table_[i]) {
      found_any = true;
      os << "    [" << i << "] = 1 (taken)\n";
    }
  }
  if (!found_any) {
    os << "    (all entries predict not-taken)\n";
  }
}

size_t OneBitBHT::normalize(size_t n) {
  size_t p = 8;
  while (p < n) p <<= 1;
  return p;
}
