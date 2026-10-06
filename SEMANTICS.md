# CADENCE — Semantics (v0.1 draft)

*Deterministic Real-Time Scheduling & Priority Inversion Simulator*

This document is **normative**. If the code disagrees with it, either the code is wrong or this document must be changed first (and the change recorded). Every rule here must be covered by at least one test.

Scope of v0.1: one CPU, one-shot tasks (one job per task), mutex resources, protocols `NONE` and `PIP`. Periodic tasks, ICPP and round-robin are reserved in §18.

---

## 1. Priority convention

- Priorities are integers in **[1, 32]**. **Larger number = higher priority.** 0 is reserved (idle, never assigned).
- `base(T)` is fixed for the life of a task. `eff(T)` is the effective priority (§10).
- The code wraps priority in a strong type with `higherThan(a, b)`. No raw `<` or `>` on priorities outside that type.
- Scheduling decisions use **effective** priority only.

## 2. Time model

- `Tick` is a signed 64-bit integer. Time is discrete; there is no fractional time and no floating point anywhere.
- **Instant `t`** is a tick boundary. **Tick `t`** is the interval `[t, t+1)`.
- There is one CPU. During tick `t` at most one task executes one unit of `COMPUTE`, or the CPU is idle.
- Only one thing advances time: executing a tick (step B6 below). All other operations are **zero-time** and happen *at an instant*.
- The simulation starts at `t = 0`. A run has a mandatory `horizon`. Boundary processing runs at every `t` from 0 up to and including the end instant `t_end`; no tick is executed at `t_end`.
- `t_end` is the first instant at which: (a) all tasks are COMPLETED, or (b) `t == horizon`, or (c) a deadlock was detected (§10.8).
- Completions that happen exactly at `t_end` are recorded.

### 2.1 The boundary procedure (the heart of the engine)

At every instant `t`, the engine runs exactly these steps, in this order:

| Step | Name | What happens |
|---|---|---|
| B1 | Continue | `resolve()` — the currently running task (if any) executes its pending zero-time ops |
| B2 | Activate | Time-driven events at `t`: wake sleepers and release NEW tasks, in ascending TaskId order |
| B3 | Resolve | `resolve()` again, now that new tasks may be READY |
| B4 | Deadline check | For each job with `abs_deadline == t` that is not COMPLETED, emit `DEADLINE_MISS` once |
| B5 | Record | Check invariants, record the tick row, check termination (stop here if `t == t_end`) |
| B6 | Execute | If a task is RUNNING, `remaining--`; if it reaches 0, `pc++` (the next op is *not* executed now). Otherwise the CPU is idle. Then `now = t+1` |

`resolve()` is defined as:

```
loop:
  R1  reschedule():
        c = READY task with highest eff (FIFO head of that level)
        if no RUNNING task and c exists:           dispatch c
        else if RUNNING and eff(c) > eff(running): preempt running (-> READY, head of its level); dispatch c
  R2  if no RUNNING task: stop
  R3  look at running task's next op:
        COMPUTE        -> stop (quiescent)
        LOCK/UNLOCK/SLEEP/END -> execute it (§4), then continue the loop
```

`resolve()` always terminates: every iteration either executes one op (programs are finite) or performs a dispatch/preempt that is followed by an op execution or a stop. The engine asserts an iteration bound (`2 * total_ops + n_tasks`) to catch bugs.

## 3. Task states

| State | Meaning |
|---|---|
| NEW | Not yet released (`release > now`) |
| READY | Runnable, waiting in exactly one ready queue |
| RUNNING | Holds the CPU. At most one task |
| BLOCKED | Waiting for a mutex (`blocked_on` is set); in exactly one mutex waiter list |
| SLEEPING | Waiting for a wake time (`wake_at` is set) |
| COMPLETED | Program finished; terminal in v0.1 |

### 3.1 Transitions (complete list)

| From | To | Cause | Queue effect |
|---|---|---|---|
| NEW | READY | release time reached (B2) | tail of level `eff` |
| READY | RUNNING | dispatch (R1) | removed from queue |
| RUNNING | READY | preempted by strictly higher eff (R1) | **head** of level `eff` |
| RUNNING | BLOCKED | `LOCK` on a mutex owned by another task | appended to mutex waiters |
| BLOCKED | READY | ownership handed to this task at `UNLOCK` | tail of level `eff` |
| RUNNING | SLEEPING | `SLEEP(n)` | `wake_at = now + n` |
| SLEEPING | READY | `wake_at == now` (B2) | tail of level `eff` |
| RUNNING | COMPLETED | END reached | none |

No other transitions exist. Any other transition observed is a bug (invariant I1).

**State during tick `t`** means the state recorded after B4 at instant `t`. Per-tick metrics (blocked ticks, inversion ticks) use this snapshot.

## 4. Task program semantics

A program is a **straight-line** list of ops (no branches, no loops). Each task has a program counter `pc` and, while in a COMPUTE, `remaining`.

| Op | Cost | Semantics |
|---|---|---|
| `COMPUTE(n)`, n ≥ 1 | n ticks | Consumes one tick of CPU per tick while RUNNING. Can be preempted between ticks, never within one. When `remaining` hits 0, `pc++` |
| `LOCK(m)` | 0 | If `m` is free: owner = task, add `m` to `held(task)`, `pc++`. If owned by another task: task → BLOCKED, appended to `waiters(m)`, `pc` stays at the LOCK until handoff. If owned by the task itself: invalid scenario (§16) |
| `UNLOCK(m)` | 0 | Task must own `m`. See §9 for the exact algorithm. `pc++` |
| `SLEEP(n)`, n ≥ 1 | 0 to start | Task → SLEEPING, `wake_at = now + n`. May be executed while holding mutexes (legal; this is blocking for any waiters) |
| END (implicit) | 0 | After the last op: task → COMPLETED, `completion = now`. The task must hold no mutexes |

A task's `pc` advances past `LOCK` at the moment of **handoff** (when it becomes the owner), not when it is later dispatched.

## 5. Scheduling policy

Fixed-priority, preemptive, single CPU, SCHED_FIFO-style, work-conserving.

1. **Selection:** the READY task with the highest `eff`; within a level, the head of the FIFO.
2. **Preemption:** only when a READY task has **strictly higher** `eff` than the RUNNING task. Equal priority never preempts.
3. **Preempted task** goes to the **head** of its level, so it resumes before its equal-priority peers.
4. **Newly READY tasks** (release, wake, unblock by handoff) go to the **tail** of their level.
5. **A READY task whose `eff` changes** is removed and placed at the **tail** of its new level.
6. **A RUNNING task whose `eff` drops** (e.g. after unlock) is treated like any other running task in R1: if a READY task is now strictly higher, it is preempted and goes to the head of its **new** level.
7. **No time slicing.** Equal-priority tasks run until they block, sleep, complete or are preempted.
8. **Work-conserving:** the CPU is idle only if no task is READY.

## 6. Scheduling points

`reschedule()` (R1) runs at the top of every iteration of `resolve()`. In effect, a scheduling point occurs: after any task becomes READY (release, wake, handoff); after the running task blocks, sleeps or completes; after any `UNLOCK`; after any change of effective priority; and when the CPU is idle and something becomes READY. There is no other scheduling point; in particular nothing happens in the middle of a tick.

## 7. Release semantics

- Each task has an integer `release ≥ 0`. At B2 of instant `t == release`, the task goes NEW → READY at the tail of its level.
- Absolute deadline `abs_deadline = release + D` (if the task has a relative deadline `D ≥ 1`).
- Wakes and releases at the same instant are processed **together, sorted by ascending TaskId**. TaskId is used only for this; it is never a priority tie-break.

## 8. Blocking semantics

- A task becomes BLOCKED only by `LOCK` on a mutex owned by another task. A task blocks on at most one mutex.
- A BLOCKED task is in no ready queue and consumes no CPU.
- There are no timed locks and no cancellation.
- `waiters(m)` is a vector in **arrival order**; stable erase preserves it, so position equals arrival order.
- `chain(B)` for a BLOCKED task B = `owner(blocked_on(B))`, then `owner(blocked_on(that task))` if it is itself BLOCKED, and so on until a non-BLOCKED owner. This is the **blocking chain**.

## 9. Mutex ownership semantics

- A mutex has `owner` (task or none) and `waiters`. Mutexes are non-recursive.
- **Direct handoff, no barging:** on unlock with waiters, ownership passes to a waiter immediately; the mutex is never observably free in between.

`UNLOCK(m)` by task `O`:

1. Validate `owner(m) == O` (else internal error; a static check prevents this in valid scenarios).
2. Remove `m` from `held(O)`.
3. If `waiters(m)` is empty: `owner(m) = none`.
   Otherwise choose `w` = waiter with highest `eff`, ties broken by earliest arrival; remove it from `waiters(m)`; `owner(m) = w`; add `m` to `held(w)`; `w`: BLOCKED → READY (tail), `pc(w)++`.
4. `recompute(O)` (O's `eff` may drop).
5. `recompute(w)` (the remaining waiters of `m` now wait on `w` and may raise it). Events emitted for each change.
6. Return to `resolve()`, whose R1 handles any preemption.

## 10. Priority inheritance

### 10.1 Protocols

A run uses one protocol: `NONE` (`eff = base` always) or `PIP`. `ICPP` is reserved (§18).

### 10.2 Definition of effective priority (PIP)

```
eff(T) = max( base(T),
              max over m in held(T), over w in waiters(m): eff(w) )
```

This is the only definition. The implementation **caches** `eff` in the task, updates it by propagation (§10.3), and the invariant checker recomputes it from scratch from this definition and compares (I7).

Never implement "save the old priority, restore on unlock". It is wrong for multiple mutexes and multiple waiters.

### 10.3 Propagation

```
propagate(t):
  loop:
    new = definition(t)          // uses cached eff of waiters
    if new == eff(t): stop
    eff(t) = new; emit PRIORITY_CHANGE
    if state(t) == READY:   reposition (remove, tail of new level)
    if state(t) == BLOCKED on m: t = owner(m); continue    // chain
    else: stop                   // RUNNING / SLEEPING: nothing more to do
```

It is called: when a task blocks (start from `owner(m)`), and from `UNLOCK` steps 4 and 5.

### 10.4 Multiple waiters and multiple held mutexes
The max in the definition covers both. A mutex with waiters at eff 2 and 3 gives the owner at least 3.

### 10.5 Nested locks
`held(T)` is a set; unlocks may happen in any order (non-LIFO is legal). Because `eff` is recomputed from the definition after each unlock, dropping one mutex leaves the boost from the others.

### 10.6 Release and restoration
Handled by §9 steps 4–5. The old owner drops to exactly the max over what it still holds, not to a saved value.

### 10.7 Chained inheritance
If H waits on R1 (owner M), M waits on R2 (owner L): when H blocks, `propagate(M)` raises M to eff(H); M is BLOCKED on R2, so propagation continues to L and raises it. When L unlocks R2, ownership goes to M (READY), then `recompute(M)` keeps M at H's priority until M unlocks R1.

### 10.8 Cycles and deadlock
- Each task blocks on at most one mutex, so the wait-for graph (task → owner of its mutex) has out-degree ≤ 1; it contains a cycle **iff** there is a deadlock.
- **PI neither creates cycles nor prevents deadlock.**
- On `LOCK` that would block, before propagation, walk the owner chain from `owner(m)`. If it reaches the requester: emit `DEADLOCK_DETECTED` (with the cycle), end the run with status `DEADLOCK`. This is independent of protocol.
- Consequence: a run that ends with unfinished tasks, no READY/RUNNING/SLEEPING/NEW tasks, and no detected deadlock is an internal error.

### 10.9 Waiter position
BLOCKED tasks are not in a priority queue. A blocked waiter's priority change only updates its `eff` field (and propagates); no repositioning is needed. The next owner is selected by scanning `waiters(m)` at unlock time.

## 11. Event ordering

Global order is the boundary procedure of §2.1. Within it:

- Events carry a strictly increasing global `seq` and the instant `time`.
- B1 effects (compute completion, unlock handoff, block, sleep, complete) come **before** B2 effects (wakes, releases).
- Consequence for same-instant FIFO order at one level: a task handed a mutex at B1 enters the tail **before** tasks woken or released at B2 of the same instant.
- Within B2: ascending TaskId.
- Within one `UNLOCK`: events in the order `UNLOCK`, `HANDOFF`, `PRIORITY_CHANGE`(old owner), `PRIORITY_CHANGE`(new owner), then R1's `PREEMPT` / `DISPATCH`.

Event kinds: `RELEASE, WAKE, DISPATCH, PREEMPT, SLEEP_START, LOCK_ACQUIRE, LOCK_BLOCK, UNLOCK, HANDOFF, PRIORITY_CHANGE, COMPLETE, DEADLINE_MISS, DEADLOCK_DETECTED`. Each has `{seq, time, kind, task, mutex?, a, b}` (e.g. `PRIORITY_CHANGE` has `a = old`, `b = new`).

**Tick row** (recorded at B5 for each tick): `time`, `running` (or idle), per task `{state, base, eff, blocked_on}`, per mutex `{owner, waiters[]}`.

## 12. Zero-time operations

- `LOCK`, `UNLOCK`, `SLEEP` and END take no time. `now` changes only in B6.
- `resolve()` runs to **quiescence**: a task keeps executing zero-time ops until it reaches a COMPUTE, blocks, sleeps or completes. After each op, R1 re-checks preemption (so an `UNLOCK` that readies a higher-priority task preempts the unlocker *before* its next op).
- **Continuation-first rule (design decision D1):** because B1 precedes B2, a task that just finished a COMPUTE executes its following zero-time ops *before* tasks released/woken at the same instant can preempt it.
  Effect: completion time = exactly the instant the last COMPUTE ends; a task finishing at `t` is never "delayed" by a release at `t`; a `LOCK` at instant `t` succeeds even if a higher-priority task is released at `t`.
  The alternative ("tick-first") would let the release preempt before the op runs. We choose continuation-first and test it explicitly.

## 13. Deadline semantics

- Deadlines are **observed, not scheduled on.** Scheduling is fixed priority.
- Job meets its deadline iff `completion ≤ abs_deadline`.
- B4 (after B3) emits `DEADLINE_MISS` at instant `abs_deadline` for an incomplete job, exactly once. The job keeps running (soft deadline). B4 must come after B3, so a job completing at exactly `abs_deadline` counts as meeting it.
- `lateness = max(0, completion − abs_deadline)`.
- If the run ends at `horizon` with a job incomplete and `abs_deadline > horizon`, the job is reported as "unfinished, deadline not reached".

## 14. Metrics

All metrics are computed **post-hoc from the trace** (events + tick rows) by a pure function, never inside the engine.

| Metric | Definition |
|---|---|
| `response` | `completion − release` (real-time definition) |
| `start_latency` | `first_dispatch − release` |
| `blocked_ticks(T)` | number of ticks `t` where state-during-tick `t` is BLOCKED |
| `inversion_ticks(B)` | number of ticks where B is BLOCKED, a task `R` is RUNNING, `R ∉ chain(B)`, and `eff(R) < eff(B)` |
| `legit_blocking(B)` | `blocked_ticks(B) − inversion_ticks(B)` |
| `deadline_misses`, `lateness` | per §13 |
| `preemptions` | count of RUNNING → READY transitions caused by R1 |
| `context_switches` | number of dispatches of task X where the last task that ran (ignoring idle) is not X; the first dispatch is not counted |
| `utilization` | busy ticks / ticks in `[0, t_end)` |

Notes:
- **Blocking vs inversion:** waiting for the owner of the mutex to finish its critical section is *blocking* (inherent). Inversion is when something *outside the blocking chain and lower in priority* runs meanwhile.
- If the CPU is idle while B is BLOCKED (e.g. the owner is SLEEPING), that is blocking, not inversion.
- Dropped as vanity or redundant: "turnaround", "average waiting time".

## 15. Determinism guarantees

1. The core is single-threaded and includes no `<thread>`, `<mutex>`, `<chrono>`, `<random>` or `<atomic>` (enforced by a CI grep).
2. No iteration over unordered containers and no ordering by pointer or address in any decision path.
3. All ordering is a total order with explicit tie-breaks (TaskId, FIFO position, arrival order).
4. No floating point. No wall clock. No locale-dependent formatting.
5. Any randomness (test generator) uses our own seeded PRNG implementation (not `std::` distributions).
6. Same scenario + same protocol + same horizon ⇒ byte-identical canonical trace. Tested by hashing the trace twice and by shuffling task declaration order (with explicit TaskIds).

## 16. Invalid scenarios

Validation is static and happens before the engine runs. Each error has a code and a field path; the engine never starts on an invalid scenario, and never invokes undefined behaviour.

`DUPLICATE_TASK_ID`, `DUPLICATE_MUTEX_ID`, `UNKNOWN_MUTEX`, `PRIORITY_OUT_OF_RANGE`, `NEGATIVE_RELEASE`, `NONPOSITIVE_DURATION` (COMPUTE/SLEEP ≤ 0, deadline ≤ 0), `NO_COMPUTE` (program with no COMPUTE), `UNLOCK_NOT_HELD`, `RELOCK_HELD` (recursive lock), `END_WHILE_HOLDING`, `TOO_MANY_TASKS` / `TOO_MANY_MUTEXES` (caps: 64 tasks, 32 mutexes), `MISSING_HORIZON`.

Because programs are straight-line, lock-set errors are checked statically by walking each program once. **Deadlock is not a validation error**; it is a dynamic outcome (§10.8).

## 17. Invariants (checked at B5 and again after B6, in debug/test builds)

| ID | Invariant |
|---|---|
| I1 | Each task is in exactly one state; the `running` pointer matches exactly one RUNNING task or none; only §3.1 transitions occurred |
| I2 | READY ⇔ in exactly one ready queue, at level `eff`; no other task is in a queue; the non-empty-level bitmap matches |
| I3 | After B3: no READY task has `eff` strictly higher than the RUNNING task |
| I4 | After B3: if any task is READY, some task is RUNNING |
| I5 | BLOCKED ⇔ in exactly one `waiters(m)`, and `blocked_on` names that `m` |
| I6 | `owner(m) = O` ⇔ `m ∈ held(O)`; an owner is never in its own mutex's waiters; no duplicates |
| I7 | **Oracle:** cached `eff(T)` equals the from-scratch definition (PIP) or `base(T)` (NONE); `eff ≥ base` |
| I8 | PIP: for every BLOCKED `w` on `m`, `eff(owner(m)) ≥ eff(w)` |
| I9 | Compute conservation: for each task `executed + remaining == n` of its current COMPUTE; total executed ticks equals busy ticks |
| I10 | Time and trace: `now` monotone; number of tick rows equals executed ticks; event `seq` strictly increasing; event times non-decreasing |
| I11 | COMPLETED tasks hold no mutex and appear in no queue or waiter list |
| I12 | SLEEPING ⇔ has `wake_at > now`; NEW ⇔ `release > now` (after B2) |
| I13 | `resolve()` stayed within its iteration bound |

Also tested (not per-tick): determinism hash, shuffle-order invariance.

## 18. Reserved for later (not in v0.1)

- **Periodic tasks:** `release_k = offset + k·T`, jobs, overrun policy. Metrics become per `(task, job)`.
- **ICPP:** on `LOCK`, raise `eff` to the mutex ceiling; restore from the definition on unlock. Prevents deadlock and chained blocking under its assumptions.
- **Round-robin quantum**, response-time analysis cross-check, seeded random scenarios.

---

# Appendix A — S1, the golden scenario

One CPU. Mutex `R` (named `R`, **not** `M`, to avoid clashing with task M). Priorities H = 3, M = 2, L = 1. TaskIds: L = 0, M = 1, H = 2.

| Task | Prio | Release | Program | Rel. deadline | Abs. deadline |
|---|---|---|---|---|---|
| L | 1 | 0 | COMPUTE 1, LOCK R, COMPUTE 3, UNLOCK R, COMPUTE 1 | none | none |
| M | 2 | 2 | COMPUTE 4 | 10 | 12 |
| H | 3 | 3 | COMPUTE 1, LOCK R, COMPUTE 1, UNLOCK R | 6 | 9 |

Draft scenario file:

```json
{
  "name": "S1", "horizon": 30, "mutexes": ["R"],
  "tasks": [
    {"id":0,"name":"L","priority":1,"release":0,
     "program":[["COMPUTE",1],["LOCK","R"],["COMPUTE",3],["UNLOCK","R"],["COMPUTE",1]]},
    {"id":1,"name":"M","priority":2,"release":2,"deadline":10,
     "program":[["COMPUTE",4]]},
    {"id":2,"name":"H","priority":3,"release":3,"deadline":6,
     "program":[["COMPUTE",1],["LOCK","R"],["COMPUTE",1],["UNLOCK","R"]]}
  ]
}
```

### Validity conditions (tested by the scenario itself)
1. L locks R (t=1) before H requests R (t=4).
2. M is released (t=2) before L unlocks R.
3. H > M > L.
4. No same-instant coincidences: relevant instants 0, 1, 2, 3, 4 are all distinct, so §12's continuation-first rule does not influence S1.
5. H's absolute deadline 9 lies between its PIP completion (7) and its NONE completion (10). Any absolute deadline in {7, 8, 9} shows the miss.
6. The critical section (3 ticks) is long enough for M to run during it.

## A.1 Protocol NONE — per-instant trace

`ready` lists tasks high→low priority (FIFO within a level). "eff" is L/M/H; `–` = NEW or COMPLETED. State columns describe tick `t` = `[t, t+1)`.

| t | Events at instant t (in order) | Running | Ready | Blocked | R owner | eff L/M/H | Note |
|---|---|---|---|---|---|---|---|
| 0 | RELEASE L; DISPATCH L | L | — | — | — | 1/–/– | |
| 1 | LOCK_ACQUIRE L,R | L | — | — | L | 1/–/– | |
| 2 | RELEASE M; PREEMPT L; DISPATCH M | M | L | — | L | 1/2/– | L to head of level 1 |
| 3 | RELEASE H; PREEMPT M; DISPATCH H | H | M, L | — | L | 1/2/3 | |
| 4 | LOCK_BLOCK H on R (owner L); DISPATCH M | M | L | H→R | L | 1/2/3 | **inversion** (1) |
| 5 | — | M | L | H→R | L | 1/2/3 | **inversion** (2) |
| 6 | — | M | L | H→R | L | 1/2/3 | **inversion** (3) |
| 7 | COMPLETE M; DISPATCH L | L | — | H→R | L | 1/–/3 | blocking, not inversion (L is the owner) |
| 8 | — | L | — | H→R | L | 1/–/3 | blocking |
| 9 | UNLOCK L,R; HANDOFF R→H; PREEMPT L; DISPATCH H; **DEADLINE_MISS H** | H | L | — | H | 1/–/3 | miss detected after resolve |
| 10 | UNLOCK H,R (no waiters); COMPLETE H; DISPATCH L | L | — | — | — | 1/–/– | |
| 11 | COMPLETE L | — | — | — | — | – | end, `t_end = 11` |

Running sequence for ticks 0–10: `L L M H M M M L L H L`.

## A.2 Protocol PIP — per-instant trace

| t | Events at instant t (in order) | Running | Ready | Blocked | R owner | eff L/M/H | Note |
|---|---|---|---|---|---|---|---|
| 0–3 | identical to NONE | | | | | | |
| 4 | LOCK_BLOCK H on R (owner L); **PRIORITY_CHANGE L 1→3**; DISPATCH L | L | M | H→R | L | 3/2/3 | L reposition to level 3, then dispatched *over* M (M was READY, not running, so this is not a preemption) |
| 5 | — | L | M | H→R | L | 3/2/3 | blocking, no inversion |
| 6 | UNLOCK L,R; HANDOFF R→H; **PRIORITY_CHANGE L 3→1**; PREEMPT L; DISPATCH H | H | M, L | — | H | 1/2/3 | L loses priority and CPU in one instant |
| 7 | UNLOCK H,R (no waiters); COMPLETE H; DISPATCH M | M | L | — | — | 1/2/– | |
| 8 | — | M | L | — | — | 1/2/– | |
| 9 | — | M | L | — | — | 1/2/– | |
| 10 | COMPLETE M; DISPATCH L | L | — | — | — | 1/–/– | |
| 11 | COMPLETE L | — | — | — | — | – | end, `t_end = 11` |

Running sequence for ticks 0–10: `L L M H L L H M M M L`.

## A.3 Derived results

| | NONE | PIP |
|---|---|---|
| H completion / response | 10 / 7 | 7 / 4 |
| H deadline (abs 9) | **miss, lateness 1** | met |
| H blocked ticks | 5 (ticks 4–8) | 2 (ticks 4–5) |
| H inversion ticks | **3** (ticks 4–6) | **0** |
| H legitimate blocking | 2 | 2 |
| M completion / response | 7 / 5 | 10 / 8 |
| M deadline (abs 12) | met | met |
| L completion / response | 11 / 11 | 11 / 11 |
| Start latency L / M / H | 0 / 0 / 0 | 0 / 0 / 0 |
| Preemptions | 3 (t=2, 3, 9) | 3 (t=2, 3, 6) |
| Context switches | 6 (t=2, 3, 4, 7, 9, 10) | 6 (t=2, 3, 4, 6, 7, 10) |
| Utilization | 11/11 = 100% | 11/11 = 100% |
| `t_end` | 11 | 11 |

Observations to be able to say aloud:
- Under PIP the CPU work is the same (11 ticks busy, same `t_end`): PI does not create CPU time, it reorders it. H gains 3 ticks; M loses 3.
- Under NONE, ticks 7–8 are blocking (the owner needs to finish), and only ticks 4–6 are inversion.
- Both runs have the same number of preemptions and context switches; the *when* differs.

## A.4 Boundary-semantics tests that S1 does NOT cover (write separate scenarios)
- Release at the same instant as a task's pending `LOCK` (continuation-first).
- Release at the same instant a job completes (completion not delayed).
- Deadline equal to completion instant (met, no `DEADLINE_MISS`).
- Wake and release at the same instant ordered by TaskId.
- Handoff task vs woken task FIFO order at one level.