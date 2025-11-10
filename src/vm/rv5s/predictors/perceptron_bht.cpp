#include "vm/rv5s/predictors/perceptron_bht.h"
#include <iostream>
#include <iomanip>
#include <cmath>

PerceptronBHT::PerceptronBHT(size_t entries, uint32_t history_length)
  : history_length_(history_length),
    num_perceptrons_(entries),  // Use entries parameter as number of perceptrons
    training_threshold_(static_cast<int>(std::floor(1.93 * history_length + 14))) {
  
  // Initialize table with num_perceptrons_ perceptrons
  table_.reserve(num_perceptrons_);
  for (int i = 0; i < num_perceptrons_; ++i) {
    table_.emplace_back(history_length_);
  }
  
  // Initialize GHR to empty (all not-taken)
  ghr_.clear();
}

bool PerceptronBHT::predict(uint64_t pc) {
  int dummy_index, dummy_output;
  return predictWithMetadata(pc, dummy_index, dummy_output);
}

bool PerceptronBHT::predictWithMetadata(uint64_t pc, int& out_index, int& out_output) {
  // Get perceptron index from PC
  size_t index = getIndex(pc);
  out_index = static_cast<int>(index);
  
  // Get bipolar history vector
  std::vector<int> x = getBipolarHistory();
  
  // Get perceptron weights
  const auto& weights = table_[index].weights;
  
  // Compute dot product: y = Σ(w_i * x_i)
  int y = 0;
  for (size_t i = 0; i < weights.size() && i < x.size(); ++i) {
    y += weights[i] * x[i];
  }
  
  out_output = y;
  
  // Prediction: taken if y >= 0
  bool prediction = (y >= 0);
  
  // Speculative GHR update (will be corrected on misprediction)
  updateGHR(prediction);
  
  return prediction;
}

void PerceptronBHT::update(uint64_t pc, bool taken) {
  // 1. Reconstruct the prediction metadata 2. Train the perceptron 3. Correct GHR if prediction was wrong
  
  size_t index = getIndex(pc);
  
  // Get CURRENT GHR state (which includes speculative updates from prediction)
  // We need to rewind by 1 to get the history AT THE TIME of prediction
  bool last_prediction = false;
  if (!ghr_.empty()) {
    last_prediction = ghr_.back();
    ghr_.pop_back();  // Remove speculative update
  }
  
  // Now get the history that was used for prediction
  std::vector<int> x = getBipolarHistory();
  
  // Recompute output for training condition
  const auto& weights = table_[index].weights;
  int y = 0;
  for (size_t i = 0; i < weights.size() && i < x.size(); ++i) {
    y += weights[i] * x[i];
  }
  
  // Train with the actual outcome
  train(static_cast<int>(index), y, x, taken);
  
  // Update GHR with ACTUAL outcome (non-speculative update)
  updateGHR(taken);
}

void PerceptronBHT::train(int perceptron_index, int perceptron_output,
                          const std::vector<int>& history_snapshot, bool actual_taken) {
  // Define target: +1 for taken, -1 for not-taken
  int t = actual_taken ? 1 : -1;
  
  // Train if prediction was wrong OR confidence was low
  bool prediction_was_wrong = ((perceptron_output >= 0) != actual_taken);
  bool confidence_was_low = (std::abs(perceptron_output) <= training_threshold_);
  
  if (prediction_was_wrong || confidence_was_low) {
    // Perform weight update
    auto& weights = table_[perceptron_index].weights;
    
    // Update all h+1 weights
    for (size_t i = 0; i < weights.size() && i < history_snapshot.size(); ++i) {
      int x_i = history_snapshot[i];
      
      // Update rule: w_i = w_i + t*x_i
      // If t and x_i have same sign, increment; otherwise decrement
      if (t == x_i) {
        if (weights[i] < MAX_WEIGHT) {
          weights[i]++;
        }
      } else {
        if (weights[i] > MIN_WEIGHT) {
          weights[i]--;
        }
      }
    }
  }
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

std::vector<int> PerceptronBHT::getBipolarHistory() const {
  std::vector<int> bipolar;
  
  // First element is always +1 (bias input)
  bipolar.push_back(1);
  
  // Add history bits as ±1 (most recent first, which is at back of ghr_)
  for (auto it = ghr_.rbegin(); it != ghr_.rend(); ++it) {
    bipolar.push_back(*it ? 1 : -1);
  }
  
  // Pad with -1 (not-taken) if history not yet full
  while (bipolar.size() <= static_cast<size_t>(history_length_)) {
    bipolar.push_back(-1);
  }
  
  return bipolar;
}

void PerceptronBHT::updateGHR(bool taken) {
  // Add new outcome at the back (most recent)
  ghr_.push_back(taken);
  
  // Keep only last history_length_ outcomes
  if (ghr_.size() > static_cast<size_t>(history_length_)) {
    ghr_.erase(ghr_.begin());
  }
}
