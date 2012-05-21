# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Store the results.

Use a db, but we could do better"""

import shelve
import string

class results:
    def __init__(self, id):
        self.id = id
        self.d = shelve.open("data/"+id+".db")

    def get_tester(self, path):
        import BaseTest

        try:
            fname = path+"tester.py"
            text = open(fname).read()
            # Thanks to markh, for showing me how to do this
            # Python Is Cool.
            codeob = compile(text, fname, "exec")
            namespace = { 'BaseTester': BaseTest.tester }
            exec codeob in namespace, namespace
            tester = namespace['tester']()
        except IOError:
            tester =  BaseTest.tester()

        if self.d.has_key(path):
            tester.__dict__ = self.d[path]
        else:
            tester.parse_config(open(path+"config"))

        return tester

    def set_tester(self, path, test):
        self.d[path] = test.__dict__

    def write_report(self, file):
        for i in self.d.keys():
            file.write("%s: " % (i))
            tester = self.get_tester(i)
            res, detail = tester.result()
            if res:
                file.write("Pass!\n")
            else:
                file.write("Fail: %s\n" % (detail))

    def __del__(self):
        self.d.close()
