#include "vm/rv5s/predictor.h"
#include "vm/rv5s/predictors/one_bit_btb.h"
#include "vm/rv5s/predictors/two_bit_btb.h"
#include "vm/rv5s/predictors/perceptron_btb.h"
#include "vm/rv5s/predictors/gshare_btb.h"
#include "vm/rv5s/predictors/static_predictor.h"
#include "config.h"

std::unique_ptr<Predictor> makePredictor(vm_config::VmConfig::PredictorKind kind) {
  using PredictorKind = vm_config::VmConfig::PredictorKind;
  switch (kind) {
    case PredictorKind::Static:
      return std::make_unique<StaticPredictor>();
    case PredictorKind::OneBit:
      return std::make_unique<OneBitBTB>(32);
    case PredictorKind::TwoBit:
      return std::make_unique<TwoBitBTB>(32);
    case PredictorKind::Perceptron:
      // Use configured history length from config
      return std::make_unique<PerceptronBTB>(32, vm_config::config.perceptron_history_length);
    case PredictorKind::Gshare:
      // Use configured history length from config
      return std::make_unique<GshareBTB>(32, vm_config::config.gshare_history_length);
    case PredictorKind::None:
    default:
      return nullptr;
  }
}
