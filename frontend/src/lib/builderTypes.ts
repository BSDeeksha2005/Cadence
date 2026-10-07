// Types for the Cadence Scenario Builder.
// These describe the user-editable configuration before simulation.
// The C++ engine is the source of truth for simulation semantics;
// these types mirror its inputs so the UI can be wired later.

export type BuilderProtocol = 'NONE' | 'PIP';

export type OperationType = 'COMPUTE' | 'LOCK' | 'UNLOCK' | 'SLEEP';

export interface TaskOperation {
  id: string;
  type: OperationType;
  // COMPUTE / SLEEP: duration in ticks
  // LOCK / UNLOCK: mutex id
  duration?: number;
  mutexId?: string;
}

export interface BuilderMutex {
  id: string;
  name: string;
}

export interface BuilderTask {
  id: string;
  name: string;
  priority: number; // 1 (highest) to 32 (lowest)
  release: number; // release tick
  deadline: number | null; // optional relative deadline (ticks from release)
  operations: TaskOperation[];
}

export interface ScenarioConfig {
  name: string;
  horizon: number; // simulation horizon in ticks
  protocol: BuilderProtocol;
  mutexes: BuilderMutex[];
  tasks: BuilderTask[];
}
