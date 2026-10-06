# Cadence

**Deterministic Real-Time Scheduling & Priority Inversion Simulator**

Cadence is a single-threaded, deterministic simulator of a fixed-priority
preemptive scheduler (one CPU, integer ticks). It exists to reproduce
**priority inversion** exactly, apply **priority inheritance**, and test
that the fix behaves correctly.

It is a simulator, not an RTOS. Simulated tasks do not use OS threads; the
same scenario always produces the same trace.

## Status

Work in progress. See the roadmap below for what exists today.

## Build and test (macOS / Linux)

```bash
cmake -S . -B build
cmake --build build
./build/cadence
ctest --test-dir build --output-on-failure
```

With sanitizers:

```bash
cmake -S . -B build-san -DCADENCE_SANITIZE=ON
cmake --build build-san
./build-san/cadence_tests
```

## Layout

```
include/cadence/   public headers (core model and engine)
src/               implementation and the CLI entry point
tests/             GoogleTest unit and scenario tests
scenarios/         scenario files (later)
SEMANTICS.md       the normative specification
```

## Specification

All behavior is defined in [SEMANTICS.md](SEMANTICS.md). If the code and the
spec disagree, one of them is wrong and the spec is updated first.

## Golden scenario S1 (expected, from the spec)

These numbers are derived by hand in SEMANTICS.md Appendix A. They are the
target for the engine and are **not yet reproduced by code**.

| | No inheritance | Priority inheritance |
|---|---|---|
| High-priority task response | 7 | 4 |
| High-priority deadline (abs 9) | missed by 1 | met |
| High-priority inversion ticks | 3 | 0 |

## Limitations (by design)

- One CPU. No multicore.
- Zero-cost context switches; no interrupts or timer jitter.
- Preemption only at tick boundaries.
- Straight-line task programs (no branches or loops).
- One-shot tasks only in v0.1.

## Roadmap

- [x] 1. Task model, states, operations
- [ ] 2. Ready queue
- [ ] 3. Scheduler selection
- [ ] 4. Tick engine
- [ ] 5. Preemption
- [ ] 6. Mutex and locking
- [ ] 7. Blocking
- [ ] 8. Priority inversion
- [ ] 9. Priority inheritance
- [ ] 10. Multiple waiters, nested locks, propagation
- [ ] 11. Deadlock detection
- [ ] 12. Metrics and event trace
- [ ] 13. Invariant checker
- [ ] 14. Golden S1 scenarios and CLI
- [ ] 15. JSON scenario loader
- [ ] 16. Visualizer