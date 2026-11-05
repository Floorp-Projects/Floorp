from marionette_driver.marionette import Marionette
from pathlib import Path
import sys,json
m=Marionette('localhost',port=int(sys.argv[2]));m.start_session();m.set_context('chrome')
r=m.execute_script('let u=Services.io.newURI("chrome://noraneko/content/assets/js/gestures.js");let c=Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIChromeRegistry);return {greDirectory:Services.dirsvc.get("GreD",Ci.nsIFile).path,executable:Services.dirsvc.get("XREExeF",Ci.nsIFile).path,gestureModule:c.convertChromeURL(u).spec,version:Services.appinfo.version,build:Services.appinfo.appBuildID,signature:Services.prefs.getBoolPref("xpinstall.signatures.required"),signatureUserSet:Services.prefs.prefHasUserValue("xpinstall.signatures.required"),sandbox:Services.prefs.getIntPref("security.sandbox.content.level"),focusTestMode:Services.prefs.getBoolPref("focusmanager.testmode")}')
assert r['greDirectory']=='/workspace/kan97-runtime/candidate',r
assert '/workspace/kan97-runtime/candidate/omni.ja!' in r['gestureModule'],r
(Path(__file__).parent/'candidate-runtime-metadata.json').write_text(json.dumps(r,indent=2));print(json.dumps(r));m.delete_session()
