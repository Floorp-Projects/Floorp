from pathlib import Path
import json,sys
sys.path.insert(0,'/workspace/qa-linux-22/C-extra/KAN-102');from control import send
r=Path('/workspace/qa-linux-22/C-extra/KAN-102/repair-boundary')
for kind in (sys.argv[1:] or ['native-owned-v2','css-layer-v2','baseline','native-cross','css-cross']):
 case='repair-boundary/'+kind
 s="""const done=arguments[arguments.length-1];(async()=>{const {AddonManager}=ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs');const a=await AddonManager.getAddonByID('simple-tab-groups@drive4ik');done({pid:Services.appinfo.processID,version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,sandbox:Services.prefs.getIntPref('security.sandbox.content.level'),signatureRequired:Services.prefs.getBoolPref('xpinstall.signatures.required'),addon:a?{signedState:a.signedState,isActive:a.isActive}:null,native:qaState()});})().catch(e=>done({error:String(e)}));"""
 d=send(case,'boundary-final-safety',s,**{'async':True});assert d['sandbox']==6 and d['signatureRequired'];fixtures=[t for t in d['native']['tabs'] if '/A/' in t['url'] or '/B/' in t['url']];assert len(fixtures)==6 and all(not t['hidden'] and t['display']!='none' for t in fixtures)
 if kind.endswith('cross'):assert d['addon']['signedState']==2 and not d['addon']['isActive']
 else:assert d['addon'] is None
 pending=r/kind/'command.pending';pending.write_text(json.dumps({'stop':True}));pending.replace(r/kind/'command.json');print(kind,'six fixtures recovered, sandbox/signature intact; normal quit requested',flush=True)
