import { type Scenario, type TaskState } from '@/data/demoScenario';
import { TICK_WIDTH, TASK_LABEL_WIDTH, ROW_HEIGHT } from '@/lib/constants';

interface TimelineProps {
  scenario: Scenario;
  selectedTaskId: string | null;
  selectedTick: number | null;
  hoverTick: number | null;
  onSelectCell: (taskId: string, tick: number) => void;
  onHoverTick: (tick: number | null) => void;
}

const STATE_STYLES: Record<
  TaskState,
  { bg: string; text: string; border: string; label: string }
> = {
  NEW: { bg: 'transparent', text: '#8f877a', border: '#d6cfc0', label: 'NEW' },
  READY: { bg: '#e3eaed', text: '#2f5868', border: '#b9cad1', label: 'READY' },
  RUNNING: { bg: '#2f5868', text: '#f4f1ea', border: '#2f5868', label: 'RUN' },
  BLOCKED: { bg: '#e8d5cf', text: '#6b4438', border: '#c9a99e', label: 'BLK' },
  SLEEPING: { bg: 'transparent', text: '#b8b0a0', border: '#e0d9cc', label: 'SLP' },
  COMPLETED: { bg: '#ece7db', text: '#8f877a', border: '#d6cfc0', label: 'DONE' },
};

function priorityDot(priority: number) {
  // 1 = High (accent), 2 = Medium (mid), 3 = Low (muted)
  if (priority === 1) return 'var(--accent)';
  if (priority === 2) return 'var(--borderStrong)';
  return 'var(--border)';
}

export default function Timeline({
  scenario,
  selectedTaskId,
  selectedTick,
  hoverTick,
  onSelectCell,
  onHoverTick,
}: TimelineProps) {
  const ticks = scenario.ticks;

  return (
    <section
      className="rounded-lg border"
      style={{ backgroundColor: 'var(--elevated)', borderColor: 'var(--border)' }}
    >
      {/* Section header */}
      <div
        className="flex items-center justify-between px-4 py-3 border-b"
        style={{ borderColor: 'var(--border)' }}
      >
        <div className="flex items-baseline gap-3">
          <h2 className="text-sm font-semibold tracking-tight">Timeline</h2>
          <span
            className="text-[11px] font-mono uppercase tracking-wider"
            style={{ color: 'var(--textMuted)' }}
          >
            {ticks} ticks
          </span>
        </div>
        {/* Legend */}
        <div className="hidden md:flex items-center gap-3 text-[11px] font-mono">
          {(['RUNNING', 'READY', 'BLOCKED', 'SLEEPING', 'COMPLETED'] as TaskState[]).map(
            (s) => (
              <div key={s} className="flex items-center gap-1.5">
                <span
                  className="inline-block w-3 h-3 rounded-sm"
                  style={{
                    backgroundColor: STATE_STYLES[s].bg,
                    border: `1px solid ${STATE_STYLES[s].border}`,
                  }}
                />
                <span style={{ color: 'var(--textSecondary)' }}>
                  {STATE_STYLES[s].label}
                </span>
              </div>
            ),
          )}
        </div>
      </div>

      {/* Scrollable timeline body */}
      <div className="overflow-x-auto scroll-thin">
        <div className="inline-block min-w-full">
          {/* Tick ruler */}
          <div className="flex border-b" style={{ borderColor: 'var(--border)' }}>
            <div
              className="shrink-0 sticky left-0 z-[2] border-r"
              style={{
                width: TASK_LABEL_WIDTH,
                backgroundColor: 'var(--elevated)',
                borderColor: 'var(--border)',
              }}
            >
              <div
                className="h-8 flex items-center px-3 text-[11px] font-mono uppercase tracking-wider"
                style={{ color: 'var(--textMuted)' }}
              >
                Task
              </div>
            </div>
            <div className="flex">
              {Array.from({ length: ticks }, (_, t) => (
                <div
                  key={t}
                  className="flex items-center justify-center text-[11px] font-mono transition-colors"
                  style={{
                    width: TICK_WIDTH,
                    height: 32,
                    color:
                      hoverTick === t || selectedTick === t
                        ? 'var(--accent)'
                        : 'var(--textMuted)',
                    backgroundColor:
                      hoverTick === t ? 'var(--surface)' : 'transparent',
                  }}
                >
                  {t}
                </div>
              ))}
            </div>
          </div>

          {/* Task rows */}
          {scenario.grid.map((row) => {
            const task = scenario.tasks.find((t) => t.id === row[0]?.taskId);
            if (!task) return null;
            const isSelectedTask = selectedTaskId === task.id;

            return (
              <div key={task.id} className="flex border-b last:border-b-0" style={{ borderColor: 'var(--border)' }}>
                {/* Task label — sticky on horizontal scroll */}
                <div
                  className="shrink-0 sticky left-0 z-[2] border-r flex items-center gap-2 px-3"
                  style={{
                    width: TASK_LABEL_WIDTH,
                    height: ROW_HEIGHT,
                    backgroundColor: 'var(--elevated)',
                    borderColor: 'var(--border)',
                  }}
                >
                  <span
                    className="inline-block w-2 h-2 rounded-full shrink-0"
                    style={{ backgroundColor: priorityDot(task.priority) }}
                    aria-hidden
                  />
                  <span
                    className="font-mono text-xs font-medium truncate"
                    style={{ color: isSelectedTask ? 'var(--accent)' : 'var(--text)' }}
                  >
                    {task.id}
                  </span>
                  <span
                    className="text-xs truncate"
                    style={{ color: 'var(--textSecondary)' }}
                  >
                    {task.name}
                  </span>
                </div>

                {/* Tick cells */}
                <div className="flex">
                  {Array.from({ length: ticks }, (_, t) => {
                    const cell = row.find((c) => c.tick === t);
                    const state = cell?.state ?? 'SLEEPING';
                    const s = STATE_STYLES[state];
                    const isSelected = selectedTick === t && isSelectedTask;
                    const isHoverCol = hoverTick === t;

                    return (
                      <button
                        key={t}
                        onClick={() => onSelectCell(task.id, t)}
                        onMouseEnter={() => onHoverTick(t)}
                        onMouseLeave={() => onHoverTick(null)}
                        className="relative flex items-center justify-center text-[10px] font-mono font-medium transition-all"
                        style={{
                          width: TICK_WIDTH,
                          height: ROW_HEIGHT,
                          backgroundColor: s.bg,
                          color: s.text,
                          borderTop: `1px solid transparent`,
                          borderBottom: `1px solid transparent`,
                          borderLeft: `1px solid ${isHoverCol ? 'var(--borderStrong)' : 'transparent'}`,
                          borderRight: `1px solid transparent`,
                          outline: isSelected ? `2px solid var(--accent)` : 'none',
                          outlineOffset: -2,
                          cursor: 'pointer',
                        }}
                        title={
                          cell?.note
                            ? `${s.label} @ t=${t}${cell.op ? ` · ${cell.op}` : ''}${cell.mutex ? ` · ${cell.mutex}` : ''}\n${cell.note}`
                            : `${s.label} @ t=${t}${cell?.op ? ` · ${cell.op}` : ''}${cell?.mutex ? ` · ${cell.mutex}` : ''}`
                        }
                      >
                        {state === 'RUNNING' && (
                          <span className="leading-none">{cell?.op ?? 'RUN'}</span>
                        )}
                        {state === 'BLOCKED' && <span className="leading-none">BLK</span>}
                        {state === 'BLOCKED' && cell?.waitingOn && (
                          <span
                            className="absolute bottom-0.5 right-1 text-[8px] leading-none"
                            style={{ color: 'var(--blockedText)', opacity: 0.7 }}
                          >
                            {cell.waitingOn}
                          </span>
                        )}
                        {cell?.effectivePriority && (
                          <span
                            className="absolute -top-px -right-px text-[8px] font-mono px-0.5 leading-tight rounded-bl-sm"
                            style={{ backgroundColor: 'var(--accent)', color: 'var(--accentText)' }}
                            title={`PIP inherited priority ${cell.effectivePriority}`}
                          >
                            {cell.effectivePriority}
                          </span>
                        )}
                        {cell?.holdsMutex && cell.holdsMutex.length > 0 && state !== 'BLOCKED' && (
                          <span
                            className="absolute top-0.5 right-1 w-1.5 h-1.5 rounded-full"
                            style={{ backgroundColor: 'var(--blocked)' }}
                            title={`Holds: ${cell.holdsMutex.join(', ')}`}
                          />
                        )}
                      </button>
                    );
                  })}
                </div>
              </div>
            );
          })}
        </div>
      </div>

      {/* Mutex strip */}
      <div
        className="flex items-center gap-3 px-4 py-2.5 border-t text-[11px] font-mono"
        style={{ borderColor: 'var(--border)', color: 'var(--textSecondary)' }}
      >
        <span style={{ color: 'var(--textMuted)' }}>MUTEXES</span>
        {scenario.mutexes.map((m) => (
          <span key={m.id} className="flex items-center gap-1.5">
            <span
              className="inline-block w-1.5 h-1.5 rounded-full"
              style={{ backgroundColor: 'var(--blocked)' }}
            />
            {m.id} ({m.name})
          </span>
        ))}
        <span className="ml-auto hidden sm:flex items-center gap-1.5">
          <span
            className="inline-block w-2 h-2 rounded-sm"
            style={{ backgroundColor: 'var(--accent)', color: 'var(--accentText)' }}
          />
          = PIP boost
        </span>
      </div>
    </section>
  );
}
