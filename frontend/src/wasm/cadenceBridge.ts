import CadenceModule from './cadence.js';

function toArray<T>(value: ArrayLike<T>): T[] {
  return Array.from(value);
}

export interface WasmTask {
  id: number;
  name: string;
  priority: number;
  release: number;
  deadline: number;
}

export interface WasmMutex {
  id: number;
  name: string;
}

export interface WasmTaskTick {
  id: number;
  state: string;
  basePriority: number;
  effectivePriority: number;
  blockedOn: number;
}

export interface WasmMutexTick {
  id: number;
  owner: number;
  waiters: number[];
}

export interface WasmTick {
  time: number;
  running: number;
  tasks: WasmTaskTick[];
  mutexes: WasmMutexTick[];
}

export interface WasmEvent {
  seq: number;
  time: number;
  kind: string;
  task: number;
  mutex: number;
  a: number;
  b: number;
  cycle: number[];
}

export interface WasmTaskMetrics {
  id: number;
  response: number;
  startLatency: number;
  blockedTicks: number;
  inversionTicks: number;
  legitimateBlocking: number;
  deadlineMissed: boolean;
  lateness: number;
  firstDispatch: number;
  completion: number;
}

export interface WasmResult {
  status: string;
  protocol: 'NONE' | 'PIP';
  endTime: number;
  scenarioName: string;
  horizon: number;
  tasks: WasmTask[];
  mutexes: WasmMutex[];
  ticks: WasmTick[];
  events: WasmEvent[];
  taskMetrics: WasmTaskMetrics[];
  busyTicks: number;
  totalTicks: number;
  preemptions: number;
  contextSwitches: number;
}

type CadenceModule = Awaited<ReturnType<typeof CadenceModule>>;

let modulePromise: Promise<CadenceModule> | null = null;

function getModule() {
  if (!modulePromise) {
    modulePromise = CadenceModule();
  }

  return modulePromise;
}

export async function runS1(
  protocol: 'NONE' | 'PIP',
): Promise<WasmResult> {
  const module = await getModule();

  const result = module.runS1(protocol);

  return {
    status: result.status,
    protocol: result.protocol,
    endTime: Number(result.endTime),
    scenarioName: result.scenarioName,
    horizon: Number(result.horizon),

    tasks: Array.from(result.tasks).map((task: any) => ({
      id: Number(task.id),
      name: task.name,
      priority: Number(task.priority),
      release: Number(task.release),
      deadline: Number(task.deadline),
    })),

    mutexes: toArray<any>(result.mutexes).map((mutex: any) => ({
      id: Number(mutex.id),
      name: mutex.name,
    })),

    ticks: toArray<any>(result.ticks).map((tick: any) => ({
      time: Number(tick.time),
      running: Number(tick.running),

      tasks: Array.from(tick.tasks).map((task: any) => ({
        id: Number(task.id),
        state: task.state,
        basePriority: Number(task.basePriority),
        effectivePriority: Number(task.effectivePriority),
        blockedOn: Number(task.blockedOn),
      })),

      mutexes: toArray<any>(tick.mutexes).map((mutex: any) => ({
        id: Number(mutex.id),
        owner: Number(mutex.owner),
        waiters: mutex.waiters.map((id: number) => Number(id)),
      })),
    })),

    events: toArray<any>(result.events).map((event: any) => ({
      seq: Number(event.seq),
      time: Number(event.time),
      kind: event.kind,
      task: Number(event.task),
      mutex: Number(event.mutex),
      a: Number(event.a),
      b: Number(event.b),
      cycle: event.cycle.map((id: number) => Number(id)),
    })),

    taskMetrics: toArray<any>(result.taskMetrics).map((metric: any) => ({
      id: Number(metric.id),
      response: Number(metric.response),
      startLatency: Number(metric.startLatency),
      blockedTicks: Number(metric.blockedTicks),
      inversionTicks: Number(metric.inversionTicks),
      legitimateBlocking: Number(metric.legitimateBlocking),
      deadlineMissed: metric.deadlineMissed,
      lateness: Number(metric.lateness),
      firstDispatch: Number(metric.firstDispatch),
      completion: Number(metric.completion),
    })),

    busyTicks: Number(result.busyTicks),
    totalTicks: Number(result.totalTicks),
    preemptions: Number(result.preemptions),
    contextSwitches: Number(result.contextSwitches),
  };
}
