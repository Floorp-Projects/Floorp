# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/

import os
import time

from mozprofile import Profile
from mozrunner import Runner


class GeckoInstance(object):

    required_prefs = {"marionette.defaultPrefs.enabled": True,
                      "marionette.defaultPrefs.port": 2828,
                      "marionette.logging": True,
                      "startup.homepage_welcome_url": "about:blank",
                      "browser.shell.checkDefaultBrowser": False,
                      "browser.startup.page": 0,
                      "browser.sessionstore.resume_from_crash": False,
                      "browser.warnOnQuit": False}

    def __init__(self, host, port, bin, profile, app_args=None, symbols_path=None,
                  gecko_log=None):
        self.marionette_host = host
        self.marionette_port = port
        self.bin = bin
        self.profile = profile
        self.app_args = app_args or []
        self.runner = None
        self.symbols_path = symbols_path
        self.gecko_log = gecko_log

    def start(self):
        profile_path = self.profile
        profile_args = {"preferences": self.required_prefs}
        if not profile_path:
            runner_class = Runner
            profile_args["restore"] = False
        else:
            runner_class = CloneRunner
            profile_args["path_from"] = profile_path

        if self.gecko_log is None:
            self.gecko_log = 'gecko.log'
        elif os.path.isdir(self.gecko_log):
            fname = "gecko-%d.log" % time.time()
            self.gecko_log = os.path.join(self.gecko_log, fname)

        self.gecko_log = os.path.realpath(self.gecko_log)
        if os.access(self.gecko_log, os.F_OK):
            os.remove(self.gecko_log)

        env = os.environ.copy()

        # environment variables needed for crashreporting
        # https://developer.mozilla.org/docs/Environment_variables_affecting_crash_reporting
        env.update({ 'MOZ_CRASHREPORTER': '1',
                     'MOZ_CRASHREPORTER_NO_REPORT': '1', })
        self.runner = runner_class.create(
            binary=self.bin,
            profile_args=profile_args,
            cmdargs=['-no-remote', '-marionette'] + self.app_args,
            env=env,
            symbols_path=self.symbols_path,
            kp_kwargs={
                'processOutputLine': [NullOutput()],
                'logfile': self.gecko_log})
        self.runner.start()

    def check_for_crashes(self):
        return self.runner.check_for_crashes()

    def close(self):
        self.runner.stop()
        self.runner.cleanup()


class B2GDesktopInstance(GeckoInstance):

    required_prefs = {"focusmanager.testmode": True}

apps = {'b2g': B2GDesktopInstance,
        'b2gdesktop': B2GDesktopInstance}


class CloneRunner(Runner):

    profile_class = Profile.clone


class NullOutput(object):

    def __call__(self, line):
        pass
