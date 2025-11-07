# Branch Prediction Plan for RV5S 5-Stage Pipeline

This document outlines modern and practical branch predictors to add to your RISC-V 5-stage simulator, how to integrate them cleanly, data structures, pseudocode, metrics, and references. It starts with your current control-hazard flow (resolve in EX, flush IF/ID) and layers predictors and BTB/RAS on top.

## TL;DR
- Integrate a predictor interface used in IF and updated in EX.
- Start with: Static, 1-bit, 2-bit saturating, GShare, Local, Tournament (Local vs GShare), and a light Loop predictor.
- Ambitious options: TAGE-lite and Perceptron (compact variants).
- Add a small BTB and RAS for target prediction of JAL/JALR and returns.
- Log mispredictions, MPKI, CPI deltas; compare across modes and benchmarks.

---

## 1) Current Pipeline Control Flow (as implemented)
- Branch/JAL/JALR resolved in EX. On taken, set `program_counter_ = branch_target` and set `flush_if_once_`, `flush_id_once_` to insert `nop(flush)` bubbles in IF/ID and ID/EX.
- No prediction yet: IF always fetches sequentially (PC+4). Control hazard penalty equals resolve latency (2 cycles typical).

Prediction plug points:
- IF: query predictor(+BTB/RAS) with fetch PC to get (pred_taken, pred_target). If predicted taken with target available: set next PC accordingly; otherwise fetch PC+4.
- EX: when actual outcome and target known, train predictor and BTB/RAS; if prediction differs, assert flush flags (already supported) and set PC to correct target.


## 2) Predictor Interface and Plumbing
Define a minimal interface and wire into `RV5SVM`:

```cpp
struct PredResult { bool taken; uint64_t target; bool has_target; };

class IBranchPredictor {
public:
  virtual ~IBranchPredictor() = default;
  virtual PredResult predict(uint64_t pc) = 0;                // IF stage
  virtual void update(uint64_t pc, bool taken,
                      uint64_t actual_target, bool is_call,
                      bool is_return, bool is_indirect) = 0;   // EX stage
};
```

In `RV5SVM`:
- Add `std::unique_ptr<IBranchPredictor> bp_;` plus a small `BTB` and `RAS` (can be inside predictor or separate components).
- In `stageIF`: consult predictor; set next PC accordingly; also latch predicted-taken bit and predicted-target in IF/ID (for later compare in EX).
- In `stageEX`: compare predicted vs actual; on mismatch, count misprediction, flush, and redirect PC; then call `bp_->update(...)`.

Minimal fields to add to pipeline regs:
- In `IFID` or `IDEX`: `bool was_predicted; bool pred_taken; uint64_t pred_target;`


## 3) BTB and RAS Basics
- BTB: tag-indexed table mapping PC -> target and type (direct branch, call, return, indirect). Use set-associative or direct-mapped; 256–1K entries is fine.
- RAS: small stack (8–32 entries) for return-address prediction on CALL/RET. Push return (pc+4) on JAL with rd=x1 (or link reg), pop on JALR with rs1=x1 (convention dependent). For generic RISC-V, JAL to x1 is call; JALR to x0/x1 with proper funct3 is return when rs1=x1.


## 4) Predictors to Implement

### 4.1 Static Predictors
- Always Not Taken (ANT) and Always Taken (AT): baselines.
- Backward-Taken, Forward-Not-Taken (BTFNT): if branch offset negative → predict taken; else not.
- Pros: trivial; set ground truth for speedups.

Implementation: single `predict` rule; `update` is no-op (or count stats).

---

### 4.2 1-bit and 2-bit Saturating Counters (Bimodal)
- Table indexed by PC bits (e.g., 1K–4K entries). Each entry is a 1-bit or 2-bit counter.
- Predict taken if MSB (2-bit) or bit==1 (1-bit). Update by increment/decrement (saturate).
- Pros: tiny, fast; good baseline.

Data:
- `std::vector<uint8_t> pht; // pattern history table`
- Index: `(pc >> 2) & (N-1)` where `N` is table size.

---

### 4.3 GShare
- XOR global branch history register (GBHR) with PC index to query a 2-bit counter table.
- Data: `uint32_t ghr` (k bits), `pht[2^k]` of 2-bit counters.
- Predict: `pht[ (pc>>2 ^ ghr) & mask ].MSB`
- Update: shift ghr with actual outcome; bump counter.
- Pros: classic and strong for moderate hardware.

---

### 4.4 Local History + PHT (Two-Level Local)
- Per-branch local history table (LHT) records recent outcomes per PC; that history indexes a local PHT of 2-bit counters.
- Data: `LHT[PC_idx]` (k-bit history), `LPHT[2^k]`.
- Pros: captures per-branch patterns missed by global.

---

### 4.5 Tournament (Hybrid: Local vs Global)
- Combine Local and GShare with a chooser table of 2-bit counters indicating which to trust at each PC.
- Predict: get local_pred and global_pred; chooser counter MSB selects; update chooser toward the one that was correct.
- Pros: strong general-purpose baseline (Alpha 21264 style).

---

### 4.6 Loop Predictor (Lightweight)
- Tracks back-to-back loop branches with a trip count model: when iterations stabilize, predict not-taken at loop exit.
- Data per PC: last_iter_count, current_count, confidence.
- Use alongside bimodal/gshare; only override when confident.
- Pros: fixes gshare/local weaknesses on regular loops with minimal storage.

---

### 4.7 TAGE-lite (Ambitious but doable)
- TAGE uses multiple tagged tables at increasing history lengths with a base bimodal table. Select longest matching provider; use alternate on low confidence.
- Implement a "lite" variant: 3–4 tagged tables, small tags (6–8 bits), small counters (2-bit), and a 10–20 bit folded global history.
- Pros: state of the art tradeoff, widely used in CBP competitions.
- Reference: Seznec & Michaud (TAGE)

---

### 4.8 Perceptron Predictor (Compact NN)
- Maintain a small vector of weights per index; dot product of GHR bits with weights, sign gives prediction.
- Use a training threshold (e.g., `theta ≈ 1.93 * history_length + 14`).
- Pros: learns linearly separable long histories; elegant.
- Caveat: integer MACs per IF; keep table small (e.g., 64–256 entries, 16–32 history length) for simulator speed.
- Reference: Jiménez & Lin (HPCA 2001).


## 5) Target Prediction for Jumps and Indirects
- For conditional branches: BTB stores fall-through+target; predictor gives direction.
- JAL: compute target immediately from immediate; also update BTB for future fetch.
- JALR/returns (indirect): use RAS; for general JALR targets, a small indirect BTB (IT-TAGE is complex; skip). Fallback to no-predict for rare indirects.


## 6) Integration Steps

1) Add predictor config:
   - Extend `PredictorKind` in `rv5s_vm.h` to: `None, Static, OneBit, TwoBit, GShare, Local, Tournament, Loop, TageLite, Perceptron`.
   - Add CLI/config to select predictor, sizes, and RAS/BTB sizes.

2) Pipeline regs:
   - In `IFID` (or `IDEX`), add:
     - `bool was_predicted{false}; bool pred_taken{false}; uint64_t pred_target{0};`

3) stageIF changes:
   - Query BTB/RAS + predictor:
     - If BTB hit or JAL/JALR known target and predictor says taken → set next PC to target; else → PC+4.
   - Latch predicted bits/target in IF/ID.

4) stageEX changes:
   - Compute actual outcome/target as you do.
   - Compare with latched predicted; if mismatch → `mispredictions_++`, set `flush_if_once_ = flush_id_once_ = true`, and correct `program_counter_`.
   - Call `bp_->update(pc, taken, target, is_call, is_return, is_indirect)`.

5) BTB and RAS:
   - BTB update on taken control-flow with resolved target.
   - RAS push on call (JAL rd=x1), pop on return (JALR rs1=x1). Guard underflow/overflow.

6) Stats and Validation:
   - Track: `mispredictions_`, `branches_`, `mpki = mispredictions / (instructions/1000)`, CPI deltas.
   - Extend `validate_test.py` and expectations to include branch counts and mispred.


## 7) Data Structures (suggested defaults)
- BTB: 512 entries, 2-way set associative, LRU; tag = pc upper bits, idx = pc>>2 & (sets-1).
- RAS: depth 16.
- Bimodal: 2K-entry 2-bit counters.
- GShare: GHR=10 bits; PHT=1K 2-bit.
- Local: LHT=1K entries, 8-bit history; LPHT=256 entries of 2-bit.
- Tournament: chooser=2K 2-bit.
- Loop: 64-entry table keyed by PC, track iter/end, simple confidence.
- TAGE-lite: 1 base bimodal (1K), plus 3 tagged tables: sizes 512/256/128, tag 7 bits, ctr 2-bit, u-bit 1-bit; history lengths ~4/12/32 (folded histories).
- Perceptron: 128 perceptrons, hist_len=20, weights 8-bit signed.


## 8) Pseudocode Samples

### Bimodal 2-bit
```c++
struct Bimodal : IBranchPredictor {
  std::vector<uint8_t> pht; // 2-bit counters
  explicit Bimodal(size_t n) : pht(n, 1) {} // weakly not taken
  size_t idx(uint64_t pc) const { return (pc >> 2) & (pht.size()-1); }
  PredResult predict(uint64_t pc) override {
    auto c = pht[idx(pc)];
    return { (c>>1), 0, false };
  }
  void update(uint64_t pc, bool taken, uint64_t, bool, bool, bool) override {
    auto &c = pht[idx(pc)];
    if (taken && c < 3) ++c; else if (!taken && c > 0) --c;
  }
};
```

### GShare
```c++
struct GShare : IBranchPredictor {
  uint32_t ghr=0; uint32_t mask; std::vector<uint8_t> pht;
  GShare(size_t n, unsigned hist) : mask(n-1), pht(n,1) {}
  size_t idx(uint64_t pc) const { return ((pc>>2) ^ ghr) & mask; }
  PredResult predict(uint64_t pc) override { return { pht[idx(pc)]>>1, 0, false }; }
  void update(uint64_t pc, bool taken, uint64_t, bool, bool, bool) override {
    auto &c = pht[idx(pc)]; if (taken && c<3) ++c; else if (!taken && c>0) --c;
    ghr = ((ghr<<1) | (taken?1:0)) & mask;
  }
};
```

### Tournament (Local vs GShare)
- Maintain `chooser[PC_idx]` 2-bit; if preds differ, move chooser toward the correct one.


## 9) Testing and Evaluation
- Add micro-benchmarks: tight loops, nested ifs, mixed branches, calls/returns, indirects.
- Use your existing `run_tests_validated.sh`; extend it to log `VM_BP stats` line with branches, mispredictions, MPKI, predictor kind.
- Compare CPI and IPC across predictors and table sizes.


## 10) References (Recommended Reading)
- Yeh & Patt, "Two-Level Adaptive Branch Prediction" (ISCA 1991)
- McFarling, "Combining Branch Predictors" (WRL TN-36, 1993)
- Jiménez & Lin, "Dynamic Branch Prediction with Perceptrons" (HPCA 2001)
- Seznec & Michaud, "A case for (partially) tagged geometric history length" (JILP 2006) — TAGE
- Seznec, "A new case for the TAGE predictor" (JILP 2011)
- CBP (Championship Branch Prediction) materials

---

## 11) Implementation Roadmap
- Phase 1 (1–2 days): Bimodal (1/2-bit), GShare, BTB, RAS; wire stats.
- Phase 2 (1–2 days): Local + Tournament; Loop predictor overlay.
- Phase 3 (2–3 days): TAGE-lite; optional Perceptron.
- Phase 4: Parameter sweeps and benchmarking; produce tables/plots (CPI, MPKI).

Appendix: For RISC-V specifics, treat JAL (direct call) and conditional branches as predictable with BTB; JALR (returns) use RAS. Ensure mispredict recovery via your existing `flush_if_once_`/`flush_id_once_` and PC redirect in EX.
