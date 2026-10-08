# Cadence

Cadence is a deterministic simulator for fixed-priority, preemptive scheduling
on one CPU. It demonstrates mutex blocking and priority inversion, and compares
the `NONE` protocol with Priority Inheritance (PIP). It is a simulator, not an
RTOS, and makes no production-certification claim.

## What it demonstrates

A high-priority task can be blocked when a low-priority task owns a mutex. With
`NONE`, a medium-priority task can preempt the mutex owner and indirectly delay
the high-priority task: this is priority inversion. With PIP, the owner inherits
the highest effective priority of waiters on its held mutexes. The boost
propagates through chains of blocked owners and is recomputed from held locks
and current waiters on unlock.

Simulation uses integer ticks, one CPU, fixed priorities (larger integer means
higher priority), and explicit event ordering. The engine has no real-time OS
threads. JSON, HTTP, and frontend code are outside the C++ simulation core.

## Architecture

- `include/cadence` and `src`: C++17 task model, ready queue, engine, mutexes,
  NONE/PIP protocols, trace, metrics, validation, and invariants.
- `include/cadence/adapter` and `src/scenario_json.cpp`: a small parser adapter
  that validates scenario JSON before constructing core task objects.
- `tests`: GoogleTest coverage, including hand-derived S1 golden traces.
- `frontend`: React visualizer backed by the checked-in C++ WebAssembly build.
- `wasm/bridge.cpp`: Emscripten bridge source.

The browser demo currently runs the normative S1 scenario. S2 and custom JSON
scenarios are supported by the native CLI, not by the browser UI.

## Build, run, and test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/cadence --both
./build/cadence --scenario scenarios/S1.json --protocol NONE
./build/cadence --scenario scenarios/S1.json --protocol PIP
./build/cadence --scenario scenarios/S2.json --protocol PIP
```

The CLI prints both hand-checkable S1 timelines by default; use `--none`,
`--pip`, or `--protocol NONE|PIP|BOTH` to select a protocol. `--scenario FILE`
loads a JSON scenario using the schema in `SEMANTICS.md` Appendix A. The loader
supports integer simulation values and standard JSON string escapes; scenario
validation runs before the engine starts.

The repository includes S1 and S2 scenario files. S2 demonstrates chained
inheritance: H waits on B owned by M, M waits on A owned by L, while X is the
unrelated medium-priority task. The native CLI also accepts additional valid
JSON scenarios.

Sanitizer verification:

```sh
cmake -S . -B build-san -DCADENCE_SANITIZE=ON
cmake --build build-san
ctest --test-dir build-san --output-on-failure
```

Release build:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
```

Frontend development and production build:

```sh
cd frontend
npm ci
npm run typecheck
npm run build
npm run dev
```

## S1 demo

The scenario is defined in [`SEMANTICS.md`](SEMANTICS.md), Appendix A. Under
`NONE`, its task timeline is `L L M H M M M L L H L`; under `PIP`, it is
`L L M H L L H M M M L`. The UI shows both runs, their timeline, state and
mutex snapshots, event log, and metrics.

S2 has IDs L=0, M=1, H=2, X=3. Its hand-derived timelines are
`0 0 3 3 0 0 1 2 2 1 0` under NONE and
`0 0 0 0 1 2 2 1 3 3 0` under PIP. H misses its deadline under NONE and meets
it under PIP.

## Deploy the visualizer

Build and run the static frontend container from the repository root:

```sh
docker build -t cadence-demo .
docker run --rm -p 8080:80 cadence-demo
```

Open `http://localhost:8080`. The UI runs the C++ WebAssembly engine in the
browser; the container serves static assets and has no database or backend.

## Tests

The suite covers scheduling, FIFO and preemption rules, mutex handoff and
blocking, PIP propagation, deadlock detection, trace/metrics, invariants,
determinism, S1 golden results, and S2 chain-inheritance timelines and metrics.
Build and sanitizer commands are above.

## Limitations

- One-shot tasks, one CPU, integer ticks, and protocols `NONE` and `PIP` only.
- The browser demo runs S1 only; it does not yet load custom JSON scenarios.
- No periodic tasks, ICPP, round robin, multicore scheduling, or real OS task
  execution.
- No claim of RTOS behavior, certification, or production readiness.
