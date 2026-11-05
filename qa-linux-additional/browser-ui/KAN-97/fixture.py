from http.server import ThreadingHTTPServer,BaseHTTPRequestHandler
HTML='''<!doctype html><meta charset="utf-8"><title>KAN97 scroll targets</title><style>html{scroll-behavior:auto}body{margin:0;height:8000px;background:repeating-linear-gradient(#eef 0 100px,#dde 100px 200px)}#rootFocus{position:fixed;right:20px;top:15px;z-index:9}#box{position:fixed;left:30px;top:90px;width:390px;height:350px;overflow:auto;border:5px solid green;background:white}#box>div{height:6000px;background:repeating-linear-gradient(#cfc 0 100px,#afa 100px 200px)}iframe{position:fixed;left:460px;top:90px;width:390px;height:350px;border:5px solid blue}h1{position:fixed;left:10px;top:0;font-size:18px}</style><body tabindex="0"><h1>QA scroll root, focused overflow and focused frame</h1><button id="rootFocus">Focus root</button><div id="box" tabindex="0"><div>OVERFLOW content</div></div><iframe id="frame" src="FRAME"></iframe>'''
CHILD='''<!doctype html><meta charset="utf-8"><title>KAN97 frame</title><style>html{scroll-behavior:auto}body{margin:0;height:7000px;background:repeating-linear-gradient(#fcc 0 100px,#faa 100px 200px)}p{position:fixed;top:0;left:0}</style><body tabindex="0"><p>QA frame scroll</p>'''
class Handler(BaseHTTPRequestHandler):
 def do_GET(self):
  if self.path.startswith('/child'):content=CHILD
  else:content=HTML.replace('FRAME','http://localhost:29972/child' if 'cross' in self.path else '/child')
  self.send_response(200);self.send_header('Content-Type','text/html');self.send_header('Cache-Control','no-store');self.end_headers();self.wfile.write(content.encode())
 def log_message(self,*args):pass
