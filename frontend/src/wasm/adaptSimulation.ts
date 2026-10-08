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
  return `T${id + 1}`;
}

function mutexLabel(id: number): string {
  return `M${id}`;
}

function priorityLabel(priority: number): Task['priorityLabel'] {
  if (priority === 1) return 'High';
  if (priority === 2) return 'Medium';
  return 'Low';
}

function state(value: string): TaskState {
  return value as TaskState;
}

function eventCategory(kind: string): EventCategory {
  switch (kind) {
    case 'Release':
      return 'release';
    case 'Dispatch':
      return 'schedule';
    case 'Preempt':
      return 'preempt';
    case 'LockAcquire':
      return 'lock';
    case 'LockBlock':
      return 'block';
    case 'Unlock':
      return 'unlock';
    case 'Handoff':
      return 'schedule';
    case 'PriorityChange':
      return 'pip';
    case 'Complete':
      return 'complete';
    case 'DeadlineMiss':
      return 'deadline';
    case 'Wake':
      return 'schedule';
    case 'SleepStart':
      return 'schedule';
    case 'DeadlockDetected':
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
    case 'Release':
      return `${task} released → READY`;

    case 'Wake':
      return `${task} woke → READY`;

    case 'Dispatch':
      return `${task} dispatched → RUNNING`;

    case 'Preempt':
      return `${task} preempted`;

    case 'LockAcquire':
      return `${task} LOCK${mutexText} — acquired`;

    case 'LockBlock':
      return `${task} LOCK${mutexText} → BLOCKED`;

    case 'Unlock':
      return `${task} UNLOCK${mutexText} — released`;

    case 'Handoff':
      return `${task} mutex handoff${mutexText}`;

    case 'PriorityChange':
      return `${task} priority ${a} → ${b}`;

    case 'Complete':
      return `${task} COMPLETED`;

    case 'DeadlineMiss':
      return `${task} DEADLINE MISSED`;

    case 'SleepStart':
      return `${task} → SLEEPING`;

    case 'DeadlockDetected':
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
    const numericId = Number(task.id.slice(1)) - 1;

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
          snapshot.eff !== snapshot.base ? snapshot.eff : undefined,
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
