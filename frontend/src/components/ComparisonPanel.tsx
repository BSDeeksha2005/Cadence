import type { WasmResult } from '@/wasm/cadenceBridge';

interface ComparisonPanelProps {
  none: WasmResult;
  pip: WasmResult;
}

type NumericMetric =
  | 'response'
  | 'startLatency'
  | 'blockedTicks'
  | 'inversionTicks'
  | 'legitimateBlocking'
  | 'lateness'
  | 'firstDispatch'
  | 'completion';

function numericMetric(
  result: WasmResult,
  key: NumericMetric,
): number | null {
  const highPriorityTask = result.tasks.reduce(
    (best, task) =>
      task.priority > best.priority ? task : best,
    result.tasks[0],
  );

  if (!highPriorityTask) return null;

  const metric = result.taskMetrics.find(
    (item) => item.id === highPriorityTask.id,
  );

  if (!metric) return null;

  const value = metric[key];
  return value == null || !Number.isFinite(value) ? null : value;
}

function display(value: number | null): string {
  return value == null ? '—' : String(value);
}

function utilization(result: WasmResult): string {
  if (result.totalTicks === 0) return '—';
  return `${Math.round((result.busyTicks / result.totalTicks) * 100)}%`;
}

export default function ComparisonPanel({
  none,
  pip,
}: ComparisonPanelProps) {
  const rows = [
    [
      'High-priority completion',
      display(numericMetric(none, 'completion')),
      display(numericMetric(pip, 'completion')),
    ],
    [
      'High-priority response',
      display(numericMetric(none, 'response')),
      display(numericMetric(pip, 'response')),
    ],
    [
      'High-priority blocked',
      display(numericMetric(none, 'blockedTicks')),
      display(numericMetric(pip, 'blockedTicks')),
    ],
    [
      'High-priority inversion',
      display(numericMetric(none, 'inversionTicks')),
      display(numericMetric(pip, 'inversionTicks')),
    ],
    [
      'Legitimate blocking',
      display(numericMetric(none, 'legitimateBlocking')),
      display(numericMetric(pip, 'legitimateBlocking')),
    ],
    [
      'Deadline',
      none.taskMetrics.some((m) => m.deadlineMissed) ? 'MISSED' : 'MET',
      pip.taskMetrics.some((m) => m.deadlineMissed) ? 'MISSED' : 'MET',
    ],
    ['Preemptions', String(none.preemptions), String(pip.preemptions)],
    [
      'Context switches',
      String(none.contextSwitches),
      String(pip.contextSwitches),
    ],
    [
      'CPU utilization',
      utilization(none),
      utilization(pip),
    ],
  ];

  return (
    <section
      className="rounded-lg border"
      style={{
        backgroundColor: 'var(--elevated)',
        borderColor: 'var(--border)',
      }}
    >
      <div
        className="px-4 py-3 border-b"
        style={{ borderColor: 'var(--border)' }}
      >
        <div className="flex items-baseline justify-between gap-4">
          <div>
            <h2 className="text-sm font-semibold tracking-tight">
              Protocol comparison
            </h2>
            <p
              className="mt-1 text-[11px] font-mono"
              style={{ color: 'var(--textMuted)' }}
            >
              Same deterministic scenario · real C++ engine
            </p>
          </div>

          <span
            className="text-[10px] font-mono uppercase tracking-wider"
            style={{ color: 'var(--textMuted)' }}
          >
            NONE vs PIP
          </span>
        </div>
      </div>

      <div className="overflow-x-auto">
        <table className="w-full text-left">
          <thead>
            <tr
              className="border-b text-[10px] font-mono uppercase tracking-wider"
              style={{
                borderColor: 'var(--border)',
                color: 'var(--textMuted)',
              }}
            >
              <th className="py-2.5 pl-4 pr-2 font-medium">Metric</th>
              <th className="py-2.5 px-3 font-medium">NONE</th>
              <th className="py-2.5 pr-4 font-medium">PIP</th>
            </tr>
          </thead>

          <tbody className="font-mono text-xs">
            {rows.map(([label, noneValue, pipValue]) => {
              const improvement =
                noneValue !== pipValue &&
                label !== 'Preemptions' &&
                label !== 'Context switches' &&
                label !== 'CPU utilization';

              return (
                <tr
                  key={label}
                  className="border-b last:border-b-0"
                  style={{ borderColor: 'var(--border)' }}
                >
                  <td
                    className="py-2 pl-4 pr-2"
                    style={{ color: 'var(--textSecondary)' }}
                  >
                    {label}
                  </td>

                  <td
                    className="py-2 px-3 tabular-nums"
                    style={{ color: 'var(--text)' }}
                  >
                    {noneValue}
                  </td>

                  <td
                    className="py-2 pr-4 tabular-nums"
                    style={{
                      color: improvement
                        ? 'var(--accent)'
                        : 'var(--text)',
                    }}
                  >
                    {pipValue}
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
