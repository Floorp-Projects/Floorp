const path=require('node:path'),fs=require('node:fs');
const source='/workspace/floorp-additional-ui';
require('/workspace/product3033/noraneko/node_modules/.deno/esbuild@0.27.7/node_modules/esbuild').build({
 entryPoints:[path.join(__dirname,'entry.ts')],outfile:path.join(__dirname,'test-module.js'),format:'esm',bundle:true,
 plugins:[{name:'live-product-modules',setup(b){
 b.onResolve({filter:/designs\/configs\.ts$/},()=>({path:'config-adapter',namespace:'qa'}));
 b.onLoad({filter:/.*/,namespace:'qa'},()=>({contents:`import {c as config} from "chrome://noraneko/content/assets/js/configs.js";export {config};export function setConfig(value){Services.prefs.setStringPref("floorp.design.configs",JSON.stringify(value));}export function updateChromeExtrasSetting(key,value){const next=structuredClone(config());next.uiCustomization.chromeExtras[key]=value;setConfig(next);}`,loader:'js'}));
 b.onResolve({filter:/\.\.\/index\.ts$/},()=>({path:'chrome://noraneko/content/assets/js/index9.js',external:true}));
 b.onResolve({filter:/test_harness\.ts$/},()=>({path:path.join(source,'browser-features/chrome/test/utils/test_harness.ts')}));
 b.onResolve({filter:/^chrome:/},a=>({path:a.path,external:true}));
 }}]
}).catch(()=>process.exitCode=1);
