/**
 * A minimal Parser that mimicks a small fraction of the Node.js parser
 * API.
 * To run:
 * - `npm run build-wasm`
 * - `npx ts-node examples/wasm.ts`
 */
import { readFileSync } from 'fs';
import { resolve } from 'path';
import * as constants from '../build/wasm/constants';

const bin = readFileSync(resolve(__dirname, '../build/wasm/llhttp.wasm'));
const mod = new WebAssembly.Module(bin);

const REQUEST = constants.TYPE.REQUEST;
const RESPONSE = constants.TYPE.RESPONSE;
const kOnMessageBegin = 0;
const kOnHeaders = 1;
const kOnHeadersComplete = 2;
const kOnBody = 3;
const kOnMessageComplete = 4;
const kOnExecute = 5;

const kPtr = Symbol('kPtr');
const kUrl = Symbol('kUrl');
const kStatusMessage = Symbol('kStatusMessage');
const kHeadersFields = Symbol('kHeadersFields');
const kHeadersValues = Symbol('kHeadersValues');
const kBody = Symbol('kBody');
const kReset = Symbol('kReset');
const kCheckErr = Symbol('kCheckErr');

const cstr = (ptr: number, len: number): string =>
  Buffer.from(memory.buffer, ptr, len).toString();

const wasm_on_message_begin = (p: number) => {
  const i = instMap.get(p);
  i[kReset]();
  return i[kOnMessageBegin]();
};

const wasm_on_url = (p: number, at: number, length: number) => {
  instMap.get(p)[kUrl] = cstr(at, length);
  return 0;
};

const wasm_on_status = (p: number, at: number, length: number) => {
  instMap.get(p)[kStatusMessage] = cstr(at, length);
  return 0;
};

const wasm_on_header_field = (p: number, at: number, length: number) => {
  const i= instMap.get(p)
  i[kHeadersFields].push(cstr(at, length));
  return 0;
};

const wasm_on_header_value = (p: number, at: number, length: number) => {
  const i = instMap.get(p);
  i[kHeadersValues].push(cstr(at, length));
  return 0;
};

const wasm_on_headers_complete = (p: number) => {
  const i = instMap.get(p);
  const type = get_type(p);
  const versionMajor = get_version_major(p);
  const versionMinor = get_version_minor(p);
  const rawHeaders = [];
  let method;
  let url;
  let statusCode;
  let statusMessage;
  const upgrade = get_upgrade(p);
  const shouldKeepAlive = should_keep_alive(p);

  for (let c = 0; c < i[kHeadersFields].length; c++) {
    rawHeaders.push(i[kHeadersFields][c], i[kHeadersValues][c])
  }

  if (type === HTTPParser.REQUEST) {
    method = constants.METHODS[get_method(p)];
    url = i[kUrl];
  } else if (type === HTTPParser.RESPONSE) {
    statusCode = get_status_code(p);
    statusMessage = i[kStatusMessage];
  }
  return i[kOnHeadersComplete](versionMajor, versionMinor, rawHeaders, method,
url, statusCode, statusMessage, upgrade, shouldKeepAlive);
};

const wasm_on_body = (p: number, at: number, length: number) => {
  const i = instMap.get(p);
  const body = Buffer.from(memory.buffer, at, length);
  return i[kOnBody](body);
};

const wasm_on_message_complete = (p: number) => {
  return instMap.get(p)[kOnMessageComplete]();
};

const instMap = new Map();

const inst = new WebAssembly.Instance(mod, {
  env: {
    wasm_on_message_begin,
    wasm_on_url,
    wasm_on_status,
    wasm_on_header_field,
    wasm_on_header_value,
    wasm_on_headers_complete,
    wasm_on_body,
    wasm_on_message_complete,
  },
});

const memory = inst.exports.memory as any;
const alloc = inst.exports.llhttp_alloc as CallableFunction;
const malloc = inst.exports.malloc as CallableFunction;
const execute = inst.exports.llhttp_execute as CallableFunction;
const get_type = inst.exports.llhttp_get_type as CallableFunction;
const get_upgrade = inst.exports.llhttp_get_upgrade as CallableFunction;
const should_keep_alive = inst.exports.llhttp_should_keep_alive as CallableFunction;
const get_method = inst.exports.llhttp_get_method as CallableFunction;
const get_status_code = inst.exports.llhttp_get_status_code as CallableFunction;
const get_version_minor = inst.exports.llhttp_get_http_minor as CallableFunction;
const get_version_major = inst.exports.llhttp_get_http_major as CallableFunction;
const get_error_reason = inst.exports.llhttp_get_error_reason as CallableFunction;
const free = inst.exports.free as CallableFunction;
const initialize = inst.exports._initialize as CallableFunction;

initialize(); // wasi reactor

class HTTPParser {
  static REQUEST = REQUEST;
  static RESPONSE = RESPONSE;
  static kOnMessageBegin = kOnMessageBegin;
  static kOnHeaders = kOnHeaders;
  static kOnHeadersComplete = kOnHeadersComplete;
  static kOnBody = kOnBody;
  static kOnMessageComplete = kOnMessageComplete;
  static kOnExecute = kOnExecute;

  [kPtr]: number;
  [kUrl]: string;
  [kStatusMessage]: null|string;
  [kHeadersFields]: []|[string];
  [kHeadersValues]: []|[string];
  [kBody]: null|Buffer;

  constructor(type: constants.TYPE) {
    this[kPtr] = alloc(constants.TYPE[type]);
    instMap.set(this[kPtr], this);

    this[kUrl] = '';
    this[kStatusMessage] = null;
    this[kHeadersFields] = [];
    this[kHeadersValues] = [];
    this[kBody] = null;
  }
 
  [kReset]() {
    this[kUrl] = '';
    this[kStatusMessage] = null;
    this[kHeadersFields] = [];
    this[kHeadersValues] = [];
    this[kBody] = null;
  }

  [kOnMessageBegin]() {
    return 0;
  }

  [kOnHeaders](rawHeaders: [string]) {}

  [kOnHeadersComplete](versionMajor: number, versionMinor: number, rawHeaders: [string], method: string,
    url: string, statusCode: number, statusMessage: string, upgrade: boolean, shouldKeepAlive: boolean) {
    return 0;
  }

  [kOnBody](body: Buffer) {
    this[kBody] = body;
    return 0;
  }

  [kOnMessageComplete]() {
    return 0;
  }

  destroy() {
    instMap.delete(this[kPtr]);
    free(this[kPtr]);
  }

  execute(data: Buffer) {
    const ptr = malloc(data.byteLength);
    const u8 = new Uint8Array(memory.buffer);
    u8.set(data, ptr);
    const ret = execute(this[kPtr], ptr, data.length);
    free(ptr);
    this[kCheckErr](ret);
    return ret;
  }

  [kCheckErr](n: number) {
    if (n === constants.ERROR.OK) {
      return;
    }
    const ptr = get_error_reason(this[kPtr]);
    const u8 = new Uint8Array(memory.buffer);
    const len = u8.indexOf(0, ptr) - ptr;
    throw new Error(cstr(ptr, len));
  }
}


{
  const p = new HTTPParser(HTTPParser.REQUEST);

  p.execute(Buffer.from([
    'POST /owo HTTP/1.1',
    'X: Y',
    'Content-Length: 9',
    '',
    'uh, meow?',
    '',
  ].join('\r\n')));

  console.log(p);

  p.destroy();
}

{
  const p = new HTTPParser(HTTPParser.RESPONSE);

  p.execute(Buffer.from([
    'HTTP/1.1 200 OK',
    'X: Y',
    'Content-Length: 9',
    '',
    'uh, meow?'
  ].join('\r\n')));

  console.log(p);

  p.destroy();
}
