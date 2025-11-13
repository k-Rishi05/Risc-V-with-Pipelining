#ifndef RV5S_PREDICTOR_FACTORY_H
#define RV5S_PREDICTOR_FACTORY_H

#include <memory>
#include "vm/rv5s/predictor.h"
#include "config.h"

// Factory: create a predictor instance based on configured kind.
// Note: Some modes (e.g., static BP) may not require a runtime predictor object;
// callers can decide whether to use it. Returning nullptr is valid for None.
std::unique_ptr<Predictor> makePredictor(vm_config::VmConfig::PredictorKind kind);

#endif // RV5S_PREDICTOR_FACTORY_H
