import pathlib,json,time,traceback
from marionette_driver.marionette import Marionette
P=pathlib.Path(__file__).parent
m=Marionette(host='127.0.0.1',port=28899,socket_timeout=120);m.start_session({'acceptInsecureCerts':False});m.timeout.script=30
if True:
 last=0
 while True:
  c=P/'control.json'
  if c.exists() and c.stat().st_mtime_ns!=last:
   last=c.stat().st_mtime_ns;d=json.loads(c.read_text())
   if d.get('stop'):
    m.set_context('chrome');m.execute_script('setTimeout(()=>Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit),100);return true;');break
   try:

    if d.get('select') is not None:m.switch_to_window(m.window_handles[d['select']])
    if d.get('uri'):
     for h in m.window_handles:
      m.switch_to_window(h);m.set_context('content')
      if d['uri'] in m.execute_script('return location.href;'):break
    if d.get('frame'):
     m.set_context('content');m.switch_to_frame(m.find_element('css selector',d['frame']))
    if d.get('click'):
     m.set_context('content');m.find_element('css selector',d['click']).click()
    if d.get('keys'):
     m.set_context('content');m.find_element('css selector',d.get('target','body')).send_keys(d['keys'])
    if d.get('screenshot'):(P/d['screenshot']).write_bytes(__import__('base64').b64decode(m.screenshot()))
    m.set_context(d.get('context','chrome'));r=m.execute_async_script(d['script']) if d.get('async') else m.execute_script(d['script']);(P/('result-'+d['id']+'.json')).write_text(json.dumps(r,indent=2));print(d['id'],'done',flush=True)
   except Exception as e:(P/('result-'+d['id']+'.json')).write_text(json.dumps({'error':str(e),'trace':traceback.format_exc()},indent=2));print(d['id'],'error',flush=True)
  time.sleep(.3)
