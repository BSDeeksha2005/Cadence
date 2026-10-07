import { type Task, type TickCell } from '@/data/demoScenario';

interface InspectorProps {
  task: Task | null;
  cell: TickCell | null;
  tickTasks: { task: Task; cell: TickCell }[];
  protocol: string;
}

function StateBadge({ state }: { state: string }) {
  const map: Record<string, { bg: string; text: string; border: string }> = {
    RUNNING: { bg: '#2f5868', text: '#f4f1ea', border: '#2f5868' },
    READY: { bg: '#e3eaed', text: '#2f5868', border: '#b9cad1' },
    BLOCKED: { bg: '#e8d5cf', text: '#6b4438', border: '#c9a99e' },
    SLEEPING: { bg: 'transparent', text: '#8f877a', border: '#d6cfc0' },
    COMPLETED: { bg: '#ece7db', text: '#8f877a', border: '#d6cfc0' },
    NEW: { bg: 'transparent', text: '#8f877a', border: '#d6cfc0' },
  };
  const s = map[state] ?? map.NEW;
  return (
    <span
      className="inline-block rounded px-1.5 py-0.5 text-[11px] font-mono font-medium"
      style={{ backgroundColor: s.bg, color: s.text, border: `1px solid ${s.border}` }}
    >
      {state}
    </span>
  );
}

function Row({
  label,
  children,
}: {
  label: string;
  children: React.ReactNode;
}) {
  return (
    <div className="flex items-baseline justify-between gap-3 py-1.5">
      <span
        className="text-[11px] font-mono uppercase tracking-wider shrink-0"
        style={{ color: 'var(--textMuted)' }}
      >
        {label}
      </span>
      <span
        className="text-sm text-right font-mono"
        style={{ color: 'var(--text)' }}
      >
        {children}
      </span>
    </div>
  );
}

export default function Inspector({ task, cell, tickTasks, protocol }: InspectorProps) {
  if (!task) {
    return (
      <div
        className="rounded-lg border p-6 text-sm"
        style={{
          backgroundColor: 'var(--elevated)',
          borderColor: 'var(--border)',
          color: 'var(--textMuted)',
        }}
      >
        Select a task or tick to inspect.
      </div>
    );
  }

  const isPipBoosted = cell?.effectivePriority !== undefined && cell.effectivePriority !== task.priority;
  const effective = cell?.effectivePriority ?? task.priority;

  return (
    <div
      className="rounded-lg border"
      style={{ backgroundColor: 'var(--elevated)', borderColor: 'var(--border)' }}
    >
      {/* Header */}
      <div
        className="px-4 py-3 border-b"
        style={{ borderColor: 'var(--border)' }}
      >
        <div className="flex items-center gap-2">
          <span
            className="inline-block w-2 h-2 rounded-full"
            style={{
              backgroundColor:
                task.priority === 1
                  ? 'var(--accent)'
                  : task.priority === 2
                    ? 'var(--borderStrong)'
                    : 'var(--border)',
            }}
          />
          <span className="font-mono text-sm font-semibold">{task.id}</span>
          <span className="text-sm" style={{ color: 'var(--textSecondary)' }}>
            {task.name}
          </span>
        </div>
        <div className="mt-2 flex items-center gap-2">
          {cell ? <StateBadge state={cell.state} /> : <StateBadge state="NEW" />}
          {isPipBoosted && (
            <span
              className="inline-block rounded px-1.5 py-0.5 text-[11px] font-mono font-medium"
              style={{ backgroundColor: 'var(--accent)', color: 'var(--accentText)' }}
            >
              PIP boost
            </span>
          )}
        </div>
      </div>

      {/* Task parameters */}
      <div className="px-4 py-3">
        <h3
          className="text-[11px] font-mono uppercase tracking-wider mb-2"
          style={{ color: 'var(--textMuted)' }}
        >
          Parameters
        </h3>
        <Row label="Priority">
          {task.priority} ({task.priorityLabel})
        </Row>
        <Row label="Effective">
          <span style={{ color: isPipBoosted ? 'var(--accent)' : 'var(--text)' }}>
            {effective}
            {isPipBoosted && ` ← ${task.priority}`}
          </span>
        </Row>
        <Row label="Release">t={task.release}</Row>
        <Row label="Deadline">t={task.deadline}</Row>
        <Row label="Period">{task.period}</Row>
      </div>

      {/* Per-tick state */}
      {cell && (
        <div
          className="px-4 py-3 border-t"
          style={{ borderColor: 'var(--border)' }}
        >
          <h3
            className="text-[11px] font-mono uppercase tracking-wider mb-2"
            style={{ color: 'var(--textMuted)' }}
          >
            At tick {cell.tick}
          </h3>
          <Row label="State">{cell.state}</Row>
          {cell.op && <Row label="Operation">{cell.op}</Row>}
          {cell.mutex && <Row label="Mutex">{cell.mutex}</Row>}
          {cell.waitingOn && (
            <Row label="Waiting on">
              <span style={{ color: 'var(--blockedText)' }}>{cell.waitingOn}</span>
            </Row>
          )}
          {cell.holdsMutex && cell.holdsMutex.length > 0 && (
            <Row label="Holds">{cell.holdsMutex.join(', ')}</Row>
          )}
          {cell.note && (
            <p
              className="mt-2 text-xs leading-relaxed"
              style={{ color: 'var(--textSecondary)' }}
            >
              {cell.note}
            </p>
          )}
        </div>
      )}

      {/* Tick snapshot — all tasks at this tick */}
      {tickTasks.length > 0 && (
        <div
          className="px-4 py-3 border-t"
          style={{ borderColor: 'var(--border)' }}
        >
          <h3
            className="text-[11px] font-mono uppercase tracking-wider mb-2"
            style={{ color: 'var(--textMuted)' }}
          >
            All tasks @ t={tickTasks[0].cell.tick}
          </h3>
          <div className="space-y-1">
            {tickTasks.map(({ task: t, cell: c }) => (
              <div key={t.id} className="flex items-center gap-2 text-xs font-mono">
                <span
                  className="w-5"
                  style={{ color: 'var(--text)' }}
                >
                  {t.id}
                </span>
                <StateBadge state={c.state} />
                {c.holdsMutex && c.holdsMutex.length > 0 && (
                  <span style={{ color: 'var(--blockedText)' }}>
                    holds {c.holdsMutex.join(',')}
                  </span>
                )}
                {c.waitingOn && (
                  <span style={{ color: 'var(--blockedText)' }}>
                    waits {c.waitingOn}
                  </span>
                )}
                {c.effectivePriority && c.effectivePriority !== t.priority && (
                  <span style={{ color: 'var(--accent)' }}>
                    eff={c.effectivePriority}
                  </span>
                )}
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Protocol note */}
      <div
        className="px-4 py-3 border-t"
        style={{ borderColor: 'var(--border)' }}
      >
        <p className="text-[11px] font-mono" style={{ color: 'var(--textMuted)' }}>
          Protocol: {protocol} — Priority Inheritance Protocol. A low-priority
          task holding a mutex inherits the highest priority among blocked
          waiters.
        </p>
      </div>
    </div>
  );
}
