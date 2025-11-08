#ifndef RV5S_PREDICTOR_H
#define RV5S_PREDICTOR_H

#include <cstdint>
#include <memory>
#include <iosfwd>

namespace vm_config { struct VmConfig; enum class PipelineMode : uint8_t; enum class BranchResolveStage : uint8_t; }

struct PredictResult {
  bool valid{false};      // prediction available
  bool taken{false};      // predicted direction
  uint64_t target{0};     // predicted target if taken
};

class Predictor {
public:
  virtual ~Predictor() = default;
  virtual PredictResult predict(uint64_t pc, uint32_t instr) = 0;
  virtual void update(uint64_t pc, bool is_branch, bool taken, uint64_t target) = 0;
  virtual void reset() {}
  // Optional: debug-dump internal state (no-op by default)
  virtual void debugDump(std::ostream& os) const {}
};


#endif // RV5S_PREDICTOR_H
