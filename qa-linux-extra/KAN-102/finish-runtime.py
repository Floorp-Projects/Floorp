from control import send
from pathlib import Path
import json,sys
case=sys.argv[1]
s="""const done=arguments[arguments.length-1];(async()=>{const {AddonManager}=ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs');const a=await AddonManager.getAddonByID('simple-tab-groups@drive4ik');done({pid:Services.appinfo.processID,version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,sandbox:Services.prefs.getIntPref('security.sandbox.content.level'),signatureRequired:Services.prefs.getBoolPref('xpinstall.signatures.required'),addon:{id:a.id,version:a.version,signedState:a.signedState,isActive:a.isActive},workspaceEnabled:Services.prefs.getBoolPref('floorp.workspaces.enabled')});})().catch(e=>done({error:String(e)}));"""
d=send(case,'final-protection',s,**{'async':True});r=Path('/workspace/qa-linux-22/C-extra/KAN-102')/case;before=json.loads((r/'ready.json').read_text());assert d['pid']==before['pid'] and d['sandbox']==6 and d['signatureRequired'] and d['addon']['signedState']==2 and d['addon']['isActive'];print(case,'protections/pid preserved',flush=True);p=r/'command.pending';p.write_text(json.dumps({'stop':True}));p.replace(r/'command.json')
