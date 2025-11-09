#ifndef RV5S_PREDICTORS_ONE_BIT_BTB_H
#define RV5S_PREDICTORS_ONE_BIT_BTB_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include "vm/rv5s/predictor.h"
#include "vm/rv5s/predictors/bht.h"
#include "vm/rv5s/predictors/btb.h"

/**
 * @brief 1-bit Branch Predictor combining BHT and BTB
 * Uses modular BHT (for direction) and BTB (for target) components
 */
class OneBitBTB : public Predictor {
public:
  explicit OneBitBTB(size_t entries = 32);

  PredictResult predict(uint64_t pc, uint32_t instr) override;
  void update(uint64_t pc, bool is_branch, bool taken, uint64_t target) override;
  void reset() override;
  void debugDump(std::ostream& os) const override;

private:
  std::unique_ptr<BHT> bht_;      // Branch History Table for direction prediction
  std::unique_ptr<BTB> btb_;      // Branch Target Buffer for target caching
};

#endif // RV5S_PREDICTORS_ONE_BIT_BTB_H
