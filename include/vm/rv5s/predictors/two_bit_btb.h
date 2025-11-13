#pragma once

#include "vm/rv5s/predictor.h"
#include "vm/rv5s/predictors/two_bit_bht.h"
#include "vm/rv5s/predictors/btb.h"
#include <memory>
#include <cstdint>
#include <ostream>

class TwoBitBTB : public Predictor {
public:
  explicit TwoBitBTB(size_t entries);
  
  PredictResult predict(uint64_t pc, uint32_t instr) override;
  void update(uint64_t pc, bool is_branch, bool taken, uint64_t target) override;
  void reset() override;
  void debugDump(std::ostream& os) const override;

private:
  std::unique_ptr<BHT> bht_; 
  std::unique_ptr<BTB> btb_;  
};
