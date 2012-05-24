# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/

import datetime
import socket
import time
from mozrunner import Runner


class GeckoInstance(object):

    def __init__(self, host, port, bin, profile):
        self.marionette_host = host
        self.marionette_port = port
        self.bin = bin
        self.profile = profile
        self.runner = None

    def start(self):
        profile = self.profile
        if not profile:
            prefs = {"dom.allow_XUL_XBL_for_file": True,
                     "marionette.defaultPrefs.enabled": True,
                     "marionette.defaultPrefs.port": 2828}
            profile = {"preferences": prefs, "restore":False}
        print "starting runner"
        self.runner = Runner.create(binary=self.bin, profile_args=profile, cmdargs=['-no-remote'])
        self.runner.start()

    def close(self):
        self.runner.stop()
        self.runner.cleanup()

    def wait_for_port(self, timeout=3000):
        assert(self.marionette_port)
        starttime = datetime.datetime.now()
        while datetime.datetime.now() - starttime < datetime.timedelta(seconds=timeout):
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.connect((self.marionette_host, self.marionette_port))
                data = sock.recv(16)
                print "closing socket"
                sock.close()
                if '"from"' in data:
                    print "got data"
                    time.sleep(5)
                    return True
            except:
                import traceback
                print traceback.format_exc()
            time.sleep(1)
        return False
