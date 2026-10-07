import { type SimEvent, type EventCategory } from '@/data/demoScenario';

interface EventLogProps {
  events: SimEvent[];
  selectedTick: number | null;
  hoverTick: number | null;
  onSelectTick: (tick: number) => void;
}

const CATEGORY_LABEL: Record<EventCategory, string> = {
  release: 'RELEASE',
  schedule: 'SCHED',
  preempt: 'PREEMPT',
  lock: 'LOCK',
  unlock: 'UNLOCK',
  block: 'BLOCK',
  pip: 'PIP',
  complete: 'DONE',
  deadline: 'DEADLINE',
};

const CATEGORY_COLOR: Record<EventCategory, string> = {
  release: 'var(--textSecondary)',
  schedule: 'var(--accent)',
  preempt: 'var(--blocked)',
  lock: 'var(--blockedText)',
  unlock: 'var(--blockedText)',
  block: 'var(--blocked)',
  pip: 'var(--accent)',
  complete: 'var(--textMuted)',
  deadline: 'var(--blockedText)',
};

export default function EventLog({
  events,
  selectedTick,
  hoverTick,
  onSelectTick,
}: EventLogProps) {
  return (
    <section
      className="rounded-lg border"
      style={{ backgroundColor: 'var(--elevated)', borderColor: 'var(--border)' }}
    >
      <div
        className="flex items-center justify-between px-4 py-3 border-b"
        style={{ borderColor: 'var(--border)' }}
      >
        <h2 className="text-sm font-semibold tracking-tight">Event Trace</h2>
        <span
          className="text-[11px] font-mono uppercase tracking-wider"
          style={{ color: 'var(--textMuted)' }}
        >
          {events.length} events · deterministic
        </span>
      </div>

      <div className="max-h-[280px] overflow-y-auto scroll-thin">
        <table className="w-full text-left">
          <thead
            className="sticky top-0 z-[1]"
            style={{ backgroundColor: 'var(--elevated)' }}
          >
            <tr
              className="border-b text-[11px] font-mono uppercase tracking-wider"
              style={{ borderColor: 'var(--border)', color: 'var(--textMuted)' }}
            >
              <th className="py-2 pl-4 pr-2 w-12 font-medium">Tick</th>
              <th className="py-2 px-2 w-20 font-medium">Type</th>
              <th className="py-2 px-2 w-14 font-medium">Task</th>
              <th className="py-2 pr-4 font-medium">Detail</th>
            </tr>
          </thead>
          <tbody className="font-mono text-xs">
            {events.map((ev) => {
              const active = selectedTick === ev.tick || hoverTick === ev.tick;
              return (
                <tr
                  key={ev.id}
                  onClick={() => onSelectTick(ev.tick)}
                  onMouseEnter={() => {}}
                  className="border-b last:border-b-0 cursor-pointer transition-colors"
                  style={{
                    borderColor: 'var(--border)',
                    backgroundColor: active ? 'var(--surface)' : 'transparent',
                  }}
                >
                  <td
                    className="py-1.5 pl-4 pr-2 tabular-nums"
                    style={{ color: active ? 'var(--accent)' : 'var(--textMuted)' }}
                  >
                    {String(ev.tick).padStart(2, '0')}
                  </td>
                  <td
                    className="py-1.5 px-2 font-medium"
                    style={{ color: CATEGORY_COLOR[ev.category] }}
                  >
                    {CATEGORY_LABEL[ev.category]}
                  </td>
                  <td
                    className="py-1.5 px-2"
                    style={{ color: 'var(--text)' }}
                  >
                    {ev.task ?? '—'}
                  </td>
                  <td
                    className="py-1.5 pr-4"
                    style={{ color: 'var(--textSecondary)' }}
                  >
                    {ev.detail}
                  </td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>
    </section>
  );
}
