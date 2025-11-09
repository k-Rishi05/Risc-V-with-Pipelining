#ifndef RV5S_PREDICTORS_BHT_H
#define RV5S_PREDICTORS_BHT_H

#include <cstddef>
#include <cstdint>
#include <vector>
#include <iosfwd>

/**
 * @brief Branch History Table (BHT) - stores branch direction predictions
 * Manages only the taken/not-taken prediction for branches
 */
class BHT {
public:
  virtual ~BHT() = default;
  
  /**
   * @brief Predict branch direction for given PC
   * @param pc Program counter of the branch
   * @return true if predict taken, false if predict not-taken
   */
  virtual bool predict(uint64_t pc) = 0;
  
  /**
   * @brief Update predictor with actual outcome
   * @param pc Program counter of the branch
   * @param taken Actual outcome (true=taken, false=not-taken)
   */
  virtual void update(uint64_t pc, bool taken) = 0;
  
  /**
   * @brief Reset predictor state
   */
  virtual void reset() = 0;
  
  /**
   * @brief Debug dump of internal state
   */
  virtual void debugDump(std::ostream& /*os*/) const {}
};

/**
 * @brief 1-bit BHT implementation
 * Each entry stores the last outcome (0=not-taken, 1=taken)
 * Prediction for next time = last outcome
 */
class OneBitBHT : public BHT {
public:
  explicit OneBitBHT(size_t entries = 128);
  
  bool predict(uint64_t pc) override;
  void update(uint64_t pc, bool taken) override;
  void reset() override;
  void debugDump(std::ostream& os) const override;
  
private:
  size_t size_;
  std::vector<bool> table_; // 1-bit per entry
  
  static size_t normalize(size_t n);
  inline size_t index(uint64_t pc) const { return (pc >> 2) & (size_ - 1); }
};

#endif // RV5S_PREDICTORS_BHT_H
