interface HeaderProps {
  view: 'build' | 'simulate';
  scenarioName: string;
  onBackToBuilder: () => void;
}

export default function Header({
  view,
  scenarioName,
  onBackToBuilder,
}: HeaderProps) {
  return (
    <header
      className="border-b sticky top-0 z-10"
      style={{ backgroundColor: 'var(--bg)', borderColor: 'var(--border)' }}
    >
      <div className="max-w-[1400px] mx-auto px-5 lg:px-8 h-14 flex items-center gap-4">
        {/* Logo / name */}
        <div className="flex items-center gap-2.5 shrink-0">
          <span
            className="inline-block w-2.5 h-2.5 rounded-sm"
            style={{ backgroundColor: 'var(--accent)' }}
            aria-hidden
          />
          <span className="font-mono text-[15px] font-semibold tracking-tight">
            Cadence
          </span>
        </div>

        <div
          className="hidden sm:block w-px h-5"
          style={{ backgroundColor: 'var(--border)' }}
        />

        {/* Current scenario name */}
        <div className="hidden sm:flex items-baseline gap-2 min-w-0">
          <span
            className="text-[11px] font-mono uppercase tracking-wider"
            style={{ color: 'var(--text-muted)' }}
          >
            {view === 'build' ? 'Building' : 'Scenario'}
          </span>
          <span className="text-sm truncate" style={{ color: 'var(--text)' }}>
            {scenarioName}
          </span>
        </div>

        {/* Workflow indicator on the right */}
        <div className="ml-auto flex items-center gap-3">
          {/* Build / Simulate step indicator */}
          <div className="hidden md:flex items-center gap-1.5 text-[11px] font-mono uppercase tracking-wider">
            <span
              style={{
                color: view === 'build' ? 'var(--accent)' : 'var(--text-muted)',
                fontWeight: view === 'build' ? 600 : 400,
              }}
            >
              01 Build
            </span>
            <span style={{ color: 'var(--border-strong)' }}>·</span>
            <span
              style={{
                color: view === 'simulate' ? 'var(--accent)' : 'var(--text-muted)',
                fontWeight: view === 'simulate' ? 600 : 400,
              }}
            >
              02 Simulate
            </span>
            <span style={{ color: 'var(--border-strong)' }}>·</span>
            <span style={{ color: 'var(--text-muted)' }}>03 Inspect</span>
          </div>

          {view === 'simulate' && (
            <button
              onClick={onBackToBuilder}
              className="cad-btn"
              aria-label="Back to scenario builder"
            >
              ← Edit scenario
            </button>
          )}
        </div>
      </div>
    </header>
  );
}
