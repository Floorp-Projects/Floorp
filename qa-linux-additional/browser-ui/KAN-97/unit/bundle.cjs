const fs=require('fs'),path=require('path'),esbuild=require('/workspace/kan30-full/noraneko/node_modules/.deno/esbuild@0.27.7/node_modules/esbuild/lib/main.js');
const base='/workspace/floorp-kan97/browser-features/chrome/common/mouse-gesture';
const prior=fs.readFileSync('/workspace/qa-linux-additional/browser-ui/KAN-97/unit/baseline-actions.ts','utf8');
if(!prior.includes('const actor = browser.browsingContext?.currentWindowGlobal?.getActor('))throw Error('baseline routing changed');
const plugins=process.env.QA_BASELINE?[{name:'old-selected-context-routing',setup(b){b.onLoad({filter:/\/utils\/scroll-target\.ts$/},a=>({contents:'// Exact old action route: browser.browsingContext, regardless of focused frame.\nexport function getScrollTargetContext(selected, _focused) { return selected; }',loader:'ts',resolveDir:path.dirname(a.path)}))}}]:[];
esbuild.build({entryPoints:[base+'/test/scrollTarget.test.ts'],bundle:true,format:'iife',globalName:'qaScrollTests',outfile:process.argv[2],plugins}).catch(e=>{console.error(e);process.exit(1)});
