# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/

from copy import deepcopy
import os
import sys
import tempfile
import time

from mozprofile import Profile
from mozrunner import Runner


class GeckoInstance(object):

    required_prefs = {
        "marionette.defaultPrefs.enabled": True,
        "marionette.logging": True,
        "browser.displayedE10SPrompt": 5,
        "browser.displayedE10SPrompt.1": 5,
        "browser.displayedE10SPrompt.2": 5,
        "browser.displayedE10SPrompt.3": 5,
        "browser.displayedE10SPrompt.4": 5,
        "browser.sessionstore.resume_from_crash": False,
        "browser.shell.checkDefaultBrowser": False,
        "browser.startup.page": 0,
        "browser.tabs.remote.autostart": False,
        "browser.tabs.remote.autostart.1": False,
        "browser.tabs.remote.autostart.2": False,
        "browser.urlbar.userMadeSearchSuggestionsChoice": True,
        "browser.warnOnQuit": False,
        "dom.ipc.reportProcessHangs": False,
        "datareporting.healthreport.service.enabled": False,
        "datareporting.healthreport.uploadEnabled": False,
        "datareporting.healthreport.service.firstRun": False,
        "datareporting.healthreport.logging.consoleEnabled": False,
        "datareporting.policy.dataSubmissionEnabled": False,
        "datareporting.policy.dataSubmissionPolicyAccepted": False,
        "focusmanager.testmode": True,
        "startup.homepage_welcome_url": "about:blank",
        "toolkit.telemetry.enabled": False,
    }

    def __init__(self, host, port, bin, profile=None, addons=None,
                 app_args=None, symbols_path=None, gecko_log=None, prefs=None,
                 workspace=None):
        self.marionette_host = host
        self.marionette_port = port
        self.bin = bin
        # Alternative to default temporary directory
        self.workspace = workspace
        # Check if it is a Profile object or a path to profile
        self.profile = None
        self.addons = addons
        if isinstance(profile, Profile):
            self.profile = profile
        else:
            self.profile_path = profile
        self.prefs = prefs
        self.required_prefs = deepcopy(GeckoInstance.required_prefs)
        if prefs:
            self.required_prefs.update(prefs)
        self.app_args = app_args or []
        self.runner = None
        self.symbols_path = symbols_path

        if gecko_log != '-':
            if gecko_log is None:
                gecko_log = 'gecko.log'
            elif os.path.isdir(gecko_log):
                fname = 'gecko-%d.log' % time.time()
                gecko_log = os.path.join(gecko_log, fname)

            gecko_log = os.path.realpath(gecko_log)
            if os.access(gecko_log, os.F_OK):
                os.remove(gecko_log)

        self.gecko_log = gecko_log

    def start(self):
        profile_args = {"preferences": deepcopy(self.required_prefs)}
        profile_args["preferences"]["marionette.defaultPrefs.port"] = self.marionette_port
        if self.prefs:
            profile_args["preferences"].update(self.prefs)
        if '-jsdebugger' in self.app_args:
            profile_args["preferences"].update({
                "devtools.browsertoolbox.panel": "jsdebugger",
                "devtools.debugger.remote-enabled": True,
                "devtools.chrome.enabled": True,
                "devtools.debugger.prompt-connection": False,
                "marionette.debugging.clicktostart": True,
            })
        if self.addons:
            profile_args['addons'] = self.addons

        if hasattr(self, "profile_path") and self.profile is None:
            if not self.profile_path:
                if self.workspace:
                    profile_args['profile'] = tempfile.mkdtemp(
                        suffix='.mozrunner-{:.0f}'.format(time.time()),
                        dir=self.workspace)
                self.profile = Profile(**profile_args)
            else:
                profile_args["path_from"] = self.profile_path
                profile_name = '{}-{:.0f}'.format(
                    os.path.basename(self.profile_path),
                    time.time()
                )
                if self.workspace:
                    profile_args["path_to"] = os.path.join(self.workspace,
                                                           profile_name)
                self.profile = Profile.clone(**profile_args)

        process_args = {
            'processOutputLine': [NullOutput()],
        }

        if self.gecko_log == '-':
            process_args['stream'] = sys.stdout
        else:
            process_args['logfile'] = self.gecko_log

        env = os.environ.copy()

        # environment variables needed for crashreporting
        # https://developer.mozilla.org/docs/Environment_variables_affecting_crash_reporting
        env.update({ 'MOZ_CRASHREPORTER': '1',
                     'MOZ_CRASHREPORTER_NO_REPORT': '1', })
        self.runner = Runner(
            binary=self.bin,
            profile=self.profile,
            cmdargs=['-no-remote', '-marionette'] + self.app_args,
            env=env,
            symbols_path=self.symbols_path,
            process_args=process_args)
        self.runner.start()

    def close(self, restart=False):
        if not restart:
            self.profile = None

        if self.runner:
            self.runner.stop()
            self.runner.cleanup()

    def restart(self, prefs=None, clean=True):
        self.close(restart=True)

        if clean:
            self.profile.cleanup()
            self.profile = None

        if prefs:
            self.prefs = prefs
        else:
            self.prefs = None
        self.start()

class B2GDesktopInstance(GeckoInstance):
    def __init__(self, host, port, bin, **kwargs):
        # Pass a profile and change the binary to -bin so that
        # the built-in gaia profile doesn't get touched.
        if kwargs.get('profile', None) is None:
            # GeckoInstance.start will clone the profile.
            kwargs['profile'] = os.path.join(os.path.dirname(bin),
                                             'gaia',
                                             'profile')
        if '-bin' not in os.path.basename(bin):
            if bin.endswith('.exe'):
                newbin = bin[:-len('.exe')] + '-bin.exe'
            else:
                newbin = bin + '-bin'
            if os.path.exists(newbin):
                bin = newbin
        super(B2GDesktopInstance, self).__init__(host, port, bin, **kwargs)
        if not self.prefs:
            self.prefs = {}
        self.prefs["focusmanager.testmode"] = True
        self.app_args += ['-chrome', 'chrome://b2g/content/shell.html']


class DesktopInstance(GeckoInstance):
    desktop_prefs = {
        'app.update.auto': False,
        'app.update.enabled': False,
        'browser.dom.window.dump.enabled': True,
        # Bug 1145668 - Has to be reverted to about:blank once Marionette
        # can correctly handle error pages
        'browser.newtab.url': 'about:newtab',
        'browser.newtabpage.enabled': False,
        'browser.reader.detectedFirstArticle': True,
        'browser.safebrowsing.enabled': False,
        'browser.safebrowsing.malware.enabled': False,
        'browser.search.update': False,
        'browser.tabs.animate': False,
        'browser.tabs.warnOnClose': False,
        'browser.tabs.warnOnOpen': False,
        'browser.uitour.enabled': False,
        'dom.report_all_js_exceptions': True,
        'extensions.enabledScopes': 5,
        'extensions.autoDisableScopes': 10,
        'extensions.getAddons.cache.enabled': False,
        'extensions.installDistroAddons': False,
        'extensions.logging.enabled': True,
        'extensions.showMismatchUI': False,
        'extensions.update.enabled': False,
        'extensions.update.notifyUser': False,
        'geo.provider.testing': True,
        'javascript.options.showInConsole': True,
        # See Bug 1221187 - marionette logging is too verbose, especially for
        # long-running tests.
        'marionette.logging': False,
        'security.notification_enable_delay': 0,
        'signon.rememberSignons': False,
        'toolkit.startup.max_resumed_crashes': -1,
    }

    def __init__(self, *args, **kwargs):
        super(DesktopInstance, self).__init__(*args, **kwargs)
        self.required_prefs.update(DesktopInstance.desktop_prefs)

class NullOutput(object):
    def __call__(self, line):
        pass


apps = {
    'b2g': B2GDesktopInstance,
    'b2gdesktop': B2GDesktopInstance,
    'fxdesktop': DesktopInstance,
}
