/**
 * @file rv5s_vm.h
 * @brief RV5S (5-stage pipeline) VM definition (minimal skeleton, no data-hazard handling)
 */
#ifndef RV5S_VM_H
#define RV5S_VM_H

#include "vm/vm_base.h"
#include "vm/alu.h"
#include "vm/rv5s/rv5s_control_unit.h"
#include "vm/rv5s/hazard_unit.h"
#include "vm/rv5s/forward_unit.h"
#include <iostream>
#include <vector>
#include <stack>

#include <cstdint>

// Minimal 5-stage pipeline register bundles
struct IFID {
	bool valid{false};
	uint32_t instr{0};
	uint64_t pc{0};
};

struct IDEX {
	bool valid{false};
	bool is_bubble{false}; // true if this is a stall bubble, not a real instruction
	uint32_t instr{0};
	uint64_t pc{0};
	uint8_t opcode{0};
	uint8_t funct3{0};
	uint8_t rs1{0};
	uint8_t rs2{0};
	uint8_t rd{0};
	int32_t imm{0};
	uint64_t rs1_val{0};
	uint64_t rs2_val{0};
	// control
	bool alu_src{false};
	bool mem_to_reg{false};
	bool reg_write{false};
	bool mem_read{false};
	bool mem_write{false};
	bool branch{false};
	alu::AluOp alu_signal{alu::AluOp::kNone};
};

struct EXMEM {
	bool valid{false};
	bool is_bubble{false}; // true if this is a stall bubble, not a real instruction
	uint32_t instr{0};
	uint64_t pc{0};
	uint8_t opcode{0};
	uint8_t funct3{0};
	uint8_t rd{0};
	uint64_t rs2_val{0}; // for store data
	// control
	bool mem_to_reg{false};
	bool reg_write{false};
	bool mem_read{false};
	bool mem_write{false};
	// results
	uint64_t alu_result{0};
	bool branch_taken{false};
	uint64_t branch_target{0};
};

struct MEMWB {
	bool valid{false};
	bool is_bubble{false}; // true if this is a stall bubble, not a real instruction
	uint32_t instr{0};
	uint8_t rd{0};
	bool mem_to_reg{false};
	bool reg_write{false};
	uint64_t alu_result{0};
	uint64_t mem_data{0};
};

struct RegisterChange5
{
	unsigned int reg_index;
	unsigned int reg_type; // 0 for GPR, 1 for CSR, 2 for FPR
	uint64_t old_value;
	uint64_t new_value;
};

struct MemoryChange5
{
	uint64_t address;
	std::vector<uint8_t> old_bytes_vec; 
	std::vector<uint8_t> new_bytes_vec; 
};

struct StepDelta5
{
	uint64_t old_pc;
	uint64_t new_pc;
	std:: vector<RegisterChange5> register_changes;
	std:: vector<MemoryChange5> memory_changes;
	IFID ifid;
	IDEX idex;
	EXMEM exmem;
	MEMWB memwb;
	uint64_t cycle_s;
	uint64_t instructions_retired;
};


class RV5SVM : public VmBase {
 public:
	RV5SVM();
		~RV5SVM() = default;

	std::stack<StepDelta5> undo_stack_;
	std::stack<StepDelta5> redo_stack_;
	StepDelta5 current_delta_;

	void Run() override;
	void DebugRun() override;
	void Step() override; // one cycle
	void Undo() override; // not implemented
	void Redo() override; // not implemented
	void Reset() override;

	void PrintType() { std::cout << "rv5svm" << std::endl; }

 private:
	// control/decode helper
	RV5SControlUnit control_;

	// Feature flags (reserved for future use)
	bool hazard_detection_enabled_{false};
	bool forwarding_enabled_{false};
	enum class PredictorKind : uint8_t { None=0, Static=1, OneBit=2 };
	PredictorKind predictor_{PredictorKind::None};

	// Optional: control hazard resolution stage (unused in basic pipeline)
	enum class BranchResolveStage : uint8_t { EX=0, ID=1 };
	BranchResolveStage branch_resolve_stage_{BranchResolveStage::EX};

	// pipeline state
	IFID if_id_{};
	IDEX id_ex_{};
	EXMEM ex_mem_{};
	MEMWB mem_wb_{};

	// simple stats
	uint64_t stall_cycles_{0};
	uint64_t load_use_stalls_{0};
	uint64_t fwd_hits_{0};
	uint64_t mispredictions_{0};

	// stage helpers (in-place, back-to-front safe)
	void stageIF();
	void stageID();
	void stageEX();
	void stageMEM();
	void stageWB();

	// hazard detection (stall only)
	HazardUnit hazard_{};
	ForwardUnit forward_{};
	bool stall_if_id_{false};
	int stall_counter_{0};
	bool flush_if_once_{false};

	// internal helpers (none for basic pipeline)
	std::string DisassembleInstruction(uint32_t instr) const;
	void PrintPipelineState();

	bool pipelineEmpty() const;
};

#endif // RV5S_VM_H

