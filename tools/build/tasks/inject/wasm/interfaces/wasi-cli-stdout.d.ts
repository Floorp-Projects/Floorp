export namespace WasiCliStdout {
  export function getStdout(): OutputStream;
}
import type { OutputStream } from './wasi-io-streams.js';
export { OutputStream };
