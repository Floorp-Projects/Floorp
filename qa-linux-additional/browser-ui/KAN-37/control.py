from marionette_driver.marionette import Marionette
import json,sys,time,base64
from pathlib import Path
m=Marionette('localhost',port=29837,socket_timeout=90);m.start_session();m.set_context('content')
if sys.argv[1]=='reload':m.refresh();time.sleep(3)
if sys.argv[1]=='eval':print(json.dumps(m.execute_script(sys.argv[2]),ensure_ascii=False))
if sys.argv[1]=='click':
 r=m.execute_script('let e='+sys.argv[2]+';e.scrollIntoView({block:"center"});let r=e.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2}')
 m._send_message('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'mouse','parameters':{'pointerType':'mouse'},'actions':[{'type':'pointerMove','duration':0,'origin':'viewport','x':int(r['x']),'y':int(r['y'])},{'type':'pointerDown','button':0},{'type':'pointerUp','button':0}]}]});time.sleep(1)
if sys.argv[1]=='shot':Path(sys.argv[2]).write_bytes(base64.b64decode(m.screenshot(format='base64')))
m.delete_session()
