# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/

import os
import sys
import tempfile
import time

from copy import deepcopy

from mozprofile import Profile
from mozrunner import Runner, FennecEmulatorRunner


class GeckoInstance(object):
    required_prefs = {
        "browser.sessionstore.resume_from_crash": False,
        "browser.shell.checkDefaultBrowser": False,
        # Needed for branded builds to prevent opening a second tab on startup
        "browser.startup.homepage_override.mstone": "ignore",
        "browser.startup.page": 0,
        "browser.tabs.remote.autostart.1": False,
        "browser.tabs.remote.autostart.2": False,
        "browser.tabs.remote.autostart": False,
        "browser.urlbar.userMadeSearchSuggestionsChoice": True,
        "browser.warnOnQuit": False,
        "datareporting.healthreport.logging.consoleEnabled": False,
        "datareporting.healthreport.service.enabled": False,
        "datareporting.healthreport.service.firstRun": False,
        "datareporting.healthreport.uploadEnabled": False,
        "datareporting.policy.dataSubmissionEnabled": False,
        "datareporting.policy.dataSubmissionPolicyAccepted": False,
        "dom.ipc.reportProcessHangs": False,
        # Only install add-ons from the profile and the application scope
        # Also ensure that those are not getting disabled.
        # see: https://developer.mozilla.org/en/Installing_extensions
        "extensions.enabledScopes": 5,
        "extensions.autoDisableScopes": 10,
        "focusmanager.testmode": True,
        "marionette.defaultPrefs.enabled": True,
        "startup.homepage_welcome_url": "",
        "startup.homepage_welcome_url.additional": "",
        "toolkit.telemetry.enabled": False,
        # Until Bug 1238095 is fixed, we have to enable CPOWs in order
        # for Marionette tests to work properly.
        "dom.ipc.cpows.forbid-unsafe-from-browser": False,
    }

    def __init__(self, host, port, bin, profile=None, addons=None,
                 app_args=None, symbols_path=None, gecko_log=None, prefs=None,
                 workspace=None, verbose=0):
        self.runner_class = Runner
        self.app_args = app_args or []
        self.runner = None
        self.symbols_path = symbols_path
        self.binary = bin

        self.marionette_host = host
        self.marionette_port = port
        # Alternative to default temporary directory
        self.workspace = workspace
        self.addons = addons
        # Check if it is a Profile object or a path to profile
        self.profile = None
        if isinstance(profile, Profile):
            self.profile = profile
        else:
            self.profile_path = profile
        self.prefs = prefs
        self.required_prefs = deepcopy(self.required_prefs)
        if prefs:
            self.required_prefs.update(prefs)

        self._gecko_log_option = gecko_log
        self._gecko_log = None
        self.verbose = verbose

    @property
    def gecko_log(self):
        if self._gecko_log:
            return self._gecko_log

        path = self._gecko_log_option
        if path != '-':
            if path is None:
                path = 'gecko.log'
            elif os.path.isdir(path):
                fname = 'gecko-%d.log' % time.time()
                path = os.path.join(path, fname)

            path = os.path.realpath(path)
            if os.access(path, os.F_OK):
                os.remove(path)

        self._gecko_log = path
        return self._gecko_log

    def _update_profile(self):
        profile_args = {"preferences": deepcopy(self.required_prefs)}
        profile_args["preferences"]["marionette.defaultPrefs.port"] = self.marionette_port
        if self.prefs:
            profile_args["preferences"].update(self.prefs)
        if self.verbose:
            level = "TRACE" if self.verbose >= 2 else "DEBUG"
            profile_args["preferences"]["marionette.logging"] = level
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

    def start(self):
        self._update_profile()
        self.runner = self.runner_class(**self._get_runner_args())
        self.runner.start()

    def _get_runner_args(self):
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
        env.update({'MOZ_CRASHREPORTER': '1',
                    'MOZ_CRASHREPORTER_NO_REPORT': '1'})

        return {
            'binary': self.binary,
            'profile': self.profile,
            'cmdargs': ['-no-remote', '-marionette'] + self.app_args,
            'env': env,
            'symbols_path': self.symbols_path,
            'process_args': process_args
        }

    def close(self, restart=False):
        if not restart:
            self.profile = None

        if self.runner:
            self.runner.stop()
            self.runner.cleanup()

    def restart(self, prefs=None, clean=True):
        self.close(restart=True)

        if clean and self.profile:
            self.profile.cleanup()
            self.profile = None

        if prefs:
            self.prefs = prefs
        else:
            self.prefs = None
        self.start()


class FennecInstance(GeckoInstance):
    def __init__(self, emulator_binary=None, avd_home=None, avd=None,
                 adb_path=None, serial=None, connect_to_running_emulator=False,
                 package_name=None, *args, **kwargs):
        super(FennecInstance, self).__init__(*args, **kwargs)
        self.runner_class = FennecEmulatorRunner
        # runner args
        self._package_name = package_name
        self.emulator_binary = emulator_binary
        self.avd_home = avd_home
        self.adb_path = adb_path
        self.avd = avd
        self.serial = serial
        self.connect_to_running_emulator = connect_to_running_emulator

    @property
    def package_name(self):
        """
        Name of app to run on emulator.

        Note that FennecInstance does not use self.binary
        """
        if self._package_name is None:
            self._package_name = 'org.mozilla.fennec'
            user = os.getenv('USER')
            if user:
                self._package_name += '_' + user
        return self._package_name

    def start(self):
        self._update_profile()
        self.runner = self.runner_class(**self._get_runner_args())
        try:
            if self.connect_to_running_emulator:
                self.runner.device.connect()
            self.runner.start()
        except Exception as e:
            exc, val, tb = sys.exc_info()
            message = 'Error possibly due to runner or device args: {}'
            raise exc, message.format(e.message), tb
        # gecko_log comes from logcat when running with device/emulator
        logcat_args = {
            'filterspec': 'Gecko',
            'serial': self.runner.device.dm._deviceSerial
        }
        if self.gecko_log == '-':
            logcat_args['stream'] = sys.stdout
        else:
            logcat_args['logfile'] = self.gecko_log
        self.runner.device.start_logcat(**logcat_args)
        self.runner.device.setup_port_forwarding(
            local_port=self.marionette_port,
            remote_port=self.marionette_port,
        )

    def _get_runner_args(self):
        process_args = {
            'processOutputLine': [NullOutput()],
        }

        runner_args = {
            'app': self.package_name,
            'avd_home': self.avd_home,
            'adb_path': self.adb_path,
            'binary': self.emulator_binary,
            'profile': self.profile,
            'cmdargs': ['-marionette'] + self.app_args,
            'symbols_path': self.symbols_path,
            'process_args': process_args,
            'logdir': self.workspace or os.getcwd(),
            'serial': self.serial,
        }
        if self.avd:
            runner_args['avd'] = self.avd

        return runner_args

    def close(self, restart=False):
        super(FennecInstance, self).close(restart)
        if self.runner and self.runner.device.connected:
            self.runner.device.dm.remove_forward(
                'tcp:%d' % int(self.marionette_port)
            )


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
        'browser.firstrun-content.dismissed': True,
        # Bug 1145668 - Has to be reverted to about:blank once Marionette
        # can correctly handle error pages
        'browser.newtab.url': 'about:newtab',
        'browser.newtabpage.enabled': False,
        'browser.reader.detectedFirstArticle': True,
        'browser.safebrowsing.blockedURIs.enabled': False,
        'browser.safebrowsing.forbiddenURIs.enabled': False,
        'browser.safebrowsing.malware.enabled': False,
        'browser.safebrowsing.phishing.enabled': False,
        'browser.search.update': False,
        'browser.tabs.animate': False,
        'browser.tabs.warnOnClose': False,
        'browser.tabs.warnOnOpen': False,
        'browser.uitour.enabled': False,
        'browser.usedOnWindows10.introURL': '',
        'extensions.getAddons.cache.enabled': False,
        'extensions.installDistroAddons': False,
        'extensions.showMismatchUI': False,
        'extensions.update.enabled': False,
        'extensions.update.notifyUser': False,
        'geo.provider.testing': True,
        'javascript.options.showInConsole': True,
        'privacy.trackingprotection.enabled': False,
        'privacy.trackingprotection.pbmode.enabled': False,
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
    'fennec': FennecInstance,
}
