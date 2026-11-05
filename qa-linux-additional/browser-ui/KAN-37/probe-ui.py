from marionette_driver.marionette import Marionette
import json,sys,time
m=Marionette('localhost',port=29837,socket_timeout=90);m.start_session();m.set_context('content')
if len(sys.argv)>1:m.navigate(sys.argv[1]);time.sleep(2)
print(json.dumps(m.execute_script('return {uri:location.href,text:document.body.innerText.slice(0,20000),buttons:[...document.querySelectorAll("button,input,select")].map(e=>({tag:e.localName,type:e.type,id:e.id,text:e.innerText,label:e.getAttribute("aria-label"),checked:e.checked}))}'),ensure_ascii=False));m.delete_session()
