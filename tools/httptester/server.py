# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Regression testing HTTP 'server'

The intent of this is to provide a (scriptable) framework for regression
testing mozilla stuff. See the docs for details.
"""

__version__ = "0.1"

import BaseHTTPServer
import string
import time
import os
from stat import *
import results

class RegressionHTTPRequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    server_version = "Regression/" + __version__

    protocol_version = "HTTP/1.1"

    def do_GET(self):
        if self.path == '/':
            return self.initial_redirect()
        if self.path[:1] != '/':
            return self.send_error(400,
                                   "Path %s does not begin with /" % `self.path`)

        try:
            id, req = string.split(self.path[1:], '/', 1)
        except ValueError:
            return self.send_error(404, "Missing id and path")

        if not req:
            # Initial request. Need to get a file list
            return self.list_tests(id)
        elif req == 'report':
            self.send_response(200)
            self.send_header('Content-Type', "text/plain")
            self.end_headers()
            res = results.results(id)
            res.write_report(self.wfile)
            del res
            return
        else:
            return self.handle_request(id, req)
            
    def initial_redirect(self):
        """Redirect the initial query.

        I don't want to use cookies, because that would bring
        wallet into all the tests. So the url will be:
        http://foo/123456/path/and/filename.html"""

        self.send_response(302)
        self.send_header("Location","/%s/" % (long(time.time()*1000)))
        self.end_headers()

    def list_tests(self, id):
        """List all test cases."""
        
        try:
            os.stat("tests")
        except IOError:
            return self.send_error(500, "Tests were not found")

        self.send_response(200)
        self.send_header('Content-Type', "text/plain")
        self.end_headers()
        return self.recurse_dir(id,"tests")

    def recurse_dir(self, id, path):
        hasDir = 0

        dir = os.listdir(path)
        dir.sort()
        
        for i in dir:
            if i == 'CVS':
              continue
            mode = os.stat(path+'/'+i)[ST_MODE]
            if S_ISDIR(mode):
                hasDir = 1
                self.recurse_dir(id,path+"/"+i)
            elif hasDir:
                print "%s: Warning! dir and non dir are mixed." % (path)

        if not hasDir:
            self.wfile.write("http://localhost:8000/%s/%s/\n" % (id, path))

    def copyfileobj(self, src, dst):
        """See shutil.copyfileobj from 2.x

        I want this to be usable with 1.5 though"""

        while 1:
            data = src.read(4096)
            if not data: break
            dst.write(data)

    default_reply = "Testcase %s for %s loaded\n"

    def handle_request(self, id, req):
        """Answer a request

        We first look for a file with the name of the request.
        If that exists, then we spit that out, otherwise we
        open req.headers and req.body (if available) separately.

        Why would you want to split it out?
        a) binary files
        b) Separating it out will send the 'standard' headers,
        and handle the Connection: details for you, if you're
        not testing that.
        c) You don't need to come up with your own body"""

        res = results.results(id)

        path = string.join(string.split(req, '/')[:-1], '/')

        path = path + '/'

        tester = res.get_tester(path)

        self.fname = string.split(req,'/')[-1]

        if not self.fname:
            self.fname = tester.baseName

        if not tester.verify_request(self):
            res.set_tester(req, tester)
            return self.send_error(400, tester.reason)

        ### perhaps this isn't the best design model...
        res.set_tester(path, tester)

        del res

        if req[-1:] == '/':
            req = req + tester.baseName
        
        try:
            f = open(req, 'rb')
            self.log_message('"%s" sent successfully for %s',
                             self.requestline,
                             id)
            self.copyfileobj(f,self.wfile)
            return f.close()
        except IOError:
            try:
                f = open(req+".headers", 'rb')
            except IOError:
                return self.send_error(404, "File %s not found" % (req))
        
        self.send_response(f.readline())
        # XXX - I should parse these out, and use send_header instead
        # so that I can change behaviour (like keep-alive...)
        # But then I couldn't test 'incorrect' header formats
        self.copyfileobj(f,self.wfile)
        f.close()

        try:
            f = open(req+".body", 'rb')
            ## XXXXX - Need to configify this
            ## and then send content-length, etc
            self.end_headers()
            self.copyfileobj(f, self.wfile)
            return f.close()
        except IOError:
            self.send_header('Content-Type', "text/plain")
            body = self.default_reply % (req, id)
            self.send_header('Content-Length', len(body))
            self.end_headers()
            self.wfile.write(body)

    def send_response(self, line, msg=None):
        if msg:
            return BaseHTTPServer.BaseHTTPRequestHandler.send_response(self, line,msg)
        try:
            x = int(line)
            BaseHTTPServer.BaseHTTPRequestHandler.send_response(self, x)
        except ValueError:
            tuple = string.split(line, ' ',2)
            ## XXXX
            old = self.protocol_version
            self.protocol_version = tuple[0]
            BaseHTTPServer.BaseHTTPRequestHandler.send_response(self, int(tuple[1]), tuple[2][:-1])
            self.protocol_version = old

import socket

# I need to thread this, with the mixin class
class RegressionHTTPServer(BaseHTTPServer.HTTPServer):
    # The 1.5.2 version doesn't do this:
    def server_bind(self):
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        BaseHTTPServer.HTTPServer.server_bind(self)

def run(HandlerClass = RegressionHTTPRequestHandler,
        ServerClass = RegressionHTTPServer):
    BaseHTTPServer.test(HandlerClass, ServerClass)

if __name__ == '__main__':
    run()
