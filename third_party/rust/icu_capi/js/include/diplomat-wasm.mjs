import cfg from '../diplomat.config.js';
import {readString} from './diplomat-runtime.js'

let wasm;

const imports = {
  env: {
    log_js(ptr, len) {
      console.log(readString(wasm, ptr, len));
    },
    warn_js(ptr, len) {
      console.warn(readString(wasm, ptr, len));
    },
    trace_js(ptr, len) {
      throw new Error(readString(wasm, ptr, len));
    }
  }
}

if (typeof fetch === 'undefined') { // Node
  const fs = await import("fs");
  const wasmFile = new Uint8Array(fs.readFileSync(cfg['wasm_path']));
  const loadedWasm = await WebAssembly.instantiate(wasmFile, imports);
  wasm = loadedWasm.instance.exports;
} else { // Browser
  const loadedWasm = await WebAssembly.instantiateStreaming(fetch(cfg['wasm_path']), imports);
  wasm = loadedWasm.instance.exports;
}

wasm.diplomat_init();
if (cfg['init'] !== undefined) {
  cfg['init'](wasm);
}

export default wasm;
