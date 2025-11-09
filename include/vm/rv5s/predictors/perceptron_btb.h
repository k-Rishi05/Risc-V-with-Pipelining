#pragma once

#include "vm/rv5s/predictor.h"
#include "vm/rv5s/predictors/perceptron_bht.h"
#include "vm/rv5s/predictors/btb.h"
#include <memory>
#include <cstdint>
#include <ostream>

/**
 * @brief Perceptron predictor with BTB
 * Combines PerceptronBHT (direction prediction) and DirectMappedBTB (target caching)
 * 
 * This predictor uses neural perceptrons to learn branch patterns
 * and correlates global history for better prediction accuracy.
 */
class PerceptronBTB : public Predictor {
public:
  explicit PerceptronBTB(size_t entries);
  
  PredictResult predict(uint64_t pc, uint32_t instr) override;
  void update(uint64_t pc, bool is_branch, bool taken, uint64_t target) override;
  void reset() override;
  void debugDump(std::ostream& os) const override;

private:
  std::unique_ptr<PerceptronBHT> bht_;  // Perceptron-based direction predictor
  std::unique_ptr<BTB> btb_;            // Target cache (reused infrastructure)
};
