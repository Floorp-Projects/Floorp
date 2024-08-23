export function instantiate(getCoreModule, imports, instantiateCore = WebAssembly.instantiate) {
  
  function throwInvalidBool() {
    throw new TypeError('invalid variant discriminant for bool');
  }
  
  const utf8Decoder = new TextDecoder();
  
  const utf8Encoder = new TextEncoder();
  
  let utf8EncodedLen = 0;
  function utf8Encode(s, realloc, memory) {
    if (typeof s !== 'string') throw new TypeError('expected a string');
    if (s.length === 0) {
      utf8EncodedLen = 0;
      return 1;
    }
    let buf = utf8Encoder.encode(s);
    let ptr = realloc(0, 0, 1, buf.length);
    new Uint8Array(memory.buffer).set(buf, ptr);
    utf8EncodedLen = buf.length;
    return ptr;
  }
  
  
  const module0 = getCoreModule('userjs-runner.core.wasm');
  const module1 = getCoreModule('userjs-runner.core2.wasm');
  const module2 = getCoreModule('userjs-runner.core3.wasm');
  
  const { setBoolPref, setIntPref, setStringPref } = imports['local:component/noraneko'];
  let gen = (function* init () {
    let exports0;
    let exports1;
    let memory0;
    
    function trampoline0(arg0, arg1, arg2) {
      var ptr0 = arg0;
      var len0 = arg1;
      var result0 = utf8Decoder.decode(new Uint8Array(memory0.buffer, ptr0, len0));
      var bool1 = arg2;
      setBoolPref(result0, bool1 == 0 ? false : (bool1 == 1 ? true : throwInvalidBool()));
    }
    
    function trampoline1(arg0, arg1, arg2, arg3) {
      var ptr0 = arg0;
      var len0 = arg1;
      var result0 = utf8Decoder.decode(new Uint8Array(memory0.buffer, ptr0, len0));
      var ptr1 = arg2;
      var len1 = arg3;
      var result1 = utf8Decoder.decode(new Uint8Array(memory0.buffer, ptr1, len1));
      setStringPref(result0, result1);
    }
    
    function trampoline2(arg0, arg1, arg2) {
      var ptr0 = arg0;
      var len0 = arg1;
      var result0 = utf8Decoder.decode(new Uint8Array(memory0.buffer, ptr0, len0));
      setIntPref(result0, arg2);
    }
    let exports2;
    let realloc0;
    let postReturn0;
    Promise.all([module0, module1, module2]).catch(() => {});
    ({ exports: exports0 } = yield instantiateCore(yield module1));
    ({ exports: exports1 } = yield instantiateCore(yield module0, {
      'local:component/noraneko': {
        'set-bool-pref': exports0['0'],
        'set-int-pref': exports0['2'],
        'set-string-pref': exports0['1'],
      },
    }));
    memory0 = exports1.memory;
    ({ exports: exports2 } = yield instantiateCore(yield module2, {
      '': {
        $imports: exports0.$imports,
        '0': trampoline0,
        '1': trampoline1,
        '2': trampoline2,
      },
    }));
    realloc0 = exports1.cabi_realloc;
    postReturn0 = exports1.cabi_post_run;
    
    function run(arg0) {
      var ptr0 = utf8Encode(arg0, realloc0, memory0);
      var len0 = utf8EncodedLen;
      exports1.run(ptr0, len0);
      postReturn0();
    }
    
    return { run,  };
  })();
  let promise, resolve, reject;
  function runNext (value) {
    try {
      let done;
      do {
        ({ value, done } = gen.next(value));
      } while (!(value instanceof Promise) && !done);
      if (done) {
        if (resolve) resolve(value);
        else return value;
      }
      if (!promise) promise = new Promise((_resolve, _reject) => (resolve = _resolve, reject = _reject));
      value.then(nextVal => done ? resolve() : runNext(nextVal), reject);
    }
    catch (e) {
      if (reject) reject(e);
      else throw e;
    }
  }
  const maybeSyncReturn = runNext(null);
  return promise || maybeSyncReturn;
}
