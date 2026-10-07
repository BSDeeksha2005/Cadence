export const COLORS = {
  bg: '#f4f1ea',
  surface: '#ece7db',
  elevated: '#f8f5ee',
  text: '#232019',
  textSecondary: '#6a6358',
  textMuted: '#8f877a',
  border: '#d6cfc0',
  borderStrong: '#b8b0a0',
  accent: '#2f5868',
  accentSoft: '#e3eaed',
  accentText: '#f4f1ea',
  blocked: '#9c6b5a',
  blockedSoft: '#e8d5cf',
  blockedText: '#6b4438',
} as const;

export const TICK_WIDTH = 48;
export const TASK_LABEL_WIDTH = 128;
export const ROW_HEIGHT = 38;

// Simulation view protocols (matches existing Timeline display)
export const SIM_PROTOCOLS = ['PIP', 'NPP', 'FPP'] as const;

// Builder protocols — the user can only choose NONE or PIP
export const BUILDER_PROTOCOLS = ['NONE', 'PIP'] as const;
