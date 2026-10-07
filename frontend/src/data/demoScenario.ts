// Demo data for the Cadence simulator workspace.
// This is labelled DEMO DATA — it does not come from the C++ engine.
// The shapes mirror Cadence terminology so the UI can be wired to the
// real engine later without restructuring.

export type TaskState =
  | 'NEW'
  | 'READY'
  | 'RUNNING'
  | 'BLOCKED'
  | 'SLEEPING'
  | 'COMPLETED';

export type Operation = 'COMPUTE' | 'LOCK' | 'UNLOCK' | 'SLEEP';

export type PriorityLabel = 'Low' | 'Medium' | 'High';

export interface Mutex {
  id: string;
  name: string;
}

export interface Task {
  id: string;
  name: string;
  priority: number; // 1 = highest
  priorityLabel: PriorityLabel;
  release: number;
  deadline: number;
  period: number;
}

export interface TickCell {
  taskId: string;
  tick: number;
  state: TaskState;
  op?: Operation;
  mutex?: string; // mutex acted on this tick
  holdsMutex?: string[]; // mutexes held at end of tick
  waitingOn?: string; // mutex this task is blocked waiting for
  effectivePriority?: number; // set when PIP inherits a higher priority
  note?: string;
}

export type EventCategory =
  | 'release'
  | 'schedule'
  | 'preempt'
  | 'lock'
  | 'unlock'
  | 'block'
  | 'pip'
  | 'complete'
  | 'deadline';

export interface SimEvent {
  id: string;
  tick: number;
  category: EventCategory;
  task?: string;
  detail: string;
}

export interface Scenario {
  name: string;
  protocol: 'PIP' | 'NPP' | 'FPP';
  ticks: number;
  tasks: Task[];
  mutexes: Mutex[];
  grid: TickCell[][];
  events: SimEvent[];
}

const tasks: Task[] = [
  {
    id: 'T1',
    name: 'Sensor',
    priority: 3,
    priorityLabel: 'Low',
    release: 0,
    deadline: 24,
    period: 24,
  },
  {
    id: 'T2',
    name: 'Filter',
    priority: 2,
    priorityLabel: 'Medium',
    release: 4,
    deadline: 24,
    period: 24,
  },
  {
    id: 'T3',
    name: 'Control',
    priority: 1,
    priorityLabel: 'High',
    release: 6,
    deadline: 20,
    period: 20,
  },
];

const mutexes: Mutex[] = [{ id: 'M1', name: 'bus' }];

const TICKS = 19;

// Helper to build a SLEEPING-filled row before release.
function sleeping(taskId: string, until: number): TickCell[] {
  const cells: TickCell[] = [];
  for (let t = 0; t < until && t < TICKS; t++) {
    cells.push({ taskId, tick: t, state: 'SLEEPING' });
  }
  return cells;
}

const t1Row: TickCell[] = [
  { taskId: 'T1', tick: 0, state: 'RUNNING', op: 'COMPUTE' },
  { taskId: 'T1', tick: 1, state: 'RUNNING', op: 'COMPUTE' },
  { taskId: 'T1', tick: 2, state: 'RUNNING', op: 'LOCK', mutex: 'M1', holdsMutex: ['M1'] },
  { taskId: 'T1', tick: 3, state: 'RUNNING', op: 'COMPUTE', holdsMutex: ['M1'] },
  { taskId: 'T1', tick: 4, state: 'READY', holdsMutex: ['M1'], note: 'Preempted by T2; still holds M1' },
  { taskId: 'T1', tick: 5, state: 'READY', holdsMutex: ['M1'] },
  { taskId: 'T1', tick: 6, state: 'RUNNING', op: 'COMPUTE', holdsMutex: ['M1'], effectivePriority: 1, note: 'PIP: inherited T3 priority' },
  { taskId: 'T1', tick: 7, state: 'RUNNING', op: 'COMPUTE', holdsMutex: ['M1'], effectivePriority: 1 },
  { taskId: 'T1', tick: 8, state: 'RUNNING', op: 'UNLOCK', mutex: 'M1', holdsMutex: [], note: 'Releases M1; priority restored' },
  { taskId: 'T1', tick: 9, state: 'READY' },
  { taskId: 'T1', tick: 10, state: 'READY' },
  { taskId: 'T1', tick: 11, state: 'READY' },
  { taskId: 'T1', tick: 12, state: 'READY' },
  { taskId: 'T1', tick: 13, state: 'READY' },
  { taskId: 'T1', tick: 14, state: 'READY' },
  { taskId: 'T1', tick: 15, state: 'READY' },
  { taskId: 'T1', tick: 16, state: 'READY' },
  { taskId: 'T1', tick: 17, state: 'RUNNING', op: 'COMPUTE' },
  { taskId: 'T1', tick: 18, state: 'COMPLETED' },
];

const t2Row: TickCell[] = [
  ...sleeping('T2', 4),
  { taskId: 'T2', tick: 4, state: 'RUNNING', op: 'COMPUTE' },
  { taskId: 'T2', tick: 5, state: 'RUNNING', op: 'COMPUTE' },
  { taskId: 'T2', tick: 6, state: 'READY', note: 'Preempted by T3' },
  { taskId: 'T2', tick: 7, state: 'READY' },
  { taskId: 'T2', tick: 8, state: 'READY' },
  { taskId: 'T2', tick: 9, state: 'READY' },
  { taskId: 'T2', tick: 10, state: 'READY' },
  { taskId: 'T2', tick: 11, state: 'READY' },
  { taskId: 'T2', tick: 12, state: 'READY' },
  { taskId: 'T2', tick: 13, state: 'READY' },
  { taskId: 'T2', tick: 14, state: 'READY' },
  { taskId: 'T2', tick: 15, state: 'RUNNING', op: 'COMPUTE' },
  { taskId: 'T2', tick: 16, state: 'COMPLETED' },
  { taskId: 'T2', tick: 17, state: 'COMPLETED' },
  { taskId: 'T2', tick: 18, state: 'COMPLETED' },
];

const t3Row: TickCell[] = [
  ...sleeping('T3', 6),
  { taskId: 'T3', tick: 6, state: 'BLOCKED', op: 'LOCK', mutex: 'M1', waitingOn: 'M1', note: 'M1 held by T1 — priority inversion' },
  { taskId: 'T3', tick: 7, state: 'BLOCKED', waitingOn: 'M1' },
  { taskId: 'T3', tick: 8, state: 'RUNNING', op: 'LOCK', mutex: 'M1', holdsMutex: ['M1'], note: 'Acquired M1 after T1 released it' },
  { taskId: 'T3', tick: 9, state: 'RUNNING', op: 'COMPUTE', holdsMutex: ['M1'] },
  { taskId: 'T3', tick: 10, state: 'RUNNING', op: 'COMPUTE', holdsMutex: ['M1'] },
  { taskId: 'T3', tick: 11, state: 'RUNNING', op: 'COMPUTE', holdsMutex: ['M1'] },
  { taskId: 'T3', tick: 12, state: 'RUNNING', op: 'UNLOCK', mutex: 'M1', holdsMutex: [] },
  { taskId: 'T3', tick: 13, state: 'RUNNING', op: 'COMPUTE' },
  { taskId: 'T3', tick: 14, state: 'COMPLETED' },
  { taskId: 'T3', tick: 15, state: 'COMPLETED' },
  { taskId: 'T3', tick: 16, state: 'COMPLETED' },
  { taskId: 'T3', tick: 17, state: 'COMPLETED' },
  { taskId: 'T3', tick: 18, state: 'COMPLETED' },
];

const events: SimEvent[] = [
  { id: 'e00', tick: 0, category: 'release', task: 'T1', detail: 'T1 released → READY' },
  { id: 'e01', tick: 0, category: 'schedule', task: 'T1', detail: 'T1 scheduled → RUNNING (COMPUTE)' },
  { id: 'e02', tick: 2, category: 'lock', task: 'T1', detail: 'T1 LOCK M1 — acquired bus' },
  { id: 'e03', tick: 4, category: 'release', task: 'T2', detail: 'T2 released → READY' },
  { id: 'e04', tick: 4, category: 'preempt', task: 'T2', detail: 'T2 preempts T1 → RUNNING (COMPUTE)' },
  { id: 'e05', tick: 6, category: 'release', task: 'T3', detail: 'T3 released → READY' },
  { id: 'e06', tick: 6, category: 'preempt', task: 'T3', detail: 'T3 preempts T2 → RUNNING' },
  { id: 'e07', tick: 6, category: 'block', task: 'T3', detail: 'T3 LOCK M1 → BLOCKED (M1 owned by T1)' },
  { id: 'e08', tick: 6, category: 'pip', task: 'T1', detail: 'PIP: T1 inherits priority 1 from T3' },
  { id: 'e09', tick: 6, category: 'schedule', task: 'T1', detail: 'T1 scheduled (boosted) → RUNNING (COMPUTE)' },
  { id: 'e10', tick: 8, category: 'unlock', task: 'T1', detail: 'T1 UNLOCK M1 — released bus' },
  { id: 'e11', tick: 8, category: 'pip', task: 'T1', detail: 'PIP restore: T1 priority → 3' },
  { id: 'e12', tick: 8, category: 'schedule', task: 'T3', detail: 'T3 unblocked → READY; LOCK M1 acquired' },
  { id: 'e13', tick: 8, category: 'schedule', task: 'T3', detail: 'T3 scheduled → RUNNING (COMPUTE)' },
  { id: 'e14', tick: 12, category: 'unlock', task: 'T3', detail: 'T3 UNLOCK M1 — released bus' },
  { id: 'e15', tick: 14, category: 'complete', task: 'T3', detail: 'T3 COMPLETED' },
  { id: 'e16', tick: 14, category: 'schedule', task: 'T2', detail: 'T2 scheduled → RUNNING (COMPUTE)' },
  { id: 'e17', tick: 16, category: 'complete', task: 'T2', detail: 'T2 COMPLETED' },
  { id: 'e18', tick: 16, category: 'schedule', task: 'T1', detail: 'T1 scheduled → RUNNING (COMPUTE)' },
  { id: 'e19', tick: 18, category: 'complete', task: 'T1', detail: 'T1 COMPLETED — CPU idle' },
];

export const demoScenario: Scenario = {
  name: 'Priority Inversion (PIP)',
  protocol: 'PIP',
  ticks: TICKS,
  tasks,
  mutexes,
  grid: [t1Row, t2Row, t3Row],
  events,
};

export const stateLabels: Record<TaskState, string> = {
  NEW: 'NEW',
  READY: 'READY',
  RUNNING: 'RUNNING',
  BLOCKED: 'BLOCKED',
  SLEEPING: 'SLEEPING',
  COMPLETED: 'COMPLETED',
};
