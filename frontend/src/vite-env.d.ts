/// <reference types="vite/client" />

declare module '*.js' {
  const factory: () => Promise<any>;
  export default factory;
}
