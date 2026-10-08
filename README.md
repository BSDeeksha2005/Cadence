# Cadence

**Deterministic Real-Time Scheduling & Priority Inversion Simulator**

Cadence is a single-threaded, deterministic simulator of fixed-priority
preemptive scheduling on one CPU. It models mutex blocking, priority
inversion, Priority Inheritance Protocol (PIP), deadlock detection, a
canonical event trace, tick snapshots, and post-hoc scheduling metrics.

It is a simulator, not an RTOS. Simulated tasks do not use OS threads.

## v0.1 status

The normative v0.1 engine is implemented through:

- task/release/deadline semantics
- deterministic FIFO ready queues
- fixed-priority preemption
- mutex ownership, direct handoff, and blocking
- priority inversion under `NONE`
- priority inheritance under `PIP`
- chained inheritance and multiple waiters
- dynamic deadlock detection
- deterministic events and per-tick snapshots
- post-hoc metrics
- invariant checking
- golden S1 scenario
- determinism hash and declaration-order tests
- a small CLI for comparing `NONE` and `PIP`

`SEMANTICS.md` remains the normative source.

## Build and test

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure