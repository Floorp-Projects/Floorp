# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/

import os
import sys
import tempfile
import time

from copy import deepcopy

import mozversion

from mozprofile import Profile
from mozrunner import Runner, FennecEmulatorRunner


class GeckoInstance(object):
    required_prefs = {
        # Increase the APZ content response timeout in tests to 1 minute.
        # This is to accommodate the fact that test environments tends to be slower
        # than production environments (with the b2g emulator being the slowest of them
        # all), resulting in the production timeout value sometimes being exceeded
        # and causing false-positive test failures. See bug 1176798, bug 1177018,
        # bug 1210465.
        "apz.content_response_timeout": 60000,

        # Do not send Firefox health reports to the production server
        "datareporting.healthreport.documentServerURI": "http://%(server)s/dummy/healthreport/",
        "datareporting.healthreport.about.reportUrl": "http://%(server)s/dummy/abouthealthreport/",

        # Do not show datareporting policy notifications which can interfer with tests
        "datareporting.policy.dataSubmissionPolicyBypassNotification": True,

        "dom.ipc.reportProcessHangs": False,

        # No slow script dialogs
        "dom.max_chrome_script_run_time": 0,
        "dom.max_script_run_time": 0,

        # Only load extensions from the application and user profile
        # AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_APPLICATION
        "extensions.autoDisableScopes": 0,
        "extensions.enabledScopes": 5,
        # don't block add-ons for e10s
        "extensions.e10sBlocksEnabling": False,
        # Disable metadata caching for installed add-ons by default
        "extensions.getAddons.cache.enabled": False,
        # Disable intalling any distribution add-ons
        "extensions.installDistroAddons": False,
        "extensions.showMismatchUI": False,
        # Turn off extension updates so they don't bother tests
        "extensions.update.enabled": False,
        "extensions.update.notifyUser": False,
        # Make sure opening about:addons won"t hit the network
        "extensions.webservice.discoverURL": "http://%(server)s/dummy/discoveryURL",

        # Allow the application to have focus even it runs in the background
        "focusmanager.testmode": True,

        # Disable useragent updates
        "general.useragent.updates.enabled": False,

        # Always use network provider for geolocation tests
        # so we bypass the OSX dialog raised by the corelocation provider
        "geo.provider.testing": True,
        # Do not scan Wifi
        "geo.wifi.scan": False,

        # No hang monitor
        "hangmonitor.timeout": 0,

        "javascript.options.showInConsole": True,

        # Enable Marionette component
        "marionette.enabled": True,
        # Deprecated, and can be removed in Firefox 60.0
        "marionette.defaultPrefs.enabled": True,
        # Disable recommended automation prefs in CI
        "marionette.prefs.recommended": False,

        "media.volume_scale": "0.01",

        # Make sure the disk cache doesn't get auto disabled
        "network.http.bypass-cachelock-threshold": 200000,
        # Do not prompt for temporary redirects
        "network.http.prompt-temp-redirect": False,
        # Disable speculative connections so they aren"t reported as leaking when they"re
        # hanging around
        "network.http.speculative-parallel-limit": 0,
        # Do not automatically switch between offline and online
        "network.manage-offline-status": False,
        # Make sure SNTP requests don't hit the network
        "network.sntp.pools": "%(server)s",

        # Tests don't wait for the notification button security delay
        "security.notification_enable_delay": 0,

        # Ensure blocklist updates don't hit the network
        "services.settings.server": "http://%(server)s/dummy/blocklist/",

        # Disable password capture, so that tests that include forms aren"t
        # influenced by the presence of the persistent doorhanger notification
        "signon.rememberSignons": False,

        # Prevent starting into safe mode after application crashes
        "toolkit.startup.max_resumed_crashes": -1,

        # We want to collect telemetry, but we don't want to send in the results
        "toolkit.telemetry.server": "https://%(server)s/dummy/telemetry/",

        # Enabling the support for File object creation in the content process.
        "dom.file.createInChild": True,
    }

    def __init__(self, host=None, port=None, bin=None, profile=None, addons=None,
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
        if path != "-":
            if path is None:
                path = "gecko.log"
            elif os.path.isdir(path):
                fname = "gecko-{}.log".format(time.time())
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
        if "-jsdebugger" in self.app_args:
            profile_args["preferences"].update({
                "devtools.browsertoolbox.panel": "jsdebugger",
                "devtools.debugger.remote-enabled": True,
                "devtools.chrome.enabled": True,
                "devtools.debugger.prompt-connection": False,
                "marionette.debugging.clicktostart": True,
            })
        if self.addons:
            profile_args["addons"] = self.addons

        if hasattr(self, "profile_path") and self.profile is None:
            if not self.profile_path:
                if self.workspace:
                    profile_args["profile"] = tempfile.mkdtemp(
                        suffix=".mozrunner-{:.0f}".format(time.time()),
                        dir=self.workspace)
                self.profile = Profile(**profile_args)
            else:
                profile_args["path_from"] = self.profile_path
                profile_name = "{}-{:.0f}".format(
                    os.path.basename(self.profile_path),
                    time.time()
                )
                if self.workspace:
                    profile_args["path_to"] = os.path.join(self.workspace,
                                                           profile_name)
                self.profile = Profile.clone(**profile_args)

    @classmethod
    def create(cls, app=None, *args, **kwargs):
        try:
            if not app and kwargs["bin"] is not None:
                app_id = mozversion.get_version(binary=kwargs["bin"])["application_id"]
                app = app_ids[app_id]

            instance_class = apps[app]
        except (IOError, KeyError):
            exc, val, tb = sys.exc_info()
            msg = 'Application "{0}" unknown (should be one of {1})'
            raise NotImplementedError, msg.format(app, apps.keys()), tb

        return instance_class(*args, **kwargs)

    def start(self):
        self._update_profile()
        self.runner = self.runner_class(**self._get_runner_args())
        self.runner.start()

    def _get_runner_args(self):
        process_args = {
            "processOutputLine": [NullOutput()],
        }

        if self.gecko_log == "-":
            process_args["stream"] = sys.stdout
        else:
            process_args["logfile"] = self.gecko_log

        env = os.environ.copy()

        # environment variables needed for crashreporting
        # https://developer.mozilla.org/docs/Environment_variables_affecting_crash_reporting
        env.update({"MOZ_CRASHREPORTER": "1",
                    "MOZ_CRASHREPORTER_NO_REPORT": "1",
                    "MOZ_CRASHREPORTER_SHUTDOWN": "1",
                    })

        return {
            "binary": self.binary,
            "profile": self.profile,
            "cmdargs": ["-no-remote", "-marionette"] + self.app_args,
            "env": env,
            "symbols_path": self.symbols_path,
            "process_args": process_args
        }

    def close(self, restart=False, clean=False):
        """
        Close the managed Gecko process.

        Depending on self.runner_class, setting `clean` to True may also kill
        the emulator process in which this instance is running.

        :param restart: If True, assume this is being called by restart method.
        :param clean: If True, also perform runner cleanup.
        """
        if not restart:
            self.profile = None

        if self.runner:
            self.runner.stop()
            if clean:
                self.runner.cleanup()

    def restart(self, prefs=None, clean=True):
        """
        Close then start the managed Gecko process.

        :param prefs: Dictionary of preference names and values.
        :param clean: If True, reset the profile before starting.
        """
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
    fennec_prefs = {
        # Enable output of dump()
        "browser.dom.window.dump.enabled": True,

        # Disable Android snippets
        "browser.snippets.enabled": False,
        "browser.snippets.syncPromo.enabled": False,
        "browser.snippets.firstrunHomepage.enabled": False,

        # Disable safebrowsing components
        "browser.safebrowsing.downloads.enabled": False,

        # Do not restore the last open set of tabs if the browser has crashed
        "browser.sessionstore.resume_from_crash": False,

        # Disable e10s by default
        "browser.tabs.remote.autostart.1": False,
        "browser.tabs.remote.autostart.2": False,
        "browser.tabs.remote.autostart": False,

        # Do not allow background tabs to be zombified, otherwise for tests that
        # open additional tabs, the test harness tab itself might get unloaded
        "browser.tabs.disableBackgroundZombification": True,
    }

    def __init__(self, emulator_binary=None, avd_home=None, avd=None,
                 adb_path=None, serial=None, connect_to_running_emulator=False,
                 package_name=None, *args, **kwargs):
        super(FennecInstance, self).__init__(*args, **kwargs)
        self.required_prefs.update(FennecInstance.fennec_prefs)

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
            self._package_name = "org.mozilla.fennec"
            user = os.getenv("USER")
            if user:
                self._package_name += "_" + user
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
            message = "Error possibly due to runner or device args: {}"
            raise exc, message.format(e.message), tb
        # gecko_log comes from logcat when running with device/emulator
        logcat_args = {
            "filterspec": "Gecko",
            "serial": self.runner.device.dm._deviceSerial
        }
        if self.gecko_log == "-":
            logcat_args["stream"] = sys.stdout
        else:
            logcat_args["logfile"] = self.gecko_log
        self.runner.device.start_logcat(**logcat_args)

        # forward marionette port (localhost:2828)
        self.runner.device.dm.forward(
            local="tcp:{}".format(self.marionette_port),
            remote="tcp:{}".format(self.marionette_port))

    def _get_runner_args(self):
        process_args = {
            "processOutputLine": [NullOutput()],
        }

        runner_args = {
            "app": self.package_name,
            "avd_home": self.avd_home,
            "adb_path": self.adb_path,
            "binary": self.emulator_binary,
            "profile": self.profile,
            "cmdargs": ["-marionette"] + self.app_args,
            "symbols_path": self.symbols_path,
            "process_args": process_args,
            "logdir": self.workspace or os.getcwd(),
            "serial": self.serial,
        }
        if self.avd:
            runner_args["avd"] = self.avd

        return runner_args

    def close(self, restart=False, clean=False):
        """
        Close the managed Gecko process.

        If `clean` is True and the Fennec instance is running in an
        emulator managed by mozrunner, this will stop the emulator.

        :param restart: If True, assume this is being called by restart method.
        :param clean: If True, also perform runner cleanup.
        """
        super(FennecInstance, self).close(restart, clean)
        if clean and self.runner and self.runner.device.connected:
            self.runner.device.dm.remove_forward(
                "tcp:{}".format(self.marionette_port))


class DesktopInstance(GeckoInstance):
    desktop_prefs = {
        # Disable application updates
        "app.update.enabled": False,

        # Enable output of dump()
        "browser.dom.window.dump.enabled": True,

        # Indicate that the download panel has been shown once so that whichever
        # download test runs first doesn"t show the popup inconsistently
        "browser.download.panel.shown": True,

        # Do not show the EULA notification which can interfer with tests
        "browser.EULA.override": True,

        # Turn off about:newtab and make use of about:blank instead for new opened tabs
        "browser.newtabpage.enabled": False,
        # Assume the about:newtab page"s intro panels have been shown to not depend on
        # which test runs first and happens to open about:newtab
        "browser.newtabpage.introShown": True,

        # Background thumbnails in particular cause grief, and disabling thumbnails
        # in general can"t hurt - we re-enable them when tests need them
        "browser.pagethumbnails.capturing_disabled": True,

        # Disable safebrowsing components
        "browser.safebrowsing.blockedURIs.enabled": False,
        "browser.safebrowsing.downloads.enabled": False,
        "browser.safebrowsing.forbiddenURIs.enabled": False,
        "browser.safebrowsing.malware.enabled": False,
        "browser.safebrowsing.phishing.enabled": False,

        # Disable updates to search engines
        "browser.search.update": False,

        # Do not restore the last open set of tabs if the browser has crashed
        "browser.sessionstore.resume_from_crash": False,

        # Don't check for the default web browser during startup
        "browser.shell.checkDefaultBrowser": False,

        # Disable e10s by default
        "browser.tabs.remote.autostart.1": False,
        "browser.tabs.remote.autostart.2": False,
        "browser.tabs.remote.autostart": False,

        # Needed for branded builds to prevent opening a second tab on startup
        "browser.startup.homepage_override.mstone": "ignore",
        # Start with a blank page by default
        "browser.startup.page": 0,

        # Disable browser animations
        "toolkit.cosmeticAnimations.enabled": False,

        # Do not warn on exit when multiple tabs are open
        "browser.tabs.warnOnClose": False,
        # Do not warn when closing all other open tabs
        "browser.tabs.warnOnCloseOtherTabs": False,
        # Do not warn when multiple tabs will be opened
        "browser.tabs.warnOnOpen": False,

        # Disable the UI tour
        "browser.uitour.enabled": False,

        # Disable first-run welcome page
        "startup.homepage_welcome_url": "about:blank",
        "startup.homepage_welcome_url.additional": "",
    }

    def __init__(self, *args, **kwargs):
        super(DesktopInstance, self).__init__(*args, **kwargs)
        self.required_prefs.update(DesktopInstance.desktop_prefs)


class NullOutput(object):
    def __call__(self, line):
        pass


apps = {
    'fennec': FennecInstance,
    'fxdesktop': DesktopInstance,
}

app_ids = {
    '{aa3c5121-dab2-40e2-81ca-7ea25febc110}': 'fennec',
    '{ec8030f7-c20a-464f-9b0e-13a3a9e97384}': 'fxdesktop',
}
