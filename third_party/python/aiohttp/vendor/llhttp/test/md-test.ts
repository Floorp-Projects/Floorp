import * as assert from 'assert';
import * as fs from 'fs';
import { LLParse } from 'llparse';
import { Group, MDGator, Metadata, Test } from 'mdgator';
import * as path from 'path';
import * as vm from 'vm';

import * as llhttp from '../src/llhttp';
import {IHTTPResult} from '../src/llhttp/http';
import {IURLResult} from '../src/llhttp/url';
import { build, FixtureResult, TestType } from './fixtures';

//
// Cache nodes/llparse instances ahead of time
// (different types of tests will re-use them)
//

interface INodeCacheEntry {
  llparse: LLParse;
  entry: IHTTPResult['entry'];
}

interface IUrlCacheEntry {
  llparse: LLParse;
  entry: IURLResult['entry']['normal'];
}

const nodeCache = new Map<llhttp.HTTPMode, INodeCacheEntry>();
const urlCache = new Map<llhttp.HTTPMode, IUrlCacheEntry>();
const modeCache = new Map<string, FixtureResult>();

function buildNode(mode: llhttp.HTTPMode) {
  let entry = nodeCache.get(mode);

  if (entry) {
    return entry;
  }

  const p = new LLParse();
  const instance = new llhttp.HTTP(p, mode);

  entry = { llparse: p, entry: instance.build().entry };
  nodeCache.set(mode, entry);
  return entry;
}

function buildURL(mode: llhttp.HTTPMode) {
  let entry = urlCache.get(mode);

  if (entry) {
    return entry;
  }

  const p = new LLParse();
  const instance = new llhttp.URL(p, mode, true);

  const node = instance.build();

  // Loop
  node.exit.toHTTP.otherwise(node.entry.normal);
  node.exit.toHTTP09.otherwise(node.entry.normal);

  entry = { llparse: p, entry: node.entry.normal };
  urlCache.set(mode, entry);
  return entry;
}

//
// Build binaries using cached nodes/llparse
//

async function buildMode(mode: llhttp.HTTPMode, ty: TestType, meta: any)
    : Promise<FixtureResult> {

  const cacheKey = `${mode}:${ty}:${JSON.stringify(meta || {})}`;
  let entry = modeCache.get(cacheKey);

  if (entry) {
    return entry;
  }

  let node;
  let prefix: string;
  let extra: string[];
  if (ty === 'url') {
    node = buildURL(mode);
    prefix = 'url';
    extra = [];
  } else {
    node = buildNode(mode);
    prefix = 'http';
    extra = [
      '-DLLHTTP__TEST_HTTP',
      path.join(__dirname, '..', 'src', 'native', 'http.c'),
    ];
  }

  if (meta.pause) {
    extra.push(`-DLLHTTP__TEST_PAUSE_${meta.pause.toUpperCase()}=1`);
  }

  if (meta.skipBody) {
    extra.push('-DLLHTTP__TEST_SKIP_BODY=1');
  }

  entry = await build(node.llparse, node.entry, `${prefix}-${mode}-${ty}`, {
    extra,
  }, ty);

  modeCache.set(cacheKey, entry);
  return entry;
}

interface IFixtureMap {
  [key: string]: { [key: string]: Promise<FixtureResult> };
}

//
// Run test suite
//

function run(name: string): void {
  const md = new MDGator();

  const raw = fs.readFileSync(path.join(__dirname, name + '.md')).toString();
  const groups = md.parse(raw);

  function runSingleTest(mode: llhttp.HTTPMode, ty: TestType, meta: any,
                         input: string,
                         expected: ReadonlyArray<string | RegExp>): void {
    it(`should pass in mode="${mode}" and for type="${ty}"`, async () => {
      const binary = await buildMode(mode, ty, meta);
      await binary.check(input, expected, {
        noScan: meta.noScan === true,
      });
    });
  }

  function runTest(test: Test) {
    describe(test.name + ` at ${name}.md:${test.line + 1}`, () => {
      let modes: llhttp.HTTPMode[] = [ 'strict', 'loose' ];
      let types: TestType[] = [ 'none' ];

      const isURL = test.values.has('url');
      const inputKey = isURL ? 'url' : 'http';

      assert(test.values.has(inputKey),
        `Missing "${inputKey}" code in md file`);
      assert.strictEqual(test.values.get(inputKey)!.length, 1,
        `Expected just one "${inputKey}" input`);

      let meta: Metadata;
      if (test.meta.has(inputKey)) {
        meta = test.meta.get(inputKey)![0]!;
      } else {
        assert(isURL, 'Missing required http metadata');
        meta = {};
      }

      if (isURL) {
        types = [ 'url' ];
      } else {
        assert(meta.hasOwnProperty('type'), 'Missing required `type` metadata');
        if (meta.type === 'request') {
          types.push('request');
        } else if (meta.type === 'response') {
          types.push('response');
        } else if (meta.type === 'request-only') {
          types = [ 'request' ];
        } else if (meta.type === 'request-lenient-headers') {
          types = [ 'request-lenient-headers' ];
        } else if (meta.type === 'request-lenient-chunked-length') {
          types = [ 'request-lenient-chunked-length' ];
        } else if (meta.type === 'request-lenient-keep-alive') {
          types = [ 'request-lenient-keep-alive' ];
        } else if (meta.type === 'request-lenient-transfer-encoding') {
          types = [ 'request-lenient-transfer-encoding' ];
        } else if (meta.type === 'request-lenient-version') {
          types = [ 'request-lenient-version' ];
        } else if (meta.type === 'response-lenient-keep-alive') {
          types = [ 'response-lenient-keep-alive' ];
        } else if (meta.type === 'response-lenient-headers') {
          types = [ 'response-lenient-headers' ];
        } else if (meta.type === 'response-lenient-version') {
          types = [ 'response-lenient-version' ];
        } else if (meta.type === 'response-only') {
          types = [ 'response' ];
        } else if (meta.type === 'request-finish') {
          types = [ 'request-finish' ];
        } else if (meta.type === 'response-finish') {
          types = [ 'response-finish' ];
        } else {
          throw new Error(`Invalid value of \`type\` metadata: "${meta.type}"`);
        }
      }

      assert(test.values.has('log'), 'Missing `log` code in md file');

      assert.strictEqual(test.values.get('log')!.length, 1,
        'Expected just one output');

      if (meta.mode === 'strict') {
        modes = [ 'strict' ];
      } else if (meta.mode === 'loose') {
        modes = [ 'loose' ];
      } else {
        assert(!meta.hasOwnProperty('mode'),
          `Invalid value of \`mode\` metadata: "${meta.mode}"`);
      }

      let input: string = test.values.get(inputKey)![0];
      let expected: string = test.values.get('log')![0];

      // Remove trailing newline
      input = input.replace(/\n$/, '');

      // Remove escaped newlines
      input = input.replace(/\\(\r\n|\r|\n)/g, '');

      // Normalize all newlines
      input = input.replace(/\r\n|\r|\n/g, '\r\n');

      // Replace escaped CRLF, tabs, form-feed
      input = input.replace(/\\r/g, '\r');
      input = input.replace(/\\n/g, '\n');
      input = input.replace(/\\t/g, '\t');
      input = input.replace(/\\f/g, '\f');
      input = input.replace(/\\x([0-9a-fA-F]+)/g, (all, hex) => {
        return String.fromCharCode(parseInt(hex, 16));
      });

      // Useful in token tests
      input = input.replace(/\\([0-7]{1,3})/g, (_, digits) => {
        return String.fromCharCode(parseInt(digits, 8));
      });

      // Evaluate inline JavaScript
      input = input.replace(/\$\{(.+?)\}/g, (_, code) => {
        return vm.runInNewContext(code) + '';
      });

      // Escape first symbol `\r` or `\n`, `|`, `&` for Windows
      if (process.platform === 'win32') {
        const firstByte = Buffer.from(input)[0];
        if (firstByte === 0x0a || firstByte === 0x0d) {
          input = '\\' + input;
        }

        input = input.replace(/\|/g, '^|');
        input = input.replace(/&/g, '^&');
      }

      // Replace escaped tabs/form-feed in expected too
      expected = expected.replace(/\\t/g, '\t');
      expected = expected.replace(/\\f/g, '\f');

      // Split
      const expectedLines = expected.split(/\n/g).slice(0, -1);

      const fullExpected = expectedLines.map((line) => {
        if (line.startsWith('/')) {
          return new RegExp(line.trim().slice(1, -1));
        } else {
          return line;
        }
      });

      for (const mode of modes) {
        for (const ty of types) {
          if (meta.skip === true || (process.env.ONLY === 'true' && !meta.only)) {
            continue;
          }

          runSingleTest(mode, ty, meta, input, fullExpected);
        }
      }
    });
  }

  function runGroup(group: Group) {
    describe(group.name + ` at ${name}.md:${group.line + 1}`, function() {
      this.timeout(60000);

      for (const child of group.children) {
        runGroup(child);
      }

      for (const test of group.tests) {
        runTest(test);
      }
    });
  }

  for (const group of groups) {
    runGroup(group);
  }
}

run('request/sample');
run('request/lenient-headers');
run('request/lenient-version');
run('request/method');
run('request/uri');
run('request/connection');
run('request/content-length');
run('request/transfer-encoding');
run('request/invalid');
run('request/finish');
run('request/pausing');
run('request/pipelining');

run('response/sample');
run('response/connection');
run('response/content-length');
run('response/transfer-encoding');
run('response/invalid');
run('response/finish');
run('response/lenient-version');
run('response/pausing');
run('response/pipelining');

run('url');
