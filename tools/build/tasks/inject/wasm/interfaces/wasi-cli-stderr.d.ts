export namespace WasiCliStderr {
  export function getStderr(): OutputStream;
}
import type { OutputStream } from './wasi-io-streams.js';
export { OutputStream };
