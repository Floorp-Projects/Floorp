// SPDX-License-Identifier: MPL-2.0
// Execute the REAL injector target-selection function and REAL after-mach call.
// Only IO/process helpers are stubbed: no browser or actual XHTML transform runs.
import { stripTypeScriptTypes } from 'node:module';
import { readFileSync, mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import vm from 'node:vm';
import assert from 'node:assert/strict';

const floorp = process.argv[2];
assert.ok(floorp, 'supply selected Floorp source root');
const source = readFileSync(path.join(floorp,'tools/src/injector.ts'),'utf8');
const feles = readFileSync(path.join(floorp,'tools/feles-build.ts'),'utf8');
const after = feles.slice(feles.indexOf('} else if (optionsPhase === "after-mach")'));
assert.ok(after.startsWith('} else if'), 'actual after-mach branch absent');
const call = after.match(/await Injector\.injectXhtmlFromTs\(\{[\s\S]*?\}\);/)?.[0];
assert.ok(call, 'actual after-mach injection call absent');
let runnable = source.replace(/^import[\s\S]*?;\n/gm,'').replace(/^export /gm,'');
runnable = stripTypeScriptTypes(runnable,{mode:'transform'});
const root = mkdtempSync(path.join(tmpdir(),'floorp-injection-contract-'));
try {
  const project=path.join(root,'runtime/noraneko');
  const expected=path.join(root,'runtime/obj-artifact-build-output/dist/bin');
  for (const ci of ['true',undefined,'false','1']) {
    /** @type {{command: string, args: string[]}[]} */
    const commands=[];
    const context=vm.createContext({path,PROJECT_ROOT:project,
      BIN_DIR:path.join(root,'wrong-local-product'),PROD_BIN_DIR:path.join(root,'wrong-prod-default'),
      Logger:class {info(){} success(){} warn(){}},
      /** @param {string} command @param {string[]} args */
      runCommandChecked:(command,args)=>{commands.push({command,args});return {success:true};},
      Deno:{env:{
        /** @param {string} key @returns {string | undefined} */
        get:(key)=>{
          /** @type {Record<string, string | undefined>} */
          const environment={CI:ci,CANON_PLATFORM:'Linux-x86_64'};
          return environment[key];
        }
      }}});
    vm.runInContext(runnable,context);
    context.Injector={injectXhtmlFromTs:context.injectXhtmlFromTs};
    await vm.runInContext('(async()=>{'+call+'})()',context);
    assert.equal(commands.length,1);
    assert.equal(commands[0].command,'deno');
    const args=commands[0].args;
    const actual=args[args.indexOf(path.join(project,'tools/scripts/xhtml.ts'))+1];
    if(ci==='true') assert.equal(actual,expected,'CI injection differs from native OBJDIR');
    else assert.notEqual(actual,expected,'negative fixture must expose lost CI env');
    assert.ok(!args.includes('--allow-browser-http-loader'));
  }
  console.log('PASS: actual after-mach/injector path selection, 4 environment fixtures');
} finally {rmSync(root,{recursive:true,force:true});}
