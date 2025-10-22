/**
 * @file config.h
 * @brief Contains configuration options for the assembler.
 * @author Vishank Singh, https://github.com/VishankSingh
 */
#ifndef CONFIG_H
#define CONFIG_H

#include "globals.h"
#include <string>
#include <iostream>
#include <stdexcept>
#include <cstdint>
#include <fstream>
#include <sstream>

/**
 * @namespace vm_config
 * @brief Namespace for VM configuration management.
 */
namespace vm_config {

// High-level pipeline modes (project deliverables)
enum class PipelineMode : uint8_t {
  SINGLE_CYCLE = 1,        // Mode 1
  PIPE_NO_HAZ = 2,         // Mode 2
  PIPE_STALL = 3,          // Mode 3
  PIPE_FWD = 4,            // Mode 4
  PIPE_STATIC_BP = 5,      // Mode 5
  PIPE_DYN1_BP = 6         // Mode 6
};

// Branch resolution stage control (kept simple)
enum class BranchResolveStage : uint8_t {
  EX = 0,
  ID = 1
};

struct VmConfig {
  PipelineMode pipeline_mode = PipelineMode::PIPE_NO_HAZ; // default to Mode 2 for multi-stage
  uint64_t run_step_delay = 300;
  uint64_t memory_size = 0xffffffffffffffff; // 64-bit address space
  uint64_t memory_block_size = 1024; // 1 KB blocks
  uint64_t data_section_start = 0x10000000; // Default start address for data section
  uint64_t text_section_start = 0x0; // Default start address for text section
  uint64_t bss_section_start = 0x11000000; // Default start address for BSS section

  uint64_t instruction_execution_limit = 100;

  bool m_extension_enabled = true;
  bool f_extension_enabled = true;
  bool d_extension_enabled = true;

  // Feature flags derived from pipeline_mode; can be overridden via config
  bool hazard_detection_enabled = false;
  bool forwarding_enabled = false;
  enum class PredictorKind : uint8_t { None=0, Static=1, OneBit=2 };
  PredictorKind predictor = PredictorKind::None;
  BranchResolveStage branch_resolve_stage = BranchResolveStage::EX; // optional tuning

  // VM type removed; selection is based solely on pipeline_mode
  void setPipelineMode(PipelineMode mode) {
    pipeline_mode = mode;
    // Set defaults for feature flags based on mode
    switch (mode) {
      case PipelineMode::SINGLE_CYCLE:
        hazard_detection_enabled = false;
        forwarding_enabled = false;
        predictor = PredictorKind::None;
        branch_resolve_stage = BranchResolveStage::EX;
        break;
      case PipelineMode::PIPE_NO_HAZ:
        hazard_detection_enabled = false;
        forwarding_enabled = false;
        predictor = PredictorKind::None;
        branch_resolve_stage = BranchResolveStage::EX;
        break;
      case PipelineMode::PIPE_STALL:
        hazard_detection_enabled = true;
        forwarding_enabled = false;
        predictor = PredictorKind::None;
        branch_resolve_stage = BranchResolveStage::EX;
        break;
      case PipelineMode::PIPE_FWD:
        hazard_detection_enabled = true;
        forwarding_enabled = true;
        predictor = PredictorKind::None;
        branch_resolve_stage = BranchResolveStage::EX;
        break;
      case PipelineMode::PIPE_STATIC_BP:
        hazard_detection_enabled = true;
        forwarding_enabled = true;
        predictor = PredictorKind::Static;
        branch_resolve_stage = BranchResolveStage::EX; // can be changed to ID if implemented
        break;
      case PipelineMode::PIPE_DYN1_BP:
        hazard_detection_enabled = true;
        forwarding_enabled = true;
        predictor = PredictorKind::OneBit;
        branch_resolve_stage = BranchResolveStage::EX; // can be changed to ID if implemented
        break;
    }
  }
  PipelineMode getPipelineMode() const { return pipeline_mode; }
  void setRunStepDelay(uint64_t delay) {
    run_step_delay = delay;
    std::cout << "Run step delay set to: " << run_step_delay << " ms" << std::endl;
  }
  uint64_t getRunStepDelay() const {
    return run_step_delay;
  }
  void setMemorySize(uint64_t size) {
    memory_size = size;
  }
  uint64_t getMemorySize() const {
    return memory_size;
  }
  void setMemoryBlockSize(uint64_t size) {
    memory_block_size = size;
  }
  uint64_t getMemoryBlockSize() const {
    return memory_block_size;
  }
  void setDataSectionStart(uint64_t start) {
    data_section_start = start;
  }
  uint64_t getDataSectionStart() const {
    return data_section_start;
  }

  void setTextSectionStart(uint64_t start) {
    text_section_start = start;
  }

  uint64_t getTextSectionStart() const {
    return text_section_start;
  }

  void setBssSectionStart(uint64_t start) {
    bss_section_start = start;
  }

  uint64_t getBssSectionStart() const {
    return bss_section_start;
  }

  void setInstructionExecutionLimit(uint64_t limit) {
    instruction_execution_limit = limit;
  }

  uint64_t getInstructionExecutionLimit() const {
    return instruction_execution_limit;
  }

  void setMExtensionEnabled(bool enabled) {
    m_extension_enabled = enabled;
  }

  bool getMExtensionEnabled() const {
    return m_extension_enabled;
  }

  void setFExtensionEnabled(bool enabled) {
    f_extension_enabled = enabled;
  }

  bool getFExtensionEnabled() const {
    return f_extension_enabled;
  }

  void setDExtensionEnabled(bool enabled) {
    d_extension_enabled = enabled;
  }

  bool getDExtensionEnabled() const {
    return d_extension_enabled;
  }

  void setHazardDetectionEnabled(bool enabled) { hazard_detection_enabled = enabled; }
  bool getHazardDetectionEnabled() const { return hazard_detection_enabled; }

  void setForwardingEnabled(bool enabled) { forwarding_enabled = enabled; }
  bool getForwardingEnabled() const { return forwarding_enabled; }

  void setPredictor(PredictorKind kind) { predictor = kind; }
  PredictorKind getPredictor() const { return predictor; }

  void setBranchResolveStage(BranchResolveStage s) { branch_resolve_stage = s; }
  BranchResolveStage getBranchResolveStage() const { return branch_resolve_stage; }

  void modifyConfig(const std::string &section, const std::string &key, const std::string &value) {
    if (section == "Execution" || section == "e") {
      if (key == "pipeline_mode" || key == "m") {
        if (value == "1" || value == "single_cycle") setPipelineMode(PipelineMode::SINGLE_CYCLE);
        else if (value == "2" || value == "pipe_no_haz") setPipelineMode(PipelineMode::PIPE_NO_HAZ);
        else if (value == "3" || value == "pipe_stall") setPipelineMode(PipelineMode::PIPE_STALL);
        else if (value == "4" || value == "pipe_fwd") setPipelineMode(PipelineMode::PIPE_FWD);
        else if (value == "5" || value == "pipe_static_bp") setPipelineMode(PipelineMode::PIPE_STATIC_BP);
        else if (value == "6" || value == "pipe_dyn1_bp") setPipelineMode(PipelineMode::PIPE_DYN1_BP);
        else throw std::invalid_argument("Unknown pipeline_mode: " + value);
      } else if (key == "processor_type") {
        // Deprecated: map to pipeline_mode for backward compatibility
        if (value == "single_stage") setPipelineMode(PipelineMode::SINGLE_CYCLE);
        else if (value == "multi_stage") setPipelineMode(PipelineMode::PIPE_NO_HAZ);
        else throw std::invalid_argument("Unknown processor_type (deprecated): " + value);
      } else if (key == "hazard_detection_enabled") {
        if (value == "true") setHazardDetectionEnabled(true);
        else if (value == "false") setHazardDetectionEnabled(false);
        else throw std::invalid_argument("Unknown value: " + value);
      } else if (key == "forwarding_enabled") {
        if (value == "true") setForwardingEnabled(true);
        else if (value == "false") setForwardingEnabled(false);
        else throw std::invalid_argument("Unknown value: " + value);
      } else if (key == "predictor") {
        if (value == "none") setPredictor(PredictorKind::None);
        else if (value == "static") setPredictor(PredictorKind::Static);
        else if (value == "onebit") setPredictor(PredictorKind::OneBit);
        else throw std::invalid_argument("Unknown predictor: " + value);
      } else if (key == "branch_resolve_stage") {
        if (value == "EX" || value == "ex") setBranchResolveStage(BranchResolveStage::EX);
        else if (value == "ID" || value == "id") setBranchResolveStage(BranchResolveStage::ID);
        else throw std::invalid_argument("Unknown branch_resolve_stage: " + value);
      } else if (key == "run_step_delay") {
        setRunStepDelay(std::stoull(value));
      } else if (key == "instruction_execution_limit") {
        setInstructionExecutionLimit(std::stoull(value));
      }
      
      else {
        throw std::invalid_argument("Unknown key: " + key);
      }
    } else if (section == "Memory") {
      if (key == "memory_size") {
        setMemorySize(std::stoull(value));
      } else if (key == "memory_block_size") {
        setMemoryBlockSize(std::stoull(value));
      } else if (key == "data_section_start") {
        setDataSectionStart(std::stoull(value, nullptr, 16));
      } else if (key == "text_section_start") {
        setTextSectionStart(std::stoull(value, nullptr, 16));
      } else if (key == "bss_section_start") {
        setBssSectionStart(std::stoull(value, nullptr, 16));
      }
      
      
      
      else {
        throw std::invalid_argument("Unknown key: " + key);
      }
    } 

    else if (section == "Assembler") {
      if (key == "m_extension_enabled") {
        if (value == "true") {
          setMExtensionEnabled(true);
        } else if (value == "false") {
          setMExtensionEnabled(false);
        } else {
          throw std::invalid_argument("Unknown value: " + value);
        }
      } else if (key == "f_extension_enabled") {
        if (value == "true") {
          setFExtensionEnabled(true);
        } else if (value == "false") {
          setFExtensionEnabled(false);  
        } else {
          throw std::invalid_argument("Unknown value: " + value);
        }
      } else if (key == "d_extension_enabled") {
        if (value == "true") {
          setDExtensionEnabled(true);
        } else if (value == "false") {
          setDExtensionEnabled(false);
        } else {
          throw std::invalid_argument("Unknown value: " + value);
        }
      }
    }
    else {
      throw std::invalid_argument("Unknown section: " + section);
    }
  }


};

extern VmConfig config;


} // namespace vm_config


#endif // CONFIG_H
