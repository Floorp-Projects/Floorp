from marionette_driver.marionette import Marionette
import json,time
m=Marionette('localhost',port=29837);m.start_session();print('initial',m.current_window_handle,m.window_handles)
for h in m.window_handles:
 m.switch_to_window(h);m.set_context('content');print(h,m.execute_script('return {uri:location.href,root:document.querySelector("#root")?.innerHTML.length,text:document.body.innerText.slice(0,1500),rpm:typeof window.wrappedJSObject.RPMAddMessageListener}'))
m.delete_session()
