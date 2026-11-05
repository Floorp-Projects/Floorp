import sys,json,time,base64,traceback
from pathlib import Path
from marionette_driver.marionette import Marionette
R=Path(__file__).parent;port=int(sys.argv[1]);kind=sys.argv[2];m=Marionette('localhost',port=port,socket_timeout=210,startup_timeout=90);m.start_session();m.set_context('chrome')
r={'port':port,'test':kind,'runtimeBinding':'actual production module singleton, diagnostic exports only','observations':[]}
try:
 r['product']=m.execute_script('return {version:Services.appinfo.version,build:Services.appinfo.appBuildID,name:Services.appinfo.name,rtl:RTL_UI,dir:document.documentElement.getAttribute("dir"),sidebarMethods:{setPosition:typeof SidebarController.setPosition,toggleVerticalTabs:typeof SidebarController.toggleVerticalTabs}}')
 code=(R/'tests'/(kind+'.js')).read_text()
 r['result']=m.execute_async_script('''const code=arguments[0],done=arguments[arguments.length-1];window.__qa2877Result=null;let el=document.createElementNS("http://www.w3.org/1999/xhtml","script");el.type="module";el.textContent=code+String.fromCharCode(10)+"runAllTests().then(()=>{window.__qa2877Result={passed:true,progress:window.__NORA_TEST_PROGRESS__}}).catch(e=>{window.__qa2877Result={passed:false,error:String(e),stack:e.stack,progress:window.__NORA_TEST_PROGRESS__}});";el.addEventListener("error",e=>window.__qa2877Result={passed:false,error:"module script load error"});document.documentElement.appendChild(el);let timer=setInterval(()=>{if(window.__qa2877Result){clearInterval(timer);done(window.__qa2877Result)}},100);''',script_args=[code],script_timeout=180000)
 r['after']=m.execute_script('return {progress:window.__NORA_TEST_PROGRESS__,panelRect:document.querySelector("#panel-sidebar-box").getBoundingClientRect().toJSON(),offsetStart:document.querySelector("#browser").style.getPropertyValue("--floorp-panel-start-width"),offsetEnd:document.querySelector("#browser").style.getPropertyValue("--floorp-panel-end-width")}')
 (R/f'{port}-{kind}.png').write_bytes(base64.b64decode(m.screenshot(format='base64')))
except Exception as e:r.update(error=repr(e),traceback=traceback.format_exc())
finally:
 (R/f'{port}-{kind}.json').write_text(json.dumps(r,indent=2));print(json.dumps(r),flush=True)
 try:m.delete_session()
 except Exception:pass
if r.get('error') or not r.get('result',{}).get('passed'):sys.exit(1)
