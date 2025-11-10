#pragma once

#include "vm/rv5s/predictors/bht.h"
#include <vector>
#include <cstdint>
#include <ostream>
#include <cmath>

/**
 * @brief Single Perceptron - a vector of weights
 * Each perceptron has (HISTORY_LENGTH + 1) weights
 * weights[0] is the bias weight (w0)
 * weights[1...h] correspond to global history bits
 */
class Perceptron {
public:
  std::vector<int> weights;
  
  explicit Perceptron(int history_length) {
    // Initialize all h+1 weights to 0
    weights.resize(history_length + 1, 0);
  }
  
  void reset() {
    for (auto& w : weights) {
      w = 0;
    }
  }
};

/**
 * @brief Perceptron-based Branch History Table
 * below is from the paper
 * Hardware budget: 4KB
 * - HISTORY_LENGTH (h): 28
 * - NUM_PERCEPTRONS (N): 141
 * - TRAINING_THRESHOLD (θ): 68
 * - WEIGHT_BITS: 8 (range: -128 to +127)
 */
class PerceptronBHT : public BHT {
public:
  explicit PerceptronBHT(size_t entries);
  
  bool predict(uint64_t pc) override;
  void update(uint64_t pc, bool taken) override;
  void reset() override;
  void debugDump(std::ostream& os) const override;
  
  /**
   * @brief Get the perceptron output and index for a given PC
   * This is used to store metadata for training later
   * @param pc Program counter
   * @param out_index Output parameter for perceptron index
   * @param out_output Output parameter for perceptron output (y)
   * @return Prediction (true=taken, false=not-taken)
   */
  bool predictWithMetadata(uint64_t pc, int& out_index, int& out_output);
  
  /**
   * @brief Train the perceptron with actual outcome
   * @param perceptron_index The index of the perceptron used for prediction
   * @param perceptron_output The raw output (y) from prediction
   * @param history_snapshot The bipolar history vector used for prediction
   * @param actual_taken The actual branch outcome
   */
  void train(int perceptron_index, int perceptron_output, 
             const std::vector<int>& history_snapshot, bool actual_taken);

private:
  // Configuration (optimal for 4KB budget)
  static constexpr int HISTORY_LENGTH = 28;
  static constexpr int NUM_PERCEPTRONS = 141;  // floor((4096*8)/((28+1)*8))
  static constexpr int TRAINING_THRESHOLD = 68; // floor(1.93*28+14)
  static constexpr int MAX_WEIGHT = 127;
  static constexpr int MIN_WEIGHT = -128;
  
  // Perceptron table
  std::vector<Perceptron> table_;
  
  // Global History Register (stores last h branch outcomes)
  std::vector<bool> ghr_;  // Most recent at back
  
  // Helper functions
  size_t getIndex(uint64_t pc) const { 
    return (pc >> 2) % NUM_PERCEPTRONS; 
  }
  
  // Get current GHR as bipolar vector {-1, 1}
  std::vector<int> getBipolarHistory() const;
  
  // Update GHR with new branch outcome (speculative update)
  void updateGHR(bool taken);
};
