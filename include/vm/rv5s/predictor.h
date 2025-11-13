#ifndef RV5S_PREDICTOR_H
#define RV5S_PREDICTOR_H

#include <cstdint>
#include <memory>
#include <iosfwd>

namespace vm_config { struct VmConfig; enum class PipelineMode : uint8_t; enum class BranchResolveStage : uint8_t; }

struct PredictResult {
  bool valid{false};
  bool taken{false};
  uint64_t target{0};
};

class Predictor {
public:
  virtual ~Predictor() = default;
  virtual PredictResult predict(uint64_t pc, uint32_t instr) = 0;
  virtual void update(uint64_t pc, bool is_branch, bool taken, uint64_t target) = 0;
  virtual void reset() {}
  virtual void debugDump(std::ostream& os) const {}
};


#endif // RV5S_PREDICTOR_H
