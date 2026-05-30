# Phase 1: Modularization Breakdown

**Goal**: Reduce the size and complexity of `src/vfdb.c` by extracting logical components into dedicated modules. This is a prerequisite for sustainable development of the query engine.

**Current Problem**:
- `src/vfdb.c` is ~46,000 lines and contains almost everything (parsing, statement execution, DML application, predicates, etc.).
- `parser.c` and `exec.c` are empty stubs.
- Adding new SQL features (better WHERE, expressions, JOINs, etc.) will become increasingly painful without refactoring.

**Success Criteria for Phase 1**:
- `vfdb.c` reduced to a reasonable size (ideally < 15k lines).
- Clear separation between **parsing**, **execution**, and **storage application**.
- New modules have their own headers.
- Existing functionality passes all current tests/examples.
- Build still works cleanly.

---

## Guiding Principles for This Phase

1. **Small, reviewable commits** — Prefer many small commits over large ones.
2. **Preserve behavior** — Do not change SQL semantics during extraction.
3. **Make incremental progress visible** — Each commit should improve the situation.
4. **Keep the Python extension working** at every step.
5. **Document decisions** in commit messages.

---

## Recommended Extraction Order

We should extract in this order (least risky → more risky):

### Step 1: Create Internal Infrastructure
- Create `src/vfdb_internal.h`
- Move shared structs and forward declarations here
- Create `src/util.c` / `src/util.h` (string helpers, memory, etc.)

### Step 2: Extract Predicate System
- Create `src/predicate.h` and `src/predicate.c`
- Move:
  - `Predicate` struct
  - `PredOp` enum
  - `parse_where_pred()`
  - `pred_match_row()`
  - `row_matches_pred()`
  - Comparison functions (`int_cmp`, `real_cmp`, `str_cmp`)

**Why first?** Predicates are relatively self-contained and used in multiple places.

### Step 3: Extract Low-Level Parsing Helpers
- Move into `src/parser.c` / `src/parser.h`:
  - `skip_ws()`
  - `skip_opt_semicolon()`
  - `parse_ident()`
  - `parse_op()`
  - `parse_quoted()`
  - `parse_type_decl()`
  - `parse_value_for_type()`
  - `parse_int_literal()`, `parse_real_literal()`, `parse_bool_literal()`, `parse_blob_literal()`
  - `try_parse_select_const_int()`

**Target**: Make `parser.c` responsible for turning text into structured data.

### Step 4: Extract Statement Representation
- Improve `VFDBStmt` handling.
- Consider creating `src/statement.h` and moving statement-related logic.
- Begin separating "what the statement is" from "how it executes".

### Step 5: Extract DML Application Logic
- Move the `apply_*` functions out of `vfdb.c`:
  - `apply_insert()`
  - `apply_delete_where_eq()`
  - `apply_update_set_eq_where_eq()`
- These could go into a new `src/dml.c` or into `exec.c`.

### Step 6: Refactor `vfdb_prepare()` and `vfdb_step()`
- Break the giant `vfdb_prepare()` into smaller functions.
- Move statement execution logic toward `exec.c`.

### Step 7: Cleanup & Final Restructuring
- Remove dead code / duplication.
- Update `CMakeLists.txt` and any other build files.
- Improve internal headers.
- Add basic smoke tests for the new modules.

---

## Detailed Task Breakdown (Actionable)

### Task 1.1 — Internal Header
- [ ] Create `src/vfdb_internal.h`
- [ ] Move `TxState`, `TxBuf`, and related functions here
- [ ] Move `StmtKind` enum here
- [ ] Move `VFDB` struct definition here (keep opaque in public header)

### Task 1.2 — Predicate Module (Highest Value Early Win)
- [ ] Create `include/predicate.h` (or keep under `src/`)
- [ ] Create `src/predicate.c`
- [ ] Move `Predicate` struct + `PredOp`
- [ ] Move all comparison functions
- [ ] Move `parse_where_pred()`
- [ ] Move `pred_match_row()` and `row_matches_pred()`
- [ ] Update `vfdb.c` to include the new header

### Task 1.3 — Parser Helpers Module
- [ ] Flesh out `src/parser.h`
- [ ] Move all the small parsing functions listed in Step 3
- [ ] Add proper documentation comments
- [ ] Make the parser functions take clearer ownership of output

### Task 1.4 — DML Application Layer
- [ ] Create `src/dml.h` + `src/dml.c`
- [ ] Move the three `apply_*` functions
- [ ] Make them take clearer parameters (reduce coupling to `VFDB`)

### Task 1.5 — Statement Execution
- [ ] Start moving logic from `vfdb_step()` into `exec.c`
- [ ] Create better abstraction for different statement kinds

---

## Risks & Mitigations

| Risk                              | Mitigation |
|-----------------------------------|----------|
| Breaking the build                | Compile after every significant change |
| Introducing bugs in parsing       | Keep old code until new code is proven |
| Transaction logic getting messy   | Handle TxBuf carefully during extraction |
| Python extension breaks           | Test `python -c "import vfdb"` frequently |

---

## Suggested Commit Strategy

We should aim for commits like:

1. `chore: add docs/PHASE1_MODULARIZATION_BREAKDOWN.md`
2. `refactor: create src/vfdb_internal.h skeleton`
3. `refactor: extract Predicate system into predicate.c/h`
4. `refactor: move low-level parsing helpers to parser.c`
5. `refactor: extract apply_* DML functions to dml.c`
6. etc.

---

## After Phase 1 (What Phase 2 Should Look Like)

Once this phase is complete, it becomes much more reasonable to work on:

- Proper expression parser (AND/OR, arithmetic, functions)
- Real AST instead of ad-hoc parsing
- Better error reporting
- Adding indexes via bptree

---

## How to Work on This (For Agents)

1. Always read the relevant section of `vfdb.c` before moving code.
2. Make changes in small, focused edits.
3. After each logical extraction, run the build + basic Python smoke test.
4. Update this document with progress (mark tasks complete).
5. Push frequently with clear commit messages.

---

**Last Updated**: 2026-05-30  
**Status**: Planning Complete — Ready to Begin Execution

Would you like to start with **Task 1.2 (Predicate Module)** as the first real extraction? This is generally the cleanest place to begin.