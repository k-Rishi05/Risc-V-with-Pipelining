#include "vm/rv5s/predictors/btb.h"
#include <iostream>
#include <iomanip>

// DirectMappedBTB implementation

DirectMappedBTB::DirectMappedBTB(size_t entries) : size_(normalize(entries)) {
  table_.resize(size_);
  reset();
}

BTB::LookupResult DirectMappedBTB::lookup(uint64_t pc) {
  size_t idx = index(pc);
  const auto& e = table_[idx];
  
  LookupResult result;
  if (e.valid && e.tag == tag(pc)) {
    result.hit = true;
    result.target = e.target;
  } else {
    result.hit = false;
    result.target = 0;
  }
  return result;
}

void DirectMappedBTB::update(uint64_t pc, uint64_t target) {
  size_t idx = index(pc);
  auto& e = table_[idx];
  e.valid = true;
  e.tag = tag(pc);
  e.target = target;
}

void DirectMappedBTB::reset() {
  for (auto& e : table_) {
    e.valid = false;
    e.tag = 0;
    e.target = 0;
  }
}

void DirectMappedBTB::debugDump(std::ostream& os) const {
  os << "DirectMappedBTB[entries=" << size_ << "]\n";
  size_t valid_count = 0;
  for (size_t i = 0; i < size_; ++i) {
    const auto& e = table_[i];
    if (!e.valid) continue;
    ++valid_count;
    os << "  [" << i << "] tag=0x" << std::hex << e.tag
       << " target=0x" << e.target << std::dec << "\n";
  }
  os << "  Valid entries: " << valid_count << "/" << size_ << "\n";
}

size_t DirectMappedBTB::normalize(size_t n) {
  // Force to power of two >= 8
  size_t p = 8;
  while (p < n) p <<= 1;
  return p;
}
