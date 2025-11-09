#include "vm/rv5s/predictor.h"
#include "vm/rv5s/predictors/one_bit_btb.h"
#include "vm/rv5s/predictors/static_predictor.h"
#include "config.h"

std::unique_ptr<Predictor> makePredictor(vm_config::VmConfig::PredictorKind kind) {
  using PredictorKind = vm_config::VmConfig::PredictorKind;
  switch (kind) {
    case PredictorKind::Static:
      return std::make_unique<StaticPredictor>();
    case PredictorKind::OneBit:
      return std::make_unique<OneBitBTB>(32);
    case PredictorKind::None:
    default:
      return nullptr;
  }
}
