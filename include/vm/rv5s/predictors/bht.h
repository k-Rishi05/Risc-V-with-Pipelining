#ifndef RV5S_PREDICTORS_BHT_H
#define RV5S_PREDICTORS_BHT_H

#include <cstddef>
#include <cstdint>
#include <iosfwd>
class BHT {
public:
  virtual ~BHT() = default;
  virtual bool predict(uint64_t pc) = 0;
  virtual void update(uint64_t pc, bool taken) = 0;
  virtual void reset() = 0;
  virtual void debugDump(std::ostream& /*os*/) const {}
};

#endif // RV5S_PREDICTORS_BHT_H
