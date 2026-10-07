import { useState, useCallback, useMemo } from 'react';
import type {
  ScenarioConfig,
  BuilderTask,
  BuilderMutex,
  TaskOperation,
  OperationType,
  BuilderProtocol,
} from '@/lib/builderTypes';
import {
  genId,
  nextTaskId,
  nextMutexId,
  validateConfig,
  summarizeOperation,
  type ValidationIssue,
} from '@/lib/builderUtils';

interface ScenarioBuilderProps {
  config: ScenarioConfig;
  onChange: (config: ScenarioConfig) => void;
  onRun: () => void;
}

// ── Section wrapper ──────────────────────────────────────────────
function Section({
  index,
  title,
  hint,
  action,
  children,
}: {
  index: number;
  title: string;
  hint?: string;
  action?: React.ReactNode;
  children: React.ReactNode;
}) {
  return (
    <section
      className="rounded-lg border"
      style={{ backgroundColor: 'var(--elevated)', borderColor: 'var(--border)' }}
    >
      <div
        className="flex items-center gap-3 px-4 py-3 border-b"
        style={{ borderColor: 'var(--border)' }}
      >
        <span
          className="text-[11px] font-mono tabular-nums"
          style={{ color: 'var(--text-muted)' }}
        >
          {String(index).padStart(2, '0')}
        </span>
        <h2 className="text-sm font-semibold tracking-tight">{title}</h2>
        {hint && (
          <span
            className="hidden sm:inline text-[11px] font-mono"
            style={{ color: 'var(--text-muted)' }}
          >
            {hint}
          </span>
        )}
        {action && <div className="ml-auto">{action}</div>}
      </div>
      <div className="p-4">{children}</div>
    </section>
  );
}

// ── Field label ──────────────────────────────────────────────────
function FieldLabel({ children }: { children: React.ReactNode }) {
  return (
    <label
      className="block text-[11px] font-mono uppercase tracking-wider mb-1"
      style={{ color: 'var(--text-muted)' }}
    >
      {children}
    </label>
  );
}

// ── Validation error badge ───────────────────────────────────────
function ErrorTag({ message }: { message: string }) {
  return (
    <span
      className="block text-[11px] font-mono mt-1"
      style={{ color: 'var(--blocked-text)' }}
    >
      {message}
    </span>
  );
}

// ── Remove button (X) ────────────────────────────────────────────
function RemoveButton({
  label,
  onClick,
}: {
  label: string;
  onClick: () => void;
}) {
  return (
    <button
      type="button"
      className="cad-btn-danger"
      onClick={onClick}
      aria-label={label}
    >
      Remove
    </button>
  );
}

// ── Main component ───────────────────────────────────────────────
export default function ScenarioBuilder({ config, onChange, onRun }: ScenarioBuilderProps) {
  const [showErrors, setShowErrors] = useState(false);

  const issues = useMemo(() => validateConfig(config), [config]);
  const issueMap = useMemo(() => {
    const m = new Map<string, ValidationIssue>();
    for (const i of issues) m.set(i.field, i);
    return m;
  }, [issues]);

  const update = useCallback(
    (patch: Partial<ScenarioConfig>) => {
      onChange({ ...config, ...patch });
    },
    [config, onChange],
  );

  // ── Mutex operations ───────────────────────────────────────────
  const addMutex = useCallback(() => {
    const id = nextMutexId(config.mutexes.map((m) => m.id));
    const newMutex: BuilderMutex = { id, name: '' };
    update({ mutexes: [...config.mutexes, newMutex] });
  }, [config.mutexes, update]);

  const updateMutex = useCallback(
    (id: string, patch: Partial<BuilderMutex>) => {
      update({
        mutexes: config.mutexes.map((m) =>
          m.id === id ? { ...m, ...patch } : m,
        ),
      });
    },
    [config.mutexes, update],
  );

  const removeMutex = useCallback(
    (id: string) => {
      const remaining = config.mutexes.filter((m) => m.id !== id);
      // Clear references in task operations
      const tasks = config.tasks.map((t) => ({
        ...t,
        operations: t.operations.map((op) =>
          op.mutexId === id ? { ...op, mutexId: undefined } : op,
        ),
      }));
      update({ mutexes: remaining, tasks });
    },
    [config, update],
  );

  // ── Task operations ────────────────────────────────────────────
  const addTask = useCallback(() => {
    const id = nextTaskId(config.tasks.map((t) => t.id));
    const newTask: BuilderTask = {
      id,
      name: '',
      priority: 1,
      release: 0,
      deadline: null,
      operations: [{ id: genId('op'), type: 'COMPUTE', duration: 1 }],
    };
    update({ tasks: [...config.tasks, newTask] });
  }, [config.tasks, update]);

  const updateTask = useCallback(
    (id: string, patch: Partial<BuilderTask>) => {
      update({
        tasks: config.tasks.map((t) =>
          t.id === id ? { ...t, ...patch } : t,
        ),
      });
    },
    [config.tasks, update],
  );

  const removeTask = useCallback(
    (id: string) => {
      update({ tasks: config.tasks.filter((t) => t.id !== id) });
    },
    [config, update],
  );

  // ── Operation operations ───────────────────────────────────────
  const addOperation = useCallback(
    (taskId: string) => {
      const tasks = config.tasks.map((t) => {
        if (t.id !== taskId) return t;
        return {
          ...t,
          operations: [
            ...t.operations,
            { id: genId('op'), type: 'COMPUTE' as OperationType, duration: 1 },
          ],
        };
      });
      update({ tasks });
    },
    [config, update],
  );

  const updateOperation = useCallback(
    (taskId: string, opId: string, patch: Partial<TaskOperation>) => {
      const tasks = config.tasks.map((t) => {
        if (t.id !== taskId) return t;
        return {
          ...t,
          operations: t.operations.map((op) =>
            op.id === opId ? { ...op, ...patch } : op,
          ),
        };
      });
      update({ tasks });
    },
    [config, update],
  );

  const removeOperation = useCallback(
    (taskId: string, opId: string) => {
      const tasks = config.tasks.map((t) => {
        if (t.id !== taskId) return t;
        return {
          ...t,
          operations: t.operations.filter((op) => op.id !== opId),
        };
      });
      update({ tasks });
    },
    [config, update],
  );

  const moveOperation = useCallback(
    (taskId: string, opId: string, dir: -1 | 1) => {
      const tasks = config.tasks.map((t) => {
        if (t.id !== taskId) return t;
        const ops = [...t.operations];
        const idx = ops.findIndex((o) => o.id === opId);
        const target = idx + dir;
        if (idx < 0 || target < 0 || target >= ops.length) return t;
        [ops[idx], ops[target]] = [ops[target], ops[idx]];
        return { ...t, operations: ops };
      });
      update({ tasks });
    },
    [config, update],
  );

  const handleRun = useCallback(() => {
    if (issues.length > 0) {
      setShowErrors(true);
      return;
    }
    onRun();
  }, [issues, onRun]);

  const hasIssues = issues.length > 0;

  return (
    <div className="flex flex-col gap-5 max-w-[860px] mx-auto">
      {/* Workflow steps indicator */}
      <div
        className="flex items-center gap-2 text-[11px] font-mono uppercase tracking-wider"
        style={{ color: 'var(--text-muted)' }}
      >
        <span style={{ color: 'var(--accent)' }}>Build scenario</span>
        <span>→</span>
        <span>Run simulation</span>
        <span>→</span>
        <span>Inspect results</span>
      </div>

      {/* 01. Scenario */}
      <Section index={1} title="Scenario" hint="global parameters">
        <div className="grid grid-cols-1 sm:grid-cols-3 gap-4">
          <div className="sm:col-span-1">
            <FieldLabel>Scenario name</FieldLabel>
            <input
              className="cad-input"
              value={config.name}
              onChange={(e) => update({ name: e.target.value })}
              placeholder="e.g. Priority Inversion"
              aria-label="Scenario name"
            />
            {showErrors && issueMap.has('name') && (
              <ErrorTag message={issueMap.get('name')!.message} />
            )}
          </div>
          <div>
            <FieldLabel>Horizon (ticks)</FieldLabel>
            <input
              className="cad-input"
              type="number"
              min={1}
              max={10000}
              value={config.horizon}
              onChange={(e) =>
                update({ horizon: parseInt(e.target.value, 10) || 0 })
              }
              aria-label="Simulation horizon in ticks"
            />
            {showErrors && issueMap.has('horizon') && (
              <ErrorTag message={issueMap.get('horizon')!.message} />
            )}
          </div>
          <div>
            <FieldLabel>Protocol</FieldLabel>
            <select
              className="cad-select w-full"
              value={config.protocol}
              onChange={(e) =>
                update({ protocol: e.target.value as BuilderProtocol })
              }
              aria-label="Scheduling protocol"
            >
              <option value="NONE">NONE — no inheritance</option>
              <option value="PIP">PIP — priority inheritance</option>
            </select>
          </div>
        </div>
      </Section>

      {/* 02. Mutexes */}
      <Section
        index={2}
        title="Mutexes"
        hint="shared resources"
        action={
          <button type="button" className="cad-btn" onClick={addMutex}>
            + Add mutex
          </button>
        }
      >
        {config.mutexes.length === 0 ? (
          <p
            className="text-sm py-4 text-center"
            style={{ color: 'var(--text-muted)' }}
          >
            No mutexes defined. Add one if tasks share a resource.
          </p>
        ) : (
          <div className="flex flex-col gap-2">
            {config.mutexes.map((m) => (
              <div
                key={m.id}
                className="flex items-center gap-3 py-2 px-3 rounded border"
                style={{ borderColor: 'var(--border)', backgroundColor: 'var(--bg)' }}
              >
                <span
                  className="font-mono text-xs font-medium shrink-0 w-8"
                  style={{ color: 'var(--accent)' }}
                >
                  {m.id}
                </span>
                <div className="flex-1 min-w-0">
                  <input
                    className="cad-input"
                    value={m.name}
                    onChange={(e) => updateMutex(m.id, { name: e.target.value })}
                    placeholder="mutex name (e.g. bus, uart)"
                    aria-label={`Name for mutex ${m.id}`}
                  />
                </div>
                <RemoveButton
                  label={`Remove mutex ${m.id}`}
                  onClick={() => removeMutex(m.id)}
                />
              </div>
            ))}
          </div>
        )}
      </Section>

      {/* 03. Tasks */}
      <Section
        index={3}
        title="Tasks"
        hint={`${config.tasks.length} defined`}
        action={
          <button type="button" className="cad-btn" onClick={addTask}>
            + Add task
          </button>
        }
      >
        {config.tasks.length === 0 ? (
          <p
            className="text-sm py-4 text-center"
            style={{ color: 'var(--text-muted)' }}
          >
            No tasks defined. Add a task to begin building the scenario.
          </p>
        ) : (
          <div className="flex flex-col gap-4">
            {config.tasks.map((task, ti) => {
              const taskIssue = issueMap.get(`task-${task.id}`);
              return (
                <div
                  key={task.id}
                  className="rounded border"
                  style={{
                    borderColor: taskIssue && showErrors ? 'var(--blocked)' : 'var(--border)',
                    backgroundColor: 'var(--bg)',
                  }}
                >
                  {/* Task header row */}
                  <div
                    className="flex items-center gap-3 px-3 py-2.5 border-b"
                    style={{ borderColor: 'var(--border)' }}
                  >
                    <span
                      className="inline-block w-2 h-2 rounded-full shrink-0"
                      style={{
                        backgroundColor:
                          task.priority === 1
                            ? 'var(--accent)'
                            : task.priority === 2
                              ? 'var(--border-strong)'
                              : 'var(--border)',
                      }}
                      aria-hidden
                    />
                    <span
                      className="font-mono text-sm font-semibold shrink-0"
                      style={{ color: 'var(--accent)' }}
                    >
                      {task.id}
                    </span>
                    <span
                      className="text-[11px] font-mono shrink-0"
                      style={{ color: 'var(--text-muted)' }}
                    >
                      Task {ti + 1}
                    </span>
                    <div className="ml-auto">
                      <RemoveButton
                        label={`Remove task ${task.id}`}
                        onClick={() => removeTask(task.id)}
                      />
                    </div>
                  </div>

                  {/* Task parameter fields */}
                  <div className="grid grid-cols-2 sm:grid-cols-4 gap-3 px-3 py-3">
                    <div>
                      <FieldLabel>Name</FieldLabel>
                      <input
                        className="cad-input"
                        value={task.name}
                        onChange={(e) => updateTask(task.id, { name: e.target.value })}
                        placeholder="e.g. Sensor"
                        aria-label={`Name for task ${task.id}`}
                      />
                    </div>
                    <div>
                      <FieldLabel>Priority (1–32)</FieldLabel>
                      <input
                        className="cad-input"
                        type="number"
                        min={1}
                        max={32}
                        value={task.priority}
                        onChange={(e) => {
                          const v = parseInt(e.target.value, 10);
                          updateTask(task.id, { priority: isNaN(v) ? 1 : v });
                        }}
                        aria-label={`Priority for task ${task.id}`}
                      />
                    </div>
                    <div>
                      <FieldLabel>Release tick</FieldLabel>
                      <input
                        className="cad-input"
                        type="number"
                        min={0}
                        value={task.release}
                        onChange={(e) => {
                          const v = parseInt(e.target.value, 10);
                          updateTask(task.id, { release: isNaN(v) ? 0 : v });
                        }}
                        aria-label={`Release tick for task ${task.id}`}
                      />
                    </div>
                    <div>
                      <FieldLabel>Rel. deadline (optional)</FieldLabel>
                      <input
                        className="cad-input"
                        type="number"
                        min={1}
                        value={task.deadline ?? ''}
                        onChange={(e) => {
                          const raw = e.target.value;
                          if (raw === '') {
                            updateTask(task.id, { deadline: null });
                          } else {
                            const v = parseInt(raw, 10);
                            updateTask(task.id, { deadline: isNaN(v) ? null : v });
                          }
                        }}
                        placeholder="—"
                        aria-label={`Relative deadline for task ${task.id}`}
                      />
                    </div>
                  </div>
                  {showErrors && taskIssue && (
                    <div className="px-3 pb-2">
                      <ErrorTag message={taskIssue.message} />
                    </div>
                  )}

                  {/* Operations subsection */}
                  <div
                    className="border-t"
                    style={{ borderColor: 'var(--border)' }}
                  >
                    <div className="flex items-center gap-2 px-3 py-2">
                      <span
                        className="text-[11px] font-mono uppercase tracking-wider"
                        style={{ color: 'var(--text-muted)' }}
                      >
                        Operations
                      </span>
                      <span
                        className="text-[11px] font-mono"
                        style={{ color: 'var(--text-muted)' }}
                      >
                        {task.operations.length} step{task.operations.length !== 1 ? 's' : ''}
                      </span>
                      <button
                        type="button"
                        className="cad-btn-ghost ml-auto"
                        onClick={() => addOperation(task.id)}
                      >
                        + Add operation
                      </button>
                    </div>

                    {task.operations.length === 0 ? (
                      <p
                        className="text-xs py-3 px-3"
                        style={{ color: 'var(--text-muted)' }}
                      >
                        No operations. Add one to define this task's behavior.
                      </p>
                    ) : (
                      <div className="flex flex-col gap-1 px-3 pb-3">
                        {task.operations.map((op, oi) => {
                          const opIssue = issueMap.get(`task-${task.id}-op-${op.id}`);
                          return (
                            <div
                              key={op.id}
                              className="flex items-center gap-2"
                            >
                              {/* Step number */}
                              <span
                                className="font-mono text-[11px] tabular-nums shrink-0 w-6 text-right"
                                style={{ color: 'var(--text-muted)' }}
                              >
                                {oi + 1}.
                              </span>

                              {/* Operation type selector */}
                              <select
                                className="cad-select shrink-0"
                                style={{ width: 110 }}
                                value={op.type}
                                onChange={(e) => {
                                  const newType = e.target.value as OperationType;
                                  const patch: Partial<TaskOperation> = { type: newType };
                                  if (newType === 'COMPUTE' || newType === 'SLEEP') {
                                    patch.duration = op.duration ?? 1;
                                    patch.mutexId = undefined;
                                  } else {
                                    patch.mutexId = op.mutexId ?? config.mutexes[0]?.id;
                                    patch.duration = undefined;
                                  }
                                  updateOperation(task.id, op.id, patch);
                                }}
                                aria-label={`Operation type for step ${oi + 1} of task ${task.id}`}
                              >
                                <option value="COMPUTE">COMPUTE</option>
                                <option value="LOCK">LOCK</option>
                                <option value="UNLOCK">UNLOCK</option>
                                <option value="SLEEP">SLEEP</option>
                              </select>

                              {/* Operation-specific parameter */}
                              {(op.type === 'COMPUTE' || op.type === 'SLEEP') && (
                                <div className="flex items-center gap-1.5 flex-1 min-w-0">
                                  <span
                                    className="text-[11px] font-mono shrink-0"
                                    style={{ color: 'var(--text-muted)' }}
                                  >
                                    duration
                                  </span>
                                  <input
                                    className="cad-input"
                                    type="number"
                                    min={1}
                                    value={op.duration ?? 1}
                                    onChange={(e) => {
                                      const v = parseInt(e.target.value, 10);
                                      updateOperation(task.id, op.id, {
                                        duration: isNaN(v) ? 1 : v,
                                      });
                                    }}
                                    style={{ width: 72 }}
                                    aria-label={`Duration for step ${oi + 1}`}
                                  />
                                  <span
                                    className="text-[11px] font-mono shrink-0"
                                    style={{ color: 'var(--text-muted)' }}
                                  >
                                    ticks
                                  </span>
                                </div>
                              )}

                              {(op.type === 'LOCK' || op.type === 'UNLOCK') && (
                                <div className="flex items-center gap-1.5 flex-1 min-w-0">
                                  <span
                                    className="text-[11px] font-mono shrink-0"
                                    style={{ color: 'var(--text-muted)' }}
                                  >
                                    mutex
                                  </span>
                                  {config.mutexes.length === 0 ? (
                                    <span
                                      className="text-[11px] font-mono"
                                      style={{ color: 'var(--blocked-text)' }}
                                    >
                                      No mutexes — add one above
                                    </span>
                                  ) : (
                                    <select
                                      className="cad-select flex-1"
                                      value={op.mutexId ?? ''}
                                      onChange={(e) => {
                                        updateOperation(task.id, op.id, {
                                          mutexId: e.target.value || undefined,
                                        });
                                      }}
                                      aria-label={`Mutex for step ${oi + 1}`}
                                    >
                                      <option value="" disabled>
                                        Select mutex…
                                      </option>
                                      {config.mutexes.map((m) => (
                                        <option key={m.id} value={m.id}>
                                          {m.id} ({m.name || 'unnamed'})
                                        </option>
                                      ))}
                                    </select>
                                  )}
                                </div>
                              )}

                              {/* Move + remove */}
                              <div className="flex items-center gap-0.5 shrink-0">
                                <button
                                  type="button"
                                  className="cad-btn-ghost"
                                  onClick={() => moveOperation(task.id, op.id, -1)}
                                  disabled={oi === 0}
                                  style={{ opacity: oi === 0 ? 0.3 : 1 }}
                                  aria-label="Move operation up"
                                >
                                  ↑
                                </button>
                                <button
                                  type="button"
                                  className="cad-btn-ghost"
                                  onClick={() => moveOperation(task.id, op.id, 1)}
                                  disabled={oi === task.operations.length - 1}
                                  style={{
                                    opacity:
                                      oi === task.operations.length - 1 ? 0.3 : 1,
                                  }}
                                  aria-label="Move operation down"
                                >
                                  ↓
                                </button>
                                <button
                                  type="button"
                                  className="cad-btn-danger"
                                  onClick={() => removeOperation(task.id, op.id)}
                                  aria-label="Remove operation"
                                >
                                  ✕
                                </button>
                              </div>
                            </div>
                          );
                        })}
                      </div>
                    )}

                    {/* Operation preview string */}
                    {task.operations.length > 0 && (
                      <div
                        className="px-3 py-2 border-t font-mono text-[11px]"
                        style={{
                          borderColor: 'var(--border)',
                          color: 'var(--text-muted)',
                        }}
                      >
                        <span style={{ color: 'var(--text-muted)' }}>Program: </span>
                        <span style={{ color: 'var(--text-secondary)' }}>
                          {task.operations.map((op) => summarizeOperation(op)).join(' → ')}
                        </span>
                      </div>
                    )}
                  </div>
                </div>
              );
            })}
          </div>
        )}
      </Section>

      {/* Run section */}
      <div className="flex items-center gap-4 pt-2">
        <button
          type="button"
          className="cad-btn-primary"
          onClick={handleRun}
          disabled={config.tasks.length === 0}
        >
          Run Simulation
        </button>
        {showErrors && hasIssues ? (
          <span className="text-sm font-mono" style={{ color: 'var(--blocked-text)' }}>
            {issues.length} validation issue{issues.length !== 1 ? 's' : ''} to fix
          </span>
        ) : (
          <span className="text-[11px] font-mono" style={{ color: 'var(--text-muted)' }}>
            {config.tasks.length} task{config.tasks.length !== 1 ? 's' : ''} ·{' '}
            {config.mutexes.length} mutex{config.mutexes.length !== 1 ? 'es' : ''} ·{' '}
            {config.horizon} tick horizon · {config.protocol}
          </span>
        )}
      </div>
    </div>
  );
}
