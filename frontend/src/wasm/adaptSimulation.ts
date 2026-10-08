import type { WasmResult } from './cadenceBridge';
import type {
  Scenario,
  Task,
  Mutex,
  TickCell,
  SimEvent,
  TaskState,
  EventCategory,
} from '@/data/demoScenario';

function taskLabel(id: number): string {
  return `T${id}`;
}

function mutexLabel(id: number): string {
  return `M${id}`;
}

function priorityLabel(priority: number): Task['priorityLabel'] {
  if (priority >= 22) return 'High';
  if (priority >= 11) return 'Medium';
  return 'Low';
}

function state(value: string): TaskState {
  return value as TaskState;
}

function eventCategory(kind: string): EventCategory {
  switch (kind) {
    case 'RELEASE':
      return 'release';
    case 'DISPATCH':
      return 'schedule';
    case 'PREEMPT':
      return 'preempt';
    case 'LOCK_ACQUIRE':
      return 'lock';
    case 'LOCK_BLOCK':
      return 'block';
    case 'UNLOCK':
      return 'unlock';
    case 'HANDOFF':
      return 'schedule';
    case 'PRIORITY_CHANGE':
      return 'pip';
    case 'COMPLETE':
      return 'complete';
    case 'DEADLINE_MISS':
      return 'deadline';
    case 'WAKE':
      return 'schedule';
    case 'SLEEP_START':
      return 'schedule';
    case 'DEADLOCK_DETECTED':
      return 'block';
    default:
      return 'schedule';
  }
}

function eventDetail(
  kind: string,
  task: string,
  mutex: number,
  a: number,
  b: number,
): string {
  const mutexText = mutex >= 0 ? ` M${mutex}` : '';

  switch (kind) {
    case 'RELEASE':
      return `${task} released → READY`;

    case 'WAKE':
      return `${task} woke → READY`;

    case 'DISPATCH':
      return `${task} dispatched → RUNNING`;

    case 'PREEMPT':
      return `${task} preempted`;

    case 'LOCK_ACQUIRE':
      return `${task} LOCK${mutexText} — acquired`;

    case 'LOCK_BLOCK':
      return `${task} LOCK${mutexText} → BLOCKED`;

    case 'UNLOCK':
      return `${task} UNLOCK${mutexText} — released`;

    case 'HANDOFF':
      return `${task} mutex handoff${mutexText}`;

    case 'PRIORITY_CHANGE':
      return `${task} priority ${a} → ${b}`;

    case 'COMPLETE':
      return `${task} COMPLETED`;

    case 'DEADLINE_MISS':
      return `${task} DEADLINE MISSED`;

    case 'SLEEP_START':
      return `${task} → SLEEPING`;

    case 'DEADLOCK_DETECTED':
      return `Deadlock detected${task ? ` involving ${task}` : ''}`;

    default:
      return `${task}${mutexText}`;
  }
}

export function adaptSimulation(result: WasmResult): Scenario {
  const tasks: Task[] = result.tasks.map((task) => ({
    id: taskLabel(task.id),
    name: task.name,
    priority: task.priority,
    priorityLabel: priorityLabel(task.priority),
    release: task.release,
    deadline: task.deadline,
    period: task.deadline >= 0 ? task.deadline - task.release : 0,
  }));

  const mutexes: Mutex[] = result.mutexes.map((mutex) => ({
    id: mutexLabel(mutex.id),
    name: mutex.name,
  }));

  const grid: TickCell[][] = tasks.map((task) => {
    const numericId = Number(task.id.slice(1));

    return result.ticks.map((tick) => {
      const snapshot = tick.tasks.find((candidate) => candidate.id === numericId);

      if (!snapshot) {
        return {
          taskId: task.id,
          tick: tick.time,
          state: 'NEW',
        };
      }

      const heldMutexes = tick.mutexes
        .filter((mutex) => mutex.owner === numericId)
        .map((mutex) => mutexLabel(mutex.id));

      return {
        taskId: task.id,
        tick: tick.time,
        state: state(snapshot.state),
        holdsMutex: heldMutexes,
        waitingOn:
          snapshot.blockedOn >= 0
            ? mutexLabel(snapshot.blockedOn)
            : undefined,
        effectivePriority:
          snapshot.effectivePriority !== snapshot.basePriority
            ? snapshot.effectivePriority
            : undefined,
      };
    });
  });

  const events: SimEvent[] = result.events.map((event) => {
    const task =
      event.task >= 0 && event.task !== 2147483647
        ? taskLabel(event.task)
        : undefined;

    return {
      id: `e${event.seq}`,
      tick: event.time,
      category: eventCategory(event.kind),
      task,
      detail: eventDetail(
        event.kind,
        task ?? '',
        event.mutex,
        event.a,
        event.b,
      ),
    };
  });

  return {
    name: result.scenarioName,
    protocol: result.protocol,
    ticks: result.ticks.length,
    tasks,
    mutexes,
    grid,
    events,
  };
}
