import sys,json
from pathlib import Path
sys.path.insert(0,'/workspace/qa-linux-22/C-extra/KAN-102');from control import send
r=Path('/workspace/qa-linux-22/C-extra/KAN-102/repair-boundary');case='repair-boundary/baseline-cross';q=lambda id,s='return true;',**k:send(case,'cross-'+id,s,**k);ws=json.loads((r/'baseline-cross/result-cross-away.json').read_text());cap="const done=arguments[arguments.length-1];setTimeout(async()=>done({native:qaState(),extension:await qaExt(),manage:await qaManage()}),1500);"
q('blocked-evidence',cap,**{'async':True})
s="const done=arguments[arguments.length-1];(async()=>{const {AddonManager}=ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs');const a=await AddonManager.getAddonByID('simple-tab-groups@drive4ik');await a.disable();done({signedState:a.signedState,isActive:a.isActive,sandbox:Services.prefs.getIntPref('security.sandbox.content.level')});})().catch(e=>done({error:String(e)}));"
q('disable',s,**{'async':True});q('after-disable-away',cap,**{'async':True});q('return-after-disable','workspacesFuncs.changeWorkspace('+json.dumps(ws['A'])+');return qaState();');d=q('final',cap,**{'async':True});print('BASELINE_GROUP_ROW_BLOCKED_NO_SWITCH; disabled-and-return fixture count',len([t for t in d['native']['tabs'] if '/A/' in t['url'] or '/B/' in t['url']]),flush=True)
