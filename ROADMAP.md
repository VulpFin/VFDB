# VFDB Roadmap

**Project**: VFDB — A tiny embedded database experiment (C core + Python facade)  
**License**: AGPL-3.0  
**Current Version**: 0.1.0 (MVP+)

---

## Vision

VFDB aims to be a **small, understandable, embeddable database** that is useful for local/project data while remaining significantly simpler than SQLite.

**Long-term goals**:
- Usable as a lightweight alternative for many embedded use cases
- Clean, maintainable codebase that is easy for humans *and* AI agents to work on
- Gradually expand SQL support without becoming a second SQLite

---

## Current State Assessment (as of 2025)

### What Works Well
- Storage layer (`heap.c` + `catalog.c`) — append-only heaps with tombstone deletes/updates
- Basic DDL + DML (`CREATE TABLE`, `INSERT`, `SELECT *`, simple `WHERE`, `UPDATE`, `DELETE`, `LIMIT`)
- Transaction support via statement replay buffer
- Python facade with reasonable ergonomics
- Type system foundation (`vf_type.c`)
- Logging infrastructure

### Major Technical Debt
| Area                    | Status                              | Severity | Notes |
|-------------------------|-------------------------------------|----------|-------|
| **Architecture**        | Very poor                           | Critical | ~46k lines of logic in `src/vfdb.c` |
| **Parser**              | Hand-written, monolithic            | High     | All parsing lives in `vfdb.c` |
| **Query Executor**      | Monolithic                          | High     | `exec.c` is a stub |
| **Modularization**      | Incomplete                          | High     | Most core modules are stubs |
| **Indexing**            | Not implemented                     | High     | `bptree.c` is a stub |
| **Compaction**          | None                                | High     | File grows forever on UPDATE/DELETE |
| **Durability**          | Weak                                | Medium   | Limited crash safety |
| **Testing**             | Minimal                             | Medium   | Very light test coverage |
| **Expression Engine**   | Extremely limited                   | High     | Single-predicate WHERE only |
| **NULL Support**        | Not implemented                     | Medium   | Explicitly unsupported |

**Biggest Problem**: The project has outgrown its "everything in one file" architecture. This is the #1 blocker for sustainable progress.

---

## Guiding Principles

1. **Modularize aggressively** — Move logic out of `vfdb.c` into proper modules.
2. **Build an AST** — Long-term, we need a proper expression + statement AST.
3. **Keep it understandable** — Prefer clarity over micro-optimizations.
4. **Test as we go** — Every significant feature should have tests.
5. **Agent-friendly** — Structure work so future agents (or humans) can continue easily.
6. **Incremental value** — Deliver usable improvements in small steps.

---

## Phased Roadmap

### Phase 0: Foundation & Hygiene (Current State)
**Goal**: Make the project easier to work on.

- [ ] Create this roadmap and keep it updated
- [ ] Improve documentation (architecture overview, storage format)
- [ ] Set up basic CI (build + test on Linux)
- [ ] Increase test coverage for existing functionality
- [ ] Add `make` / improved build targets

**Success Criteria**: New contributors (human or AI) can understand the project structure within 30 minutes.

---

### Phase 1: Modularization & Architecture Cleanup (Highest Priority)
**Goal**: Break the monolith.

This is the most important phase.

**Key Tasks**:
- [ ] Move predicate / expression parsing into `src/parser.c`
- [ ] Create a minimal AST for expressions (start simple)
- [ ] Move execution logic into `src/exec.c`
- [ ] Extract transaction handling into `src/tx.c`
- [ ] Clean up `vfdb.c` so it becomes mostly a coordinator + public API
- [ ] Improve error handling and error codes

**Target**: `vfdb.c` should be reduced significantly (ideally under 15-20k lines).

---

### Phase 2: Query Engine Foundation
**Goal**: Make the query engine properly extensible.

- [ ] Implement a real expression parser (support `AND`, `OR`, parentheses)
- [ ] Add basic arithmetic and function call support in expressions
- [ ] Improve `WHERE` clause capabilities significantly
- [ ] Add support for `SELECT` column expressions (not just `*`)
- [ ] Wire up basic infrastructure for indexes (even if not used yet)

---

### Phase 3: Indexing & Query Planning
**Goal**: Stop doing full table scans for everything.

- [ ] Implement B+Tree in `bptree.c` properly
- [ ] Add `CREATE INDEX` / `DROP INDEX` support
- [ ] Build a very simple query planner / optimizer
- [ ] Use indexes for equality and range predicates

---

### Phase 4: Storage Maturity
**Goal**: Make the database safe for real workloads.

- [ ] Implement **compaction** / `VACUUM` (critical)
- [ ] Improve durability (better flushing, atomic metadata)
- [ ] Consider adding a simple WAL or checkpoint mechanism
- [ ] Add support for `NULL` values
- [ ] Add basic statistics / `ANALYZE`

---

### Phase 5: Feature Completeness
- [ ] `DROP TABLE`
- [ ] `ORDER BY`
- [ ] Basic `JOIN` support (inner joins first)
- [ ] Subqueries (limited)
- [ ] Better type system (proper date/time handling, etc.)
- [ ] User-defined functions (Python UDFs already partially exist)

---

### Phase 6: Polish & Production Readiness
- [ ] Comprehensive test suite
- [ ] Performance benchmarks
- [ ] Better error messages
- [ ] Documentation website / examples
- [ ] Packaging improvements (wheels, etc.)
- [ ] Consider license change if broader adoption is desired

---

## Priority Matrix (Recommended Focus Order)

| Priority | Area                        | Effort | Impact | Recommendation |
|----------|-----------------------------|--------|--------|----------------|
| **P0**   | Modularization (Phase 1)    | High   | Very High | Do this first |
| **P1**   | Compaction / VACUUM         | Medium | Very High | Critical for usability |
| **P2**   | Expression engine + AND/OR  | Medium | High   | Big usability win |
| **P3**   | Indexing (Phase 3)          | High   | High   | Long-term performance |
| **P4**   | Durability improvements     | Medium | High   | - |
| **P5**   | Testing & CI                | Medium | Medium | Ongoing |

---

## How to Work on This Roadmap (For AI Agents & Humans)

### Recommended Workflow

1. **Always start by reading**:
   - `ROADMAP.md` (this file)
   - `README.md`
   - `GRAMMAR.md`
   - `include/vfdb.h`

2. **Understand the current architecture** before making changes.

3. **Work in small, focused commits**:
   - One logical change per commit
   - Prefer refactor commits before big feature commits

4. **Update this roadmap** when you complete or reprioritize items.

5. **Add tests** for any new or changed behavior.

6. **Document decisions** in commit messages or a `docs/decisions/` folder.

### Good First Tasks for Agents

- Refactor a specific parsing function out of `vfdb.c` into `parser.c`
- Add a new test case for existing WHERE behavior
- Implement a basic `VACUUM` command
- Create an architecture document

---

## Risks & Open Questions

- How much SQL complexity do we actually want to support?
- Should we eventually move to a page-based storage model (like most real DBs)?
- Is the current tombstone + heap approach sustainable long-term?
- License implications for adoption

---

## How to Update This Document

When working on VFDB:
- Mark completed items with `[x]`
- Add new items as they are discovered
- Update priority when circumstances change
- Keep the "Current State Assessment" reasonably fresh

---

**Last Updated**: 2025  
**Maintained by**: Project contributors (including AI agents)

---

*This roadmap exists so that work can continue effectively across multiple sessions and multiple agents.*