# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/

from mozrunner import Runner


class GeckoInstance(object):

    required_prefs = {"marionette.defaultPrefs.enabled": True,
                      "marionette.defaultPrefs.port": 2828,
                      "browser.warnOnQuit": False}

    def __init__(self, host, port, bin, profile):
        self.marionette_host = host
        self.marionette_port = port
        self.bin = bin
        self.profile = profile
        self.runner = None

    def start(self):
        profile_path = self.profile
        profile_args = {"preferences": self.required_prefs}
        if not profile_path:
            profile_args["restore"] = False
        else:
            profile_args["profile"] = profile_path
        print "starting runner"
        self.runner = Runner.create(binary=self.bin,
                                    profile_args=profile_args,
                                    cmdargs=['-no-remote'])
        self.runner.start()

    def close(self):
        self.runner.stop()
        self.runner.cleanup()


class B2GDesktopInstance(GeckoInstance):

    required_prefs = {"focusmanager.testmode": True}

apps = {'b2gdesktop': B2GDesktopInstance}
