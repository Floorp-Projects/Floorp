from pathlib import Path
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlsplit, parse_qs, quote
import json, time, threading, queue, runpy, re
MIDDLE='''<!doctype html><meta charset="utf-8"><title>KAN97 nested middle</title><style>html{scroll-behavior:auto}body{margin:0;height:7000px;background:repeating-linear-gradient(#cff 0 100px,#aff 100px 200px)}iframe{position:fixed;inset:0;width:100%;height:100%;border:0}</style><body tabindex="0"><iframe id="leaf" src="LEAF"></iframe>'''
BASE=runpy.run_path(str(Path(__file__).parents[1]/'fixture.py'))
LOCK=threading.Lock();STATE={};CLIENTS={};SERIAL=0
PUBLISHER=r'''
<script>
(()=>{
 const token=TOKEN,role=ROLE,auto=AUTO;
 function sample(nonce=0){
  const box=document.getElementById('box');
  const value={top:scrollY,max:document.scrollingElement.scrollHeight-innerHeight,
   overflow:box?{top:box.scrollTop,max:box.scrollHeight-box.clientHeight}:null,
   active:document.activeElement?.id||document.activeElement?.localName,
   documentFocused:document.hasFocus(),visibility:document.visibilityState,nonce};
  fetch('/state?token='+encodeURIComponent(token)+'&role='+role,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(value),keepalive:true}).catch(()=>{});
 }
 const source=new EventSource('/sample-stream?token='+encodeURIComponent(token)+'&role='+role);
 source.onmessage=e=>sample(Number(e.data));
 for(const n of ['scroll','focusin','focusout','visibilitychange','resize'])addEventListener(n,()=>sample(),true);
 addEventListener('pagehide',()=>source.close(),{once:true});
 function ready(){document.title='KAN97 full '+token;if(auto){scrollTo({top:1800,behavior:'instant'});const b=document.getElementById('box');if(b)b.scrollTop=1800;}sample();}
 if(document.readyState==='loading')addEventListener('DOMContentLoaded',ready,{once:true});else ready();
})();
</script>
'''
class Server(ThreadingHTTPServer):daemon_threads=True
class Handler(BaseHTTPRequestHandler):
 protocol_version='HTTP/1.1'
 def reply(self,status,content,ctype='application/json'):
  data=content if isinstance(content,bytes) else content.encode();self.send_response(status);self.send_header('Content-Type',ctype);self.send_header('Content-Length',str(len(data)));self.send_header('Cache-Control','no-store');self.end_headers();self.wfile.write(data)
 def params(self):
  u=urlsplit(self.path);q=parse_qs(u.query);token=q.get('token',['matrix'])[0];role=q.get('role',['root'])[0]
  if not re.fullmatch(r'[a-zA-Z0-9_-]{1,100}',token):raise ValueError('invalid fixture token')
  return u.path,q,token,role
 def do_POST(self):
  try:
   path,q,token,role=self.params();assert path=='/state' and role in ['root','frame','leaf'];size=int(self.headers.get('Content-Length','0'));assert 0<size<8192;value=json.loads(self.rfile.read(size));value['seenAt']=time.monotonic()
   with LOCK:
    prior=STATE.setdefault(token,{}).get(role,{})
    value['nonce']=max(value.get('nonce',0),prior.get('nonce',0));STATE[token][role]=value
   self.reply(200,'{}')
  except Exception as e:self.reply(400,json.dumps({'fixtureError':str(e)}))
 def do_GET(self):
  global SERIAL
  try:
   path,q,token,role=self.params()
   if path=='/sample-stream':
    chan=queue.Queue()
    with LOCK:CLIENTS.setdefault(token,{}).setdefault(role,[]).append(chan)
    self.send_response(200);self.send_header('Content-Type','text/event-stream');self.send_header('Cache-Control','no-store');self.send_header('Connection','keep-alive');self.end_headers()
    try:
     self.wfile.write(b': ready\n\n');self.wfile.flush()
     while True:
      try:nonce=chan.get(timeout=8);data=f'data: {nonce}\n\n'.encode()
      except queue.Empty:data=b': keepalive\n\n'
      self.wfile.write(data);self.wfile.flush()
    except (BrokenPipeError,ConnectionResetError):pass
    finally:
     with LOCK:
      entries=CLIENTS.get(token,{}).get(role,[])
      if chan in entries:entries.remove(chan)
    return
   if path=='/snapshot':
    tokens=q.get('tokens',[''])[0].split(',');deadline=time.monotonic()+5
    with LOCK:
     SERIAL+=1;nonce=SERIAL
     for t in tokens:
      for channels in CLIENTS.get(t,{}).values():
       for c in channels:c.put(nonce)
    lastNotify=0
    while time.monotonic()<deadline:
     with LOCK:
      data={t:dict(STATE.get(t,{})) for t in tokens}
      if time.monotonic()-lastNotify>.2:
       for t in tokens:
        for k in ['root','frame']:
         if data[t].get(k,{}).get('nonce')!=nonce:
          for c in CLIENTS.get(t,{}).get(k,[]):c.put(nonce)
       lastNotify=time.monotonic()
     if all(all(data[t].get(k,{}).get('nonce')==nonce for k in ['root','frame']) for t in tokens):break
     time.sleep(.03)
    self.reply(200,json.dumps({'nonce':nonce,'serverNow':time.monotonic(),'values':data,'fresh':all(all(data[t].get(k,{}).get('nonce')==nonce for k in ['root','frame']) for t in tokens)}));return
   child=path.startswith('/child');middle=path.startswith('/middle');auto=q.get('auto',['0'])[0]=='1'
   if child:html=BASE['CHILD']
   elif middle:
    leaf=('http://localhost:29972/child' if q.get('cross',['0'])[0]=='1' else '/child')+'?token='+quote(token)+'&role=leaf&auto='+str(int(auto));html=MIDDLE.replace('LEAF',leaf)
   else:
    if 'nested' in path:childurl='/middle?token='+quote(token)+'&cross='+str(int('cross' in path))+'&auto='+str(int(auto))
    else:childurl=('http://localhost:29972/child' if 'cross' in path else '/child')+'?token='+quote(token)+'&auto='+str(int(auto))
    html=BASE['HTML'].replace('FRAME',childurl)
   pageRole=('leaf' if role=='leaf' else 'frame') if child or middle else 'root'
   script=PUBLISHER.replace('TOKEN',json.dumps(token)).replace('ROLE',json.dumps(pageRole)).replace('AUTO','true' if auto else 'false')
   self.reply(200,html+script,'text/html; charset=utf-8')
  except Exception as e:self.reply(400,json.dumps({'fixtureError':str(e)}))
 def log_message(self,*args):pass
