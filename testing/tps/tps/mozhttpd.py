#!/usr/bin/python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import BaseHTTPServer
import SimpleHTTPServer
import threading
import sys
import os
import urllib
import re
from urlparse import urlparse
from SocketServer import ThreadingMixIn

DOCROOT = '.'

class EasyServer(ThreadingMixIn, BaseHTTPServer.HTTPServer):
    allow_reuse_address = True
    
class MozRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
    def translate_path(self, path):
        # It appears that the default path is '/' and os.path.join makes the '/' 
        o = urlparse(path)

        sep = '/'
        if sys.platform == 'win32':
            sep = ''

        ret = '%s%s' % ( sep, DOCROOT.strip('/') )

        # Stub out addons.mozilla.org search API, which is used when installing
        # add-ons. The version is hard-coded because we want tests to fail when
        # the API updates so we can update our stubbed files with the changes.
        if o.path.find('/en-US/firefox/api/1.5/search/guid:') == 0:
            ids = urllib.unquote(o.path[len('/en-US/firefox/api/1.5/search/guid:'):])

            if ids.count(',') > 0:
                raise Exception('Searching for multiple IDs is not supported.')

            base = ids
            at_loc = ids.find('@')
            if at_loc > 0:
                base = ids[0:at_loc]

            ret += '/%s.xml' % base

        else:
            ret += '/%s' % o.path.strip('/')

        return ret

    # I found on my local network that calls to this were timing out
    # I believe all of these calls are from log_message
    def address_string(self):
        return "a.b.c.d"

    # This produces a LOT of noise
    def log_message(self, format, *args):
        pass

class MozHttpd(object):
    def __init__(self, host="127.0.0.1", port=8888, docroot='.'):
        global DOCROOT
        self.host = host
        self.port = int(port)
        DOCROOT = docroot

    def start(self):
        self.httpd = EasyServer((self.host, self.port), MozRequestHandler)
        self.server = threading.Thread(target=self.httpd.serve_forever)
        self.server.setDaemon(True) # don't hang on exit
        self.server.start()
        #self.testServer()

    #TODO: figure this out
    def testServer(self):
        fileList = os.listdir(DOCROOT)
        filehandle = urllib.urlopen('http://%s:%s' % (self.host, self.port))
        data = filehandle.readlines();
        filehandle.close()

        for line in data:
            found = False
            # '@' denotes a symlink and we need to ignore it.
            webline = re.sub('\<[a-zA-Z0-9\-\_\.\=\"\'\/\\\%\!\@\#\$\^\&\*\(\) ]*\>', '', line.strip('\n')).strip('/').strip().strip('@')
            if webline != "":
                if webline == "Directory listing for":
                    found = True
                else:
                    for fileName in fileList:
                        if fileName == webline:
                            found = True
                
                if (found == False):
                    print "NOT FOUND: " + webline.strip()                

    def stop(self):
        if self.httpd:
            self.httpd.shutdown()
            self.httpd.server_close()

    __del__ = stop

