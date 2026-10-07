import { useMemo, useState, useCallback } from 'react';
import {
  demoScenario,
  type TickCell,
  type Task,
} from '@/data/demoScenario';
import type { ScenarioConfig } from '@/lib/builderTypes';
import { defaultConfig } from '@/lib/builderUtils';
import Header from '@/components/Header';
import Timeline from '@/components/Timeline';
import EventLog from '@/components/EventLog';
import Inspector from '@/components/Inspector';
import ScenarioBuilder from '@/components/ScenarioBuilder';

type View = 'build' | 'simulate';

export default function App() {
  const [view, setView] = useState<View>('build');
  const [config, setConfig] = useState<ScenarioConfig>(defaultConfig);

  // Simulation view state
  const scenario = demoScenario;
  const [selectedTick, setSelectedTick] = useState<number | null>(null);
  const [selectedTaskId, setSelectedTaskId] = useState<string | null>('T1');
  const [hoverTick, setHoverTick] = useState<number | null>(null);

  const tickIndex = useMemo(() => {
    const map = new Map<number, { task: Task; cell: TickCell }[]>();
    for (const row of scenario.grid) {
      const task = scenario.tasks.find((t) => t.id === row[0]?.taskId);
      if (!task) continue;
      for (const cell of row) {
        if (!map.has(cell.tick)) map.set(cell.tick, []);
        map.get(cell.tick)!.push({ task, cell });
      }
    }
    return map;
  }, [scenario]);

  const handleSelectCell = useCallback(
    (taskId: string, tick: number) => {
      setSelectedTaskId(taskId);
      setSelectedTick(tick);
    },
    [],
  );

  const handleRun = useCallback(() => {
    setView('simulate');
    setSelectedTick(null);
    setSelectedTaskId(scenario.tasks[0]?.id ?? null);
  }, [scenario.tasks]);

  const handleBack = useCallback(() => {
    setView('build');
  }, []);

  const selectedTask =
    scenario.tasks.find((t) => t.id === selectedTaskId) ?? null;
  const selectedCell =
    selectedTask && selectedTick !== null
      ? scenario.grid
          .find((r) => r[0]?.taskId === selectedTask.id)
          ?.find((c) => c.tick === selectedTick) ?? null
      : null;

  return (
    <div
      className="min-h-screen flex flex-col"
      style={{ backgroundColor: 'var(--bg)' }}
    >
      <Header
        view={view}
        scenarioName={view === 'build' ? config.name || 'Untitled' : scenario.name}
        onBackToBuilder={handleBack}
      />

      {view === 'build' ? (
        <main className="flex-1 w-full px-5 lg:px-8 py-8">
          <ScenarioBuilder
            config={config}
            onChange={setConfig}
            onRun={handleRun}
          />
        </main>
      ) : (
        <main className="flex-1 w-full max-w-[1400px] mx-auto px-5 lg:px-8 py-6">
          {/* Demo data notice */}
          <div
            className="mb-4 px-4 py-2 rounded border text-[11px] font-mono"
            style={{
              borderColor: 'var(--border)',
              backgroundColor: 'var(--surface)',
              color: 'var(--text-muted)',
            }}
          >
            Showing demo simulation data — the configured scenario will be
            connected to the engine in a later step.
          </div>
          <div className="grid grid-cols-1 xl:grid-cols-[1fr_300px] gap-6">
            <div className="min-w-0 flex flex-col gap-6">
              <Timeline
                scenario={scenario}
                selectedTaskId={selectedTaskId}
                selectedTick={selectedTick}
                hoverTick={hoverTick}
                onSelectCell={handleSelectCell}
                onHoverTick={setHoverTick}
              />
              <EventLog
                events={scenario.events}
                selectedTick={selectedTick}
                hoverTick={hoverTick}
                onSelectTick={(t) => setSelectedTick(t)}
              />
            </div>
            <aside className="min-w-0">
              <Inspector
                task={selectedTask}
                cell={selectedCell ?? null}
                tickTasks={
                  selectedTick !== null
                    ? tickIndex.get(selectedTick) ?? []
                    : []
                }
                protocol={scenario.protocol}
              />
            </aside>
          </div>
        </main>
      )}

      <footer className="border-t" style={{ borderColor: 'var(--border)' }}>
        <div className="max-w-[1400px] mx-auto px-5 lg:px-8 py-4 flex flex-wrap items-center gap-x-4 gap-y-1 text-xs">
          <span style={{ color: 'var(--text-muted)' }}>Cadence</span>
          <span style={{ color: 'var(--border-strong)' }}>·</span>
          <span style={{ color: 'var(--text-muted)' }}>
            {view === 'build'
              ? 'Scenario builder — not yet simulated'
              : 'Demo data — not connected to engine'}
          </span>
          <span className="ml-auto font-mono" style={{ color: 'var(--text-muted)' }}>
            {view === 'build'
              ? `${config.tasks.length} task${config.tasks.length !== 1 ? 's' : ''} · ${config.mutexes.length} mutex${config.mutexes.length !== 1 ? 'es' : ''} · ${config.horizon} ticks · ${config.protocol}`
              : `${scenario.ticks} ticks · ${scenario.tasks.length} tasks · ${scenario.mutexes.length} mutex`}
          </span>
        </div>
      </footer>
    </div>
  );
}
