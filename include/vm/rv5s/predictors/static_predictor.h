#ifndef RV5S_PREDICTORS_STATIC_PREDICTOR_H
#define RV5S_PREDICTORS_STATIC_PREDICTOR_H

#include "vm/rv5s/predictor.h"
class StaticPredictor : public Predictor {
public:
  PredictResult predict(uint64_t pc, uint32_t /*instr*/) override {
    return PredictResult{true,false,pc+4};
  }
  void update(uint64_t /*pc*/, bool /*is_branch*/, bool /*taken*/, uint64_t /*target*/) override {}
};

#endif // RV5S_PREDICTORS_STATIC_PREDICTOR_H
