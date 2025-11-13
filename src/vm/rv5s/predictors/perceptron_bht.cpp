#include "vm/rv5s/predictors/perceptron_bht.h"
#include <iostream>
#include <iomanip>
#include <cmath>

PerceptronBHT::PerceptronBHT(size_t entries, uint32_t history_length)
  : history_length_(history_length),
    num_perceptrons_(entries),  
    training_threshold_(static_cast<int>(std::floor(1.93 * history_length + 14))) {
  
  table_.reserve(num_perceptrons_);
  for (int i = 0; i < num_perceptrons_; ++i) {
    table_.emplace_back(history_length_);
  }
  
  ghr_.clear();
}

bool PerceptronBHT::predict(uint64_t pc) {
  size_t index = getIndex(pc);
  int y = computeOutput(index);
  return (y >= 0);
}

void PerceptronBHT::update(uint64_t pc, bool taken) {
  size_t index = getIndex(pc);
  int y = computeOutput(index);

  int t = taken ? 1 : -1;
  
  bool prediction_was_wrong = ((y >= 0) != taken);
  bool confidence_was_low = (std::abs(y) <= training_threshold_);
  
  if (prediction_was_wrong || confidence_was_low) {
    std::vector<int> x;
    x.push_back(1);  // bias
    for (auto it = ghr_.rbegin(); it != ghr_.rend(); ++it) {
      x.push_back(*it ? 1 : -1); // bipolar vector
    }
    while (x.size() <= static_cast<size_t>(history_length_)) {
      x.push_back(-1);
    }
    
    auto& weights = table_[index].weights;
    for (size_t i = 0; i < weights.size() && i < x.size(); ++i) {
      if (t == x[i]) {
        if (weights[i] < MAX_WEIGHT) weights[i]++;
      } else {
        if (weights[i] > MIN_WEIGHT) weights[i]--;
      }
    }
  }
  updateGHR(taken);
}

void PerceptronBHT::reset() {
  for (auto& perceptron : table_) {
    perceptron.reset();
  }
  ghr_.clear();
}

void PerceptronBHT::debugDump(std::ostream& os) const {
  os << "PerceptronBHT[perceptrons=" << num_perceptrons_ 
     << ", history_len=" << history_length_ << "]\n";
  os << "  GHR length: " << ghr_.size() << "/" << history_length_ << "\n";
  os << "  Training threshold: " << training_threshold_ << "\n";
  
  // Show GHR state (most recent at the END, oldest at start)
  os << "  Current GHR: ";
  if (ghr_.empty()) {
    os << "(empty - no branches executed yet)";
  } else {
    os << "[";
    for (size_t i = 0; i < std::min(ghr_.size(), size_t(15)); ++i) {
      if (i > 0) os << " ";
      os << (ghr_[i] ? "T" : "N");
    }
    if (ghr_.size() > 15) {
      os << " ... (+" << (ghr_.size() - 15) << " more)";
    }
    os << "] (oldest→newest)";
  }
  os << "\n";
  
  // Show statistics about perceptron weights
  int total_nonzero_weights = 0;
  int perceptrons_with_training = 0;
  
  for (const auto& perceptron : table_) {
    bool has_nonzero = false;
    for (int w : perceptron.weights) {
      if (w != 0) {
        total_nonzero_weights++;
        has_nonzero = true;
      }
    }
    if (has_nonzero) {
      perceptrons_with_training++;
    }
  }
  
  os << "  Perceptrons trained: " << perceptrons_with_training << "/" << num_perceptrons_ << "\n";
  os << "  Non-zero weights: " << total_nonzero_weights 
     << "/" << (num_perceptrons_ * (history_length_ + 1)) << "\n";
  
  // Show a few example perceptrons (first 3 with non-zero weights)
  os << "  Sample perceptrons (first 3 trained):\n";
  int shown = 0;
  for (size_t i = 0; i < table_.size() && shown < 3; ++i) {
    const auto& p = table_[i].weights;
    bool has_nonzero = false;
    for (int w : p) {
      if (w != 0) {
        has_nonzero = true;
        break;
      }
    }
    
    if (has_nonzero) {
      os << "    Perceptron[" << i << "]: bias=" << p[0];
      
      // Show first few history weights
      os << ", weights=[";
      for (size_t j = 1; j < std::min(p.size(), size_t(6)); ++j) {
        if (j > 1) os << ",";
        os << p[j];
      }
      if (p.size() > 6) {
        os << "...";
      }
      os << "]\n";
      shown++;
    }
  }
  
  if (shown == 0) {
    os << "    (no perceptrons trained yet)\n";
  }
}

int PerceptronBHT::computeOutput(size_t index) const {
  const auto& weights = table_[index].weights;
  int y = weights[0];
  size_t weight_idx = 1;
  for (auto it = ghr_.rbegin(); it != ghr_.rend() && weight_idx < weights.size(); ++it, ++weight_idx) {
    int x_i = (*it) ? 1 : -1;
    y += weights[weight_idx] * x_i;
  }
  for (; weight_idx < weights.size(); ++weight_idx) {
    y += weights[weight_idx] * (-1);
  }
  return y;
}

void PerceptronBHT::updateGHR(bool taken) {
  ghr_.push_back(taken);
  if (ghr_.size() > static_cast<size_t>(history_length_)) {
    ghr_.erase(ghr_.begin());
  }
}
