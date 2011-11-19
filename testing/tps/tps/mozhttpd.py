#!/usr/bin/python
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Joel Maher <joel.maher@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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
        return "%s%s" % ('' if sys.platform == 'win32' else '/', '/'.join([i.strip('/') for i in (DOCROOT, o.path)]))

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
        
    __del__ = stop

