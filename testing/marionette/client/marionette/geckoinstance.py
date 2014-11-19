# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/

from copy import deepcopy
import errno
import platform
import os
import sys
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
                      "browser.warnOnQuit": False,
                      "browser.displayedE10SPrompt": 5,
                      "browser.displayedE10SPrompt.1": 5,
                      "browser.displayedE10SPrompt.2": 5,
                      "browser.displayedE10SPrompt.3": 5,
                      "browser.displayedE10SPrompt.4": 5,
                      "browser.tabs.remote.autostart.1": False,
                      "browser.tabs.remote.autostart.2": False}

    def __init__(self, host, port, bin, profile, app_args=None, symbols_path=None,
                  gecko_log=None, prefs=None):
        self.marionette_host = host
        self.marionette_port = port
        self.bin = bin
        self.profile_path = profile
        self.prefs = prefs
        self.app_args = app_args or []
        self.runner = None
        self.symbols_path = symbols_path
        self.gecko_log = gecko_log

    def start(self):
        profile_args = {"preferences": deepcopy(self.required_prefs)}
        if self.prefs:
            profile_args["preferences"].update(self.prefs)
        if not self.profile_path:
            profile_args["restore"] = False
            profile = Profile(**profile_args)
        else:
            profile_args["path_from"] = self.profile_path
            profile = Profile.clone(**profile_args)

        process_args = {
            'processOutputLine': [NullOutput()],
        }

        if self.gecko_log == '-':
            process_args['stream'] = sys.stdout
        else:
            if self.gecko_log is None:
                self.gecko_log = 'gecko.log'
            elif os.path.isdir(self.gecko_log):
                fname = "gecko-%d.log" % time.time()
                self.gecko_log = os.path.join(self.gecko_log, fname)

            self.gecko_log = os.path.realpath(self.gecko_log)
            if os.access(self.gecko_log, os.F_OK):
                if platform.system() is 'Windows':
                    # NOTE: windows has a weird filesystem where it happily 'closes'
                    # the file, but complains if you try to delete it. You get a
                    # 'file still in use' error. Sometimes you can wait a bit and
                    # a retry will succeed.
                    # If all retries fail, we'll just continue without removing
                    # the file. In this case, if we are restarting the instance,
                    # then the new logs just get appended to the old file.
                    tries = 0
                    while tries < 10:
                        try:
                            os.remove(self.gecko_log)
                            break
                        except WindowsError as e:
                            if e.errno == errno.EACCES:
                                tries += 1
                                time.sleep(0.5)
                            else:
                                raise e
                else:
                    os.remove(self.gecko_log)

            process_args['logfile'] = self.gecko_log

        env = os.environ.copy()

        # environment variables needed for crashreporting
        # https://developer.mozilla.org/docs/Environment_variables_affecting_crash_reporting
        env.update({ 'MOZ_CRASHREPORTER': '1',
                     'MOZ_CRASHREPORTER_NO_REPORT': '1', })
        self.runner = Runner(
            binary=self.bin,
            profile=profile,
            cmdargs=['-no-remote', '-marionette'] + self.app_args,
            env=env,
            symbols_path=self.symbols_path,
            process_args=process_args)
        self.runner.start()

    def close(self):
        if self.runner:
            self.runner.stop()
            self.runner.cleanup()

    def restart(self, prefs=None):
        self.close()
        if prefs:
            self.prefs = prefs
        else:
            self.prefs = None
        self.start()

class B2GDesktopInstance(GeckoInstance):
    required_prefs = {"focusmanager.testmode": True}

    def __init__(self, **kwargs):
        super(B2GDesktopInstance, self).__init__(**kwargs)
        self.app_args += ['-chrome', 'chrome://b2g/content/shell.html']

class NullOutput(object):
    def __call__(self, line):
        pass


apps = {'b2g': B2GDesktopInstance,
        'b2gdesktop': B2GDesktopInstance}
