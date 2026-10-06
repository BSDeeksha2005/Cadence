# Instructions for AI coding agents working on Cadence

Cadence is a deterministic real-time scheduler simulator in C++17.
The owner is learning C++ and must be able to explain every line of the
kernel. Prefer clarity over cleverness.

## Source of truth
- `SEMANTICS.md` is normative. Do not change any semantic rule.
- If an implementation seems to require changing a rule, STOP and report
  which section conflicts and why. Do not edit semantics silently.

## Scope discipline
- Implement ONLY the step the owner asked for. Do not start later steps.
- Build order: Task model, ReadyQueue, scheduler selection, tick engine,
  preemption, mutex, blocking, inversion, inheritance, propagation,
  deadlock detection, metrics/trace, invariants, S1 + CLI, JSON, UI.
- Small diffs. Do not refactor unrelated files.

## Hard constraints for `include/` and `src/` core code
- C++17, macOS AppleClang compatible.
- NO std::thread, std::mutex, std::condition_variable, std::atomic,
  std::chrono, <random>, or any OS threading in the core.
- NO JSON, file I/O or HTTP in the core. Those are adapters.
- No unordered containers and no ordering by pointer address in any
  decision path. All tie-breaks must be explicit.
- No floating point in simulation logic.

## Style (kernel code)
- Plain structs, enum class, std::vector, indices/ids instead of pointers.
- No inheritance, templates, std::variant, or smart-pointer graphs unless
  the owner asks.
- Short functions. Comments say WHY, and reference SEMANTICS.md sections.
- No raw `new`/`delete`.

## Testing
- Every change ships with GoogleTest tests.
- Expected values in scenario tests are derived by hand from SEMANTICS.md,
  never copied from the program's own output.
- Include negative/edge cases.
- Must pass: `cmake -S . -B build && cmake --build build &&
  ctest --test-dir build --output-on-failure`
- Also run once with `-DCADENCE_SANITIZE=ON` in `build-san`.
- Warnings are errors in the core (`-Wall -Wextra -Wpedantic -Werror`).

## When finished
Report: files changed, which SEMANTICS.md sections were implemented, tests
added, and anything uncertain. Do not claim tests pass unless you ran them.