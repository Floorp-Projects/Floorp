import SimpleHTTPServer
import SocketServer

# minimal web server.  serves files relative to the
# current directory.

PORT = 8888

Handler = SimpleHTTPServer.SimpleHTTPRequestHandler
Handler.extensions_map[".xhtml"] = "application/xhtml+xml"
Handler.extensions_map[".svg"] = "image/svg+xml"
Handler.extensions_map[".xul"] = "application/vnd.mozilla.xul+xml"
httpd = SocketServer.TCPServer(("", PORT), Handler)

print "serving at port", PORT
httpd.serve_forever()
