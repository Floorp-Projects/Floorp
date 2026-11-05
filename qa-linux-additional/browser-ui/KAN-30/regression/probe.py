import os,sys,json,hashlib
from pathlib import Path
from marionette_driver.marionette import Marionette
R=Path(__file__).parent;port=int(sys.argv[1]);m=Marionette('localhost',port=port,socket_timeout=45);m.start_session();m.set_context('chrome');r={'fixture':'MouseEvent hover + synthetic blur/deactivate; not physical or OS XTest','productCopy':os.environ.get('QA_PRODUCT')}
try:
 r['resources']=m.execute_script('return {gre:Services.dirsvc.get("GreD",Ci.nsIFile).path,module:Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIChromeRegistry).convertChromeURL(Services.io.newURI("chrome://noraneko/content/assets/js/index9.js")).spec}')
 assert r['resources']['gre']==str(Path(r['productCopy']).parent)
 r['moduleSha256']=m.execute_async_script('const done=arguments[arguments.length-1];fetch("chrome://noraneko/content/assets/js/index9.js").then(x=>x.text()).then(s=>done(s));');r['moduleSha256']=hashlib.sha256(r['moduleSha256'].encode()).hexdigest()
 r['result']=m.execute_async_script('''const done=arguments[arguments.length-1];let s=document.createElement('script');s.type='module';s.src='chrome://noraneko/content/assets/js/qa-sidebar-test.js';s.onload=()=>window.qaKan30Run().then(()=>done({passed:true}),e=>done({passed:false,error:String(e)}));s.onerror=e=>done({error:'external test module load failed'});document.head.appendChild(s);''',script_timeout=45000)
except Exception as e:r['error']=str(e)
finally:(R/f'result-{port}.json').write_text(json.dumps(r,indent=2));print(json.dumps(r));m.delete_session()
