from pathlib import Path
import json,hashlib,sys
from control import send
r=Path(__file__).parent;c=sys.argv[1];suffix=('-'+sys.argv[2]) if len(sys.argv)>2 else '';live=send(c,'fixed-version-contract-v3'+suffix,script="const p=Services.prefs;const u=Cc['@mozilla.org/updates/update-service-stub;1'].getService(Ci.nsIApplicationUpdateServiceStub);return {pid:Services.appinfo.processID,version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,pref:p.getBoolPref('app.update.disabledForTesting',false),user:p.prefHasUserValue('app.update.disabledForTesting'),disabledForTesting:u.updateDisabledForTesting,disabled:u.updateDisabled};");assert live.get('value',{}).get('version')=='156.0.1',live;v=live['value'];assert v['buildID']=='20260916130640' and v['pref'] and v['user'] and v['disabledForTesting'] and v['disabled'],v
root=r/'reported-fixed-version/product/floorp';before=json.loads((r/'reported-fixed-version/files-before.json').read_text());actual=[]
for p in sorted(root.rglob('*')):
 if p.is_symlink():actual.append({'path':str(p.relative_to(root)),'symlink':str(p.readlink())})
 elif p.is_file():actual.append({'path':str(p.relative_to(root)),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
assert before==actual,'product changed';out={'live':v,'all46EntriesMatchArchive':True,'files':actual};(r/'trials'/c/('fixed-version-contract'+suffix+'.json')).write_text(json.dumps(out,indent=2)+'\n');print(c,v,'46 entries match')
