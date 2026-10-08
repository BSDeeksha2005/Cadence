import { useCallback, useMemo, useState } from 'react';
import type { Scenario } from '@/data/demoScenario';
import type { ScenarioConfig } from '@/lib/builderTypes';
import { defaultConfig } from '@/lib/builderUtils';

import Header from '@/components/Header';
import Timeline from '@/components/Timeline';
import EventLog from '@/components/EventLog';
import ComparisonPanel from './components/ComparisonPanel';
import Inspector from '@/components/Inspector';

import { runS1, type WasmResult } from '@/wasm/cadenceBridge';
import { adaptSimulation } from '@/wasm/adaptSimulation';

type View = 'build' | 'simulate';

export default function App() {
  const [view, setView] = useState<View>('build');
  const [config, setConfig] = useState<ScenarioConfig>(defaultConfig);

  const [wasmResult, setWasmResult] = useState<WasmResult | null>(null);
  const [comparisonResults, setComparisonResults] = useState<{
    none: WasmResult;
    pip: WasmResult;
  } | null>(null);
  const [isRunning, setIsRunning] = useState(false);
  const [runError, setRunError] = useState<string | null>(null);

  const [selectedTick, setSelectedTick] = useState<number | null>(null);
  const [selectedTaskId, setSelectedTaskId] = useState<string | null>(null);
  const [hoverTick, setHoverTick] = useState<number | null>(null);

  const scenario: Scenario | null = useMemo(() => {
    if (!wasmResult) return null;
    return adaptSimulation(wasmResult);
  }, [wasmResult]);

  const tickIndex = useMemo(() => {
    const map = new Map<
      number,
      { task: Scenario['tasks'][number]; cell: Scenario['grid'][number][number] }[]
    >();

    if (!scenario) return map;

    for (const row of scenario.grid) {
      const task = scenario.tasks.find((t) => t.id === row[0]?.taskId);
      if (!task) continue;

      for (const cell of row) {
        const entries = map.get(cell.tick) ?? [];
        entries.push({ task, cell });
        map.set(cell.tick, entries);
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

  const handleRun = useCallback(async () => {
    setIsRunning(true);
    setRunError(null);

    try {
      const [noneResult, pipResult] = await Promise.all([
        runS1('NONE'),
        runS1('PIP'),
      ]);

      const result =
        config.protocol === 'PIP' ? pipResult : noneResult;

      setWasmResult(result);
      setComparisonResults({
        none: noneResult,
        pip: pipResult,
      });

      const adapted = adaptSimulation(result);

      setView('simulate');
      setSelectedTick(null);
      setSelectedTaskId(adapted.tasks[0]?.id ?? null);
    } catch (error) {
      console.error(error);
      setRunError(
        error instanceof Error
          ? error.message
          : 'Failed to run the C++ simulation.',
      );
    } finally {
      setIsRunning(false);
    }
  }, [config.protocol]);

  const handleBack = useCallback(() => {
    setView('build');
  }, []);

  const selectedTask =
    scenario?.tasks.find((t) => t.id === selectedTaskId) ?? null;

  const selectedCell =
    selectedTask && selectedTick !== null && scenario
      ? scenario.grid
          .find((row) => row[0]?.taskId === selectedTask.id)
          ?.find((cell) => cell.tick === selectedTick) ?? null
      : null;

  return (
    <div
      className="min-h-screen flex flex-col"
      style={{ backgroundColor: 'var(--bg)' }}
    >
      <Header
        view={view}
        scenarioName={
          view === 'build'
            ? config.name || 'Untitled'
            : scenario?.name ?? 'Simulation'
        }
        onBackToBuilder={handleBack}
      />

      {view === 'build' ? (
        <main className="flex-1 w-full max-w-4xl mx-auto px-5 lg:px-8 py-12">
          <section
            className="rounded-lg border p-6 md:p-8"
            style={{ backgroundColor: 'var(--elevated)', borderColor: 'var(--border)' }}
          >
            <p className="text-xs font-mono uppercase tracking-wider" style={{ color: 'var(--text-muted)' }}>
              Deterministic single-CPU simulation
            </p>
            <h1 className="mt-3 text-2xl font-semibold">Compare priority inversion protocols</h1>
            <p className="mt-3 max-w-2xl text-sm leading-6" style={{ color: 'var(--text-muted)' }}>
              Run the normative S1 scenario under NONE and Priority Inheritance (PIP).
              Inspect the execution timeline, task states, mutex ownership, event trace,
              and derived metrics.
            </p>
            <div className="mt-6 flex flex-wrap items-end gap-4">
              <label className="text-xs font-mono">
                <span className="mb-2 block uppercase tracking-wider" style={{ color: 'var(--text-muted)' }}>Scenario</span>
                <select className="cad-input min-w-48" value="S1" disabled>
                  <option value="S1">S1 · Priority inversion</option>
                </select>
              </label>
              <label className="text-xs font-mono">
                <span className="mb-2 block uppercase tracking-wider" style={{ color: 'var(--text-muted)' }}>Selected protocol</span>
                <select
                  className="cad-input min-w-32"
                  value={config.protocol}
                  onChange={(event) => setConfig({ ...config, protocol: event.target.value as 'NONE' | 'PIP' })}
                >
                  <option value="NONE">NONE</option>
                  <option value="PIP">PIP</option>
                </select>
              </label>
              <button type="button" className="cad-btn-primary" onClick={handleRun} disabled={isRunning}>
                {isRunning ? 'Running…' : 'Run comparison'}
              </button>
            </div>

          {isRunning && (
            <p
              className="mt-4 text-xs font-mono"
              style={{ color: 'var(--textMuted)' }}
            >
              Running C++ simulation…
            </p>
          )}

          {runError && (
            <p
              className="mt-4 text-xs font-mono"
              style={{ color: 'var(--blockedText)' }}
            >
              {runError}
            </p>
          )}
          </section>
        </main>
      ) : scenario ? (
        <main className="flex-1 w-full max-w-[1400px] mx-auto px-5 lg:px-8 py-6">
          <div
            className="mb-4 px-4 py-2 rounded border text-[11px] font-mono"
            style={{
              borderColor: 'var(--border)',
              backgroundColor: 'var(--surface)',
              color: 'var(--text-muted)',
            }}
          >
            Real C++ engine · WASM · {scenario.protocol}
          </div>

          <div className="grid grid-cols-1 xl:grid-cols-[1fr_300px] gap-6">
            <div className="min-w-0 flex flex-col gap-6">
              {comparisonResults && (
                <ComparisonPanel
                  none={comparisonResults.none}
                  pip={comparisonResults.pip}
                />
              )}

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
                onSelectTick={(tick) => setSelectedTick(tick)}
              />
            </div>

            <aside className="min-w-0">
              <Inspector
                task={selectedTask}
                cell={selectedCell}
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
      ) : (
        <main className="flex-1 flex items-center justify-center">
          <span
            className="text-sm font-mono"
            style={{ color: 'var(--textMuted)' }}
          >
            No simulation result.
          </span>
        </main>
      )}

      <footer className="border-t" style={{ borderColor: 'var(--border)' }}>
        <div className="max-w-[1400px] mx-auto px-5 lg:px-8 py-4 flex flex-wrap items-center gap-x-4 gap-y-1 text-xs">
          <span style={{ color: 'var(--text-muted)' }}>Cadence</span>
          <span style={{ color: 'var(--border-strong)' }}>·</span>

          <span style={{ color: 'var(--text-muted)' }}>
            {view === 'build'
              ? 'Scenario builder — not yet simulated'
              : 'C++ engine — WebAssembly'}
          </span>

          <span
            className="ml-auto font-mono"
            style={{ color: 'var(--text-muted)' }}
          >
            {view === 'build'
              ? `${config.tasks.length} task${config.tasks.length !== 1 ? 's' : ''} · ${config.mutexes.length} mutex${config.mutexes.length !== 1 ? 'es' : ''} · ${config.horizon} ticks · ${config.protocol}`
              : scenario
                ? `${scenario.ticks} ticks · ${scenario.tasks.length} tasks · ${scenario.mutexes.length} mutex`
                : ''}
          </span>
        </div>
      </footer>
    </div>
  );
}
