#pragma once

#include "vm/rv5s/predictors/bht.h"
#include <vector>
#include <cstdint>
#include <ostream>
#include <cmath>

class Perceptron {
public:
  std::vector<int> weights;  
  explicit Perceptron(int history_length) {
    weights.resize(history_length + 1, 0);
  }
  void reset() {
    for (auto& w : weights) {
      w = 0;
    }
  }
};

class PerceptronBHT : public BHT {
public:
  explicit PerceptronBHT(size_t entries, uint32_t history_length);
  
  bool predict(uint64_t pc) override;
  void update(uint64_t pc, bool taken) override;
  void reset() override;
  void debugDump(std::ostream& os) const override;

private:

  const int history_length_;         // ghr length
  const int num_perceptrons_;       
  const int training_threshold_;    
  static constexpr int MAX_WEIGHT = 127;
  static constexpr int MIN_WEIGHT = -128;

  std::vector<Perceptron> table_;
  std::vector<bool> ghr_;
  

  size_t getIndex(uint64_t pc) const { return (pc >> 2) % num_perceptrons_; }
  int computeOutput(size_t index) const;
  void updateGHR(bool taken);
};
