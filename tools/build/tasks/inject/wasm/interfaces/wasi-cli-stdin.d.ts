export namespace WasiCliStdin {
  export function getStdin(): InputStream;
}
import type { InputStream } from './wasi-io-streams.js';
export { InputStream };
