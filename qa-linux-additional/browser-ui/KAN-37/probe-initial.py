from marionette_driver.marionette import Marionette
from pathlib import Path
import json,base64
R=Path(__file__).parent;m=Marionette('localhost',port=29837,socket_timeout=90);m.start_session();m.set_context('chrome');print(m.execute_script('return {uri:gBrowser.currentURI.spec,design:Services.prefs.getStringPref("floorp.design.configs"),newtab:Services.prefs.getStringPref("browser.newtab.url", "unset")}'))
m.set_context('content');print(m.execute_script('return {uri:location.href,title:document.title,text:document.body.innerText.slice(0,1500),images:[...document.images].slice(0,20).map(x=>({src:x.src,w:x.naturalWidth,c:x.className})),links:[...document.querySelectorAll("a")].slice(0,20).map(x=>({text:x.textContent,url:x.href,class:x.className}))}'))
(R/'initial.png').write_bytes(base64.b64decode(m.screenshot(format='base64')));m.delete_session()
