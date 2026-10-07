import type { ScenarioConfig } from './builderTypes';

let counter = 0;

export function genId(prefix: string): string {
  counter += 1;
  return `${prefix}${counter}`;
}

export function nextTaskId(existing: string[]): string {
  const used = new Set(existing);
  for (let i = 1; i <= 999; i++) {
    const candidate = `T${i}`;
    if (!used.has(candidate)) return candidate;
  }
  return `T${Date.now() % 10000}`;
}

export function nextMutexId(existing: string[]): string {
  const used = new Set(existing);
  for (let i = 1; i <= 999; i++) {
    const candidate = `M${i}`;
    if (!used.has(candidate)) return candidate;
  }
  return `M${Date.now() % 10000}`;
}

export interface ValidationIssue {
  field: string;
  message: string;
}

export function validateConfig(config: ScenarioConfig): ValidationIssue[] {
  const issues: ValidationIssue[] = [];

  if (!config.name.trim()) {
    issues.push({ field: 'name', message: 'Scenario name is required.' });
  }

  if (config.horizon < 1 || config.horizon > 10000) {
    issues.push({ field: 'horizon', message: 'Horizon must be between 1 and 10000 ticks.' });
  }

  const mutexIds = new Set(config.mutexes.map((m) => m.id));

  for (const m of config.mutexes) {
    if (!m.name.trim()) {
      issues.push({ field: `mutex-${m.id}`, message: `Mutex ${m.id}: name is required.` });
    }
  }

  const taskIds = new Set<string>();
  for (const t of config.tasks) {
    if (taskIds.has(t.id)) {
      issues.push({ field: `task-${t.id}`, message: `Duplicate task ID: ${t.id}.` });
    }
    taskIds.add(t.id);

    if (!t.name.trim()) {
      issues.push({ field: `task-${t.id}`, message: `${t.id}: task name is required.` });
    }
    if (t.priority < 1 || t.priority > 32) {
      issues.push({ field: `task-${t.id}`, message: `${t.id}: priority must be 1–32.` });
    }
    if (t.release < 0) {
      issues.push({ field: `task-${t.id}`, message: `${t.id}: release must be ≥ 0.` });
    }
    if (t.deadline !== null && t.deadline < 1) {
      issues.push({ field: `task-${t.id}`, message: `${t.id}: deadline must be ≥ 1 or empty.` });
    }

    for (const op of t.operations) {
      if ((op.type === 'COMPUTE' || op.type === 'SLEEP')) {
        if (op.duration === undefined || op.duration < 1) {
          issues.push({
            field: `task-${t.id}-op-${op.id}`,
            message: `${t.id}: ${op.type} duration must be ≥ 1.`,
          });
        }
      }
      if ((op.type === 'LOCK' || op.type === 'UNLOCK')) {
        if (!op.mutexId || !mutexIds.has(op.mutexId)) {
          issues.push({
            field: `task-${t.id}-op-${op.id}`,
            message: `${t.id}: ${op.type} references a missing or removed mutex.`,
          });
        }
      }
    }
  }

  if (config.tasks.length === 0) {
    issues.push({ field: 'tasks', message: 'At least one task is required.' });
  }

  return issues;
}

export function summarizeOperation(op: { type: string; duration?: number; mutexId?: string }): string {
  switch (op.type) {
    case 'COMPUTE':
      return `COMPUTE ${op.duration ?? '?'}`;
    case 'LOCK':
      return `LOCK ${op.mutexId ?? '?'}`;
    case 'UNLOCK':
      return `UNLOCK ${op.mutexId ?? '?'}`;
    case 'SLEEP':
      return `SLEEP ${op.duration ?? '?'}`;
    default:
      return op.type;
  }
}

export const defaultConfig: ScenarioConfig = {
  name: 'Priority Inversion (PIP)',
  horizon: 20,
  protocol: 'PIP',
  mutexes: [
    { id: 'M1', name: 'bus' },
  ],
  tasks: [
    {
      id: 'T1',
      name: 'Sensor',
      priority: 3,
      release: 0,
      deadline: 24,
      operations: [
        { id: 'op1', type: 'COMPUTE', duration: 2 },
        { id: 'op2', type: 'LOCK', mutexId: 'M1' },
        { id: 'op3', type: 'COMPUTE', duration: 4 },
        { id: 'op4', type: 'UNLOCK', mutexId: 'M1' },
        { id: 'op5', type: 'COMPUTE', duration: 2 },
      ],
    },
    {
      id: 'T2',
      name: 'Filter',
      priority: 2,
      release: 4,
      deadline: 20,
      operations: [
        { id: 'op6', type: 'COMPUTE', duration: 3 },
        { id: 'op7', type: 'COMPUTE', duration: 1 },
      ],
    },
    {
      id: 'T3',
      name: 'Control',
      priority: 1,
      release: 6,
      deadline: 14,
      operations: [
        { id: 'op8', type: 'LOCK', mutexId: 'M1' },
        { id: 'op9', type: 'COMPUTE', duration: 5 },
        { id: 'op10', type: 'UNLOCK', mutexId: 'M1' },
        { id: 'op11', type: 'COMPUTE', duration: 2 },
      ],
    },
  ],
};
