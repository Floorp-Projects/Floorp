import BaseHTTPServer
import threading
import zlib
import json

class PingHandler(BaseHTTPServer.BaseHTTPRequestHandler):

    def do_HEAD(s):
        s.send_response(200)
        s.send_header("Content-type", "text/html")
        s.end_headers()

    def do_GET(s):
        self.do_HEAD(self, s)
        s.wfile.write("<html>")
        s.wfile.write("  <head><title>Success</title></head>")
        s.wfile.write("  <body>")
        s.wfile.write("    <p>The server is working correctly. Firefox should send a POST request on port %d</p>" % self.server.port)
        s.wfile.write("  </body>")
        s.wfile.write("</html>")

    def do_POST(s):
        length = int(s.headers["Content-Length"])
        plainData = s.rfile.read(length)
        if s.headers.get("Content-Encoding") == "gzip":
            plainData = zlib.decompress(plainData, zlib.MAX_WBITS | 16)
        jsonData = json.loads(plainData)
        self.server.appendPing(jsonData)
        self.do_HEAD(self, s)

class PingServer(threading.Thread):

    def __init__(self):
        threading.Thread.__init__(self)
        self._httpd = BaseHTTPServer.HTTPServer(('', 0), PingHandler)
        self._receivedPings = dict()

    def run(self):
        self._httpd.serve_forever()

    def stop(self):
        self._httpd.shutdown()

    def appendPing(self, jsonData):
        self._receivedPings[jsonData["id"]] = jsonData

    def clearPings(self):
        self._receivedPings.clear()

    @property
    def pings(self):
        return self._receivedPings

    def ping(self, id):
        return self._receivedPings.get(id)

    @property
    def name(self):
        return self._httpd.server_name

    @property
    def port(self):
        return self._httpd.server_port
