#ifndef RV5S_PREDICTORS_BTB_H
#define RV5S_PREDICTORS_BTB_H

#include <cstddef>
#include <cstdint>
#include <vector>
#include <iosfwd>


//Branch Target Buffer (BTB) - caches branch target addresses
class BTB {
public:
  struct LookupResult {
    bool hit{false};        // true if PC found in BTB
    uint64_t target{0};     // cached target address
  };
  
  virtual ~BTB() = default;
  virtual LookupResult lookup(uint64_t pc) = 0;
  
  // Update BTB with branch PC and its target
  virtual void update(uint64_t pc, uint64_t target) = 0;
  virtual void reset() = 0;
  virtual void debugDump(std::ostream& /*os*/) const {}
};

class DirectMappedBTB : public BTB {
public:
  explicit DirectMappedBTB(size_t entries = 128);
  
  LookupResult lookup(uint64_t pc) override;
  void update(uint64_t pc, uint64_t target) override;
  void reset() override;
  void debugDump(std::ostream& os) const override;
  
private:
  struct Entry {
    bool valid{false};
    uint64_t tag{0};
    uint64_t target{0};
  };
  
  size_t size_;
  std::vector<Entry> table_;
  
  static size_t normalize(size_t n);
  inline size_t index(uint64_t pc) const { return (pc >> 2) & (size_ - 1); }
  inline uint64_t tag(uint64_t pc) const { 
    unsigned shift = 2; 
    while ((1ull << shift) < size_) ++shift; 
    return pc >> shift; 
  }
};

#endif // RV5S_PREDICTORS_BTB_H
