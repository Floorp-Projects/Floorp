# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""The test case itsself, and associated stuff"""

import string

class BaseTest:
    def __init__(self):
        self.res = -1
        self.reason = None
    
    def parse_config(self, config):
        self.files = []

        for line in config.readlines():
            line = string.strip(line)
            self.files.append({'file': line, 'read': 0})

    baseName = 'index'

    def verify_request(self, req):
        """Check that the request is valid.

        Also needs to update any internal 'read' stuff"""
        
        ## XXXXX
        ## This needs to be done using exceptions, maybe
        ## XXXXX
        
        for i in self.files:
            if i['file'] == req.fname:
                if i['read'] == 1:
                    self.res = 0
                    self.reason = "File %s was read twice" % (req.fname)
                    return 0
                i['read'] = 1
                break
            elif i['read'] == 0:
                self.res = 0
                self.reason = "File %s requested, expected %s" % (req.fname, i['file'])
                return 0

        ### Simplistic for now...
        res = req.headers.getheader('Host')

        return res

    def result(self):
        if self.res == -1:
            for i in self.files:
                if i['read'] == 0:
                    self.res = 0
                    self.reason = "%s not read" % (i['file'])
                    return self.res, self.reason
            self.res = 1
            
        return self.res, self.reason

tester = BaseTest
