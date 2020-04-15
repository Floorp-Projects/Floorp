#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import traceback
from abc import ABCMeta, abstractmethod

import mozinfo
import mozprocess
import mozversion
import mozproxy.utils as mpu
from mozprofile import create_profile
from mozproxy import get_playback


# need this so raptor imports work both from /raptor and via mach
here = os.path.abspath(os.path.dirname(__file__))
paths = [here]

webext_dir = os.path.join(here, "..", "webext")
paths.append(webext_dir)

for path in paths:
    if not os.path.exists(path):
        raise IOError("%s does not exist. " % path)
    sys.path.insert(0, path)


from cmdline import FIREFOX_ANDROID_APPS
from condprof.client import get_profile, ProfileNotFoundError
from condprof.util import get_current_platform
from logger.logger import RaptorLogger
from gecko_profile import GeckoProfile
from results import RaptorResultsHandler

LOG = RaptorLogger(component="raptor-perftest")

# - mozproxy.utils LOG displayed INFO messages even when LOG.error() was used in mitm.py
mpu.LOG = RaptorLogger(component='raptor-mitmproxy')

try:
    from mozbuild.base import MozbuildObject

    build = MozbuildObject.from_environment(cwd=here)
except ImportError:
    build = None

POST_DELAY_CONDPROF = 1000
POST_DELAY_DEBUG = 3000
POST_DELAY_DEFAULT = 30000


class Perftest(object):
    """Abstract base class for perftests that execute via a subharness,
either Raptor or browsertime."""

    __metaclass__ = ABCMeta

    def __init__(
        self,
        app,
        binary,
        run_local=False,
        noinstall=False,
        obj_path=None,
        profile_class=None,
        installerpath=None,
        gecko_profile=False,
        gecko_profile_interval=None,
        gecko_profile_entries=None,
        symbols_path=None,
        host=None,
        power_test=False,
        cpu_test=False,
        memory_test=False,
        is_release_build=False,
        debug_mode=False,
        post_startup_delay=POST_DELAY_DEFAULT,
        interrupt_handler=None,
        e10s=True,
        enable_webrender=False,
        results_handler_class=RaptorResultsHandler,
        no_conditioned_profile=False,
        device_name=None,
        disable_perf_tuning=False,
        conditioned_profile_scenario='settled',
        extra_prefs={},
        **kwargs
    ):

        # Override the magic --host HOST_IP with the value of the environment variable.
        if host == "HOST_IP":
            host = os.environ["HOST_IP"]

        self.config = {
            "app": app,
            "binary": binary,
            "platform": mozinfo.os,
            "processor": mozinfo.processor,
            "run_local": run_local,
            "obj_path": obj_path,
            "gecko_profile": gecko_profile,
            "gecko_profile_interval": gecko_profile_interval,
            "gecko_profile_entries": gecko_profile_entries,
            "symbols_path": symbols_path,
            "host": host,
            "power_test": power_test,
            "memory_test": memory_test,
            "cpu_test": cpu_test,
            "is_release_build": is_release_build,
            "enable_control_server_wait": memory_test or cpu_test,
            "e10s": e10s,
            "enable_webrender": enable_webrender,
            "no_conditioned_profile": no_conditioned_profile,
            "device_name": device_name,
            "enable_fission": extra_prefs.get("fission.autostart", False),
            "disable_perf_tuning": disable_perf_tuning,
            "conditioned_profile_scenario": conditioned_profile_scenario,
            "extra_prefs": extra_prefs,
        }

        self.firefox_android_apps = FIREFOX_ANDROID_APPS
        # See bugs 1582757, 1606199, and 1606767; until we support win10-aarch64,
        # fennec_aurora, and reference browser conditioned-profile builds,
        # fall back to mozrunner-created profiles
        self.no_condprof = (
            (self.config["platform"] == "win" and self.config["processor"] == "aarch64")
            or self.config["binary"] == "org.mozilla.fennec_aurora"
            or self.config["binary"] == "org.mozilla.reference.browser.raptor"
            or self.config["no_conditioned_profile"]
        )
        LOG.info("self.no_condprof is: {}".format(self.no_condprof))

        # We can never use e10s on fennec
        if self.config["app"] == "fennec":
            self.config["e10s"] = False

        self.browser_name = None
        self.browser_version = None

        self.raptor_venv = os.path.join(os.getcwd(), "raptor-venv")
        self.installerpath = installerpath
        self.playback = None
        self.benchmark = None
        self.gecko_profiler = None
        self.device = None
        self.runtime_error = None
        self.profile_class = profile_class or app
        self.conditioned_profile_dir = None
        self.interrupt_handler = interrupt_handler
        self.results_handler = results_handler_class(**self.config)

        self.browser_name, self.browser_version = self.get_browser_meta()

        browser_name, browser_version = self.get_browser_meta()
        self.results_handler.add_browser_meta(self.config["app"], browser_version)

        # debug mode is currently only supported when running locally
        self.run_local = self.config['run_local']
        self.debug_mode = debug_mode if self.run_local else False

        # For the post startup delay, we want to max it to 1s when using the
        # conditioned profiles.
        if not self.no_condprof and not self.run_local:
            self.post_startup_delay = min(post_startup_delay, POST_DELAY_CONDPROF)
        else:
            # if running debug-mode reduce the pause after browser startup
            if self.debug_mode:
                self.post_startup_delay = min(post_startup_delay,
                                              POST_DELAY_DEBUG)
            else:
                self.post_startup_delay = post_startup_delay

        LOG.info("Post startup delay set to %d ms" % self.post_startup_delay)
        LOG.info("main raptor init, config is: %s" % str(self.config))
        self.build_browser_profile()

        # Crashes counter
        self.crashes = 0

    @property
    def is_localhost(self):
        return self.config.get("host") in ("localhost", "127.0.0.1")

    def get_conditioned_profile(self):
        """Downloads a platform-specific conditioned profile, using the
        condprofile client API; returns a self.conditioned_profile_dir"""

        # create a temp file to help ensure uniqueness
        temp_download_dir = tempfile.mkdtemp()
        LOG.info(
            "Making temp_download_dir from inside get_conditioned_profile {}".format(
                temp_download_dir
            )
        )
        # call condprof's client API to yield our platform-specific
        # conditioned-profile binary
        if isinstance(self, PerftestAndroid):
            android_app = self.config["binary"].split("org.mozilla.")[-1]
            device_name = self.config.get("device_name")
            if device_name is None:
                device_name = "g5"
            platform = "%s-%s" % (device_name, android_app)
        else:
            platform = get_current_platform()

        LOG.info("Platform used: %s" % platform)
        profile_scenario = self.config.get("conditioned_profile_scenario", "settled")
        try:
            cond_prof_target_dir = get_profile(
                temp_download_dir,
                platform,
                profile_scenario
            )
        except ProfileNotFoundError:
            # If we can't find the profile on mozilla-central, we look on try
            cond_prof_target_dir = get_profile(
                temp_download_dir,
                platform,
                profile_scenario,
                repo="try"
            )
        except Exception:
            # any other error is a showstopper
            LOG.critical("Could not get the conditioned profile")
            traceback.print_exc()
            raise

        # now get the full directory path to our fetched conditioned profile
        self.conditioned_profile_dir = os.path.join(
            temp_download_dir, cond_prof_target_dir
        )
        if not os.path.exists(cond_prof_target_dir):
            LOG.critical(
                "Can't find target_dir {}, from get_profile()"
                "temp_download_dir {}, platform {}, scenario {}".format(
                    cond_prof_target_dir,
                    temp_download_dir,
                    platform,
                    profile_scenario
                )
            )
            raise OSError

        LOG.info(
            "self.conditioned_profile_dir is now set: {}".format(
                self.conditioned_profile_dir
            )
        )
        shutil.rmtree(temp_download_dir)

        return self.conditioned_profile_dir

    def build_browser_profile(self):
        if self.no_condprof or self.config['app'] in ['chrome', 'chromium', 'chrome-m']:
            self.profile = create_profile(self.profile_class)
        else:
            self.get_conditioned_profile()
            # use mozprofile to create a profile for us, from our conditioned profile's path
            self.profile = create_profile(
                self.profile_class, profile=self.conditioned_profile_dir
            )
        # Merge extra profile data from testing/profiles
        with open(os.path.join(self.profile_data_dir, "profiles.json"), "r") as fh:
            base_profiles = json.load(fh)["raptor"]

        for profile in base_profiles:
            path = os.path.join(self.profile_data_dir, profile)
            LOG.info("Merging profile: {}".format(path))
            self.profile.merge(path)

        if self.config["extra_prefs"].get("fission.autostart", False):
            LOG.info("Enabling fission via browser preferences")
            LOG.info("Browser preferences: {}".format(self.config["extra_prefs"]))
        self.profile.set_preferences(self.config["extra_prefs"])

        # share the profile dir with the config and the control server
        self.config["local_profile_dir"] = self.profile.profile
        LOG.info("Local browser profile: {}".format(self.profile.profile))

    @property
    def profile_data_dir(self):
        if "MOZ_DEVELOPER_REPO_DIR" in os.environ:
            return os.path.join(
                os.environ["MOZ_DEVELOPER_REPO_DIR"], "testing", "profiles"
            )
        if build:
            return os.path.join(build.topsrcdir, "testing", "profiles")
        return os.path.join(here, "profile_data")

    @property
    def artifact_dir(self):
        artifact_dir = os.getcwd()
        if self.config.get("run_local", False):
            if "MOZ_DEVELOPER_REPO_DIR" in os.environ:
                artifact_dir = os.path.join(
                    os.environ["MOZ_DEVELOPER_REPO_DIR"],
                    "testing",
                    "mozharness",
                    "build",
                )
            else:
                artifact_dir = here
        elif os.getenv("MOZ_UPLOAD_DIR"):
            artifact_dir = os.getenv("MOZ_UPLOAD_DIR")
        return artifact_dir

    @abstractmethod
    def run_test_setup(self, test):
        LOG.info("starting test: %s" % test["name"])

        # if 'alert_on' was provided in the test INI, add to our config for results/output
        self.config["subtest_alert_on"] = test.get("alert_on")

        if test.get("playback") is not None and self.playback is None:
            self.start_playback(test)

        if test.get("preferences") is not None:
            self.set_browser_test_prefs(test["preferences"])

    @abstractmethod
    def setup_chrome_args(self):
        pass

    @abstractmethod
    def get_browser_meta(self):
        pass

    def run_tests(self, tests, test_names):
        try:
            for test in tests:
                try:
                    self.run_test(test, timeout=int(test.get("page_timeout")))
                except RuntimeError as e:
                    # Check for crashes before showing the timeout error.
                    self.check_for_crashes()
                    if self.crashes == 0:
                        LOG.critical(e)
                    os.sys.exit(1)
                finally:
                    self.run_test_teardown(test)
            return self.process_results(tests, test_names)
        finally:
            self.clean_up()

    @abstractmethod
    def run_test(self, test, timeout):
        raise NotImplementedError()

    @abstractmethod
    def run_test_teardown(self, test):
        self.check_for_crashes()

        # gecko profiling symbolication
        if self.config["gecko_profile"]:
            self.gecko_profiler.symbolicate()
            # clean up the temp gecko profiling folders
            LOG.info("cleaning up after gecko profiling")
            self.gecko_profiler.clean()

    def process_results(self, tests, test_names):
        # when running locally output results in build/raptor.json; when running
        # in production output to a local.json to be turned into tc job artifact
        raptor_json_path = os.path.join(self.artifact_dir, "raptor.json")
        if not self.config.get("run_local", False):
            raptor_json_path = os.path.join(os.getcwd(), "local.json")

        self.config["raptor_json_path"] = raptor_json_path
        self.config["artifact_dir"] = self.artifact_dir
        return self.results_handler.summarize_and_output(self.config, tests, test_names)

    @abstractmethod
    def set_browser_test_prefs(self):
        pass

    @abstractmethod
    def check_for_crashes(self):
        pass

    @abstractmethod
    def clean_up(self):
        pass

    def get_page_timeout_list(self):
        return self.results_handler.page_timeout_list

    def get_recording_paths(self, test):
        recordings = test.get("playback_recordings")

        if recordings:
            recording_paths = []
            proxy_dir = self.playback.mozproxy_dir

            for recording in recordings.split():
                if not recording:
                    continue
                recording_paths.append(os.path.join(proxy_dir, recording))

            return recording_paths

    def log_recording_dates(self, test):
        _recording_paths = self.get_recording_paths(test)
        if _recording_paths is None:
            LOG.info(
                "No playback recordings specified in the test; so not getting recording info"
            )
            return

        for r in _recording_paths:
            json_path = "{}.json".format(r.split(".")[0])

            if os.path.exists(json_path):
                with open(json_path) as f:
                    recording_date = json.loads(f.read()).get("recording_date")

                    if recording_date is not None:
                        LOG.info(
                            "Playback recording date: {} ".format(
                                recording_date.split(" ")[0]
                            )
                        )
                    else:
                        LOG.info("Playback recording date not available")
            else:
                LOG.info("Playback recording information not available")

    def get_playback_config(self, test):
        platform = self.config["platform"]
        playback_dir = os.path.join(here, "playback")

        self.config.update(
            {
                "playback_tool": test.get("playback"),
                "playback_version": test.get("playback_version", "4.0.4"),
                "playback_binary_zip": test.get("playback_binary_zip_%s" % platform),
                "playback_pageset_zip": test.get("playback_pageset_zip_%s" % platform),
                "playback_binary_manifest": test.get("playback_binary_manifest"),
                "playback_pageset_manifest": test.get("playback_pageset_manifest"),
            }
        )

        for key in ("playback_pageset_manifest", "playback_pageset_zip"):
            if self.config.get(key) is None:
                continue
            self.config[key] = os.path.join(playback_dir, self.config[key])

        LOG.info("test uses playback tool: %s " % self.config["playback_tool"])

    def delete_proxy_settings_from_profile(self):
        # Must delete the proxy settings from the profile if running
        # the test with a host different from localhost.
        userjspath = os.path.join(self.profile.profile, "user.js")
        with open(userjspath) as userjsfile:
            prefs = userjsfile.readlines()
        prefs = [pref for pref in prefs if "network.proxy" not in pref]
        with open(userjspath, "w") as userjsfile:
            userjsfile.writelines(prefs)

    def start_playback(self, test):
        # creating the playback tool
        self.get_playback_config(test)
        self.playback = get_playback(self.config)

        self.playback.config["playback_files"] = self.get_recording_paths(test)

        # let's start it!
        self.playback.start()

        self.log_recording_dates(test)

    def _init_gecko_profiling(self, test):
        LOG.info("initializing gecko profiler")
        upload_dir = os.getenv("MOZ_UPLOAD_DIR")
        if not upload_dir:
            LOG.critical("Profiling ignored because MOZ_UPLOAD_DIR was not set")
        else:
            self.gecko_profiler = GeckoProfile(upload_dir, self.config, test)


class PerftestAndroid(Perftest):
    """Mixin class for Android-specific Perftest subclasses."""

    def setup_chrome_args(self, test):
        """Sets up chrome/chromium cmd-line arguments.

        Needs to be "implemented" here to deal with Python 2
        unittest failures.
        """
        raise NotImplementedError

    def get_browser_meta(self):
        """Returns the browser name and version in a tuple (name, version).

        Uses mozversion as the primary method to get this meta data and for
        android this is the only method which exists to get this data. With android,
        we use the installerpath attribute to determine this and this only works
        with Firefox browsers.
        """
        browser_name = None
        browser_version = None

        if self.config["app"] in self.firefox_android_apps:
            try:
                meta = mozversion.get_version(binary=self.installerpath)
                browser_name = meta.get("application_name")
                browser_version = meta.get("application_version")
            except Exception as e:
                LOG.warning(
                    "Failed to get android browser meta data through mozversion: %s-%s"
                    % (e.__class__.__name__, e)
                )

        if self.config["app"] == "chrome-m":
            # We absolutely need to determine the chrome
            # version here so that we can select the correct
            # chromedriver for browsertime
            from mozdevice import ADBDevice
            device = ADBDevice(verbose=True)
            binary = "com.android.chrome"

            pkg_info = device.shell_output("dumpsys package %s" % binary)
            version_matcher = re.compile(r".*versionName=([\d.]+)")
            for line in pkg_info.split("\n"):
                match = version_matcher.match(line)
                if match:
                    browser_version = match.group(1)
                    browser_name = self.config["app"]
                    # First one found is the non-system
                    # or latest version.
                    break

            if not browser_version:
                raise Exception("Could not determine version for Google Chrome for Android")

        if not browser_name:
            LOG.warning("Could not find a browser name")
        else:
            LOG.info("Browser name: %s" % browser_name)

        if not browser_version:
            LOG.warning("Could not find a browser version")
        else:
            LOG.info("Browser version: %s" % browser_version)

        return (browser_name, browser_version)

    def set_reverse_port(self, port):
        tcp_port = "tcp:{}".format(port)
        self.device.create_socket_connection("reverse", tcp_port, tcp_port)

    def set_reverse_ports(self):
        if self.is_localhost:

            # only raptor-webext uses the control server
            if self.config.get("browsertime", False) is False:
                LOG.info("making the raptor control server port available to device")
                self.set_reverse_port(self.control_server.port)

            if self.playback:
                LOG.info("making the raptor playback server port available to device")
                self.set_reverse_port(self.playback.port)

            if self.benchmark:
                LOG.info("making the raptor benchmarks server port available to device")
                self.set_reverse_port(int(self.benchmark.port))
        else:
            LOG.info("Reverse port forwarding is used only on local devices")

    def build_browser_profile(self):
        super(PerftestAndroid, self).build_browser_profile()

        # Merge in the Android profile.
        path = os.path.join(self.profile_data_dir, "raptor-android")
        LOG.info("Merging profile: {}".format(path))
        self.profile.merge(path)
        self.profile.set_preferences(
            {"browser.tabs.remote.autostart": self.config["e10s"]}
        )

    def clear_app_data(self):
        LOG.info("clearing %s app data" % self.config["binary"])
        self.device.shell("pm clear %s" % self.config["binary"])

    def set_debug_app_flag(self):
        # required so release apks will read the android config.yml file
        LOG.info("setting debug-app flag for %s" % self.config["binary"])
        self.device.shell("am set-debug-app --persistent %s" % self.config["binary"])

    def copy_profile_to_device(self):
        """Copy the profile to the device, and update permissions of all files."""
        if not self.device.is_app_installed(self.config["binary"]):
            raise Exception("%s is not installed" % self.config["binary"])

        try:
            LOG.info("copying profile to device: %s" % self.remote_profile)
            self.device.rm(self.remote_profile, force=True, recursive=True)
            # self.device.mkdir(self.remote_profile)
            self.device.push(self.profile.profile, self.remote_profile)
            self.device.chmod(self.remote_profile, recursive=True, root=True)

        except Exception:
            LOG.error("Unable to copy profile to device.")
            raise

    def turn_on_android_app_proxy(self):
        # for geckoview/android pageload playback we can't use a policy to turn on the
        # proxy; we need to set prefs instead; note that the 'host' may be different
        # than '127.0.0.1' so we must set the prefs accordingly
        proxy_prefs = {}
        proxy_prefs["network.proxy.type"] = 1
        proxy_prefs["network.proxy.http"] = self.playback.host
        proxy_prefs["network.proxy.http_port"] = self.playback.port
        proxy_prefs["network.proxy.ssl"] = self.playback.host
        proxy_prefs["network.proxy.ssl_port"] = self.playback.port
        proxy_prefs["network.proxy.no_proxies_on"] = self.config["host"]

        LOG.info(
            "setting profile prefs to turn on the android app proxy: {}".format(
                proxy_prefs
            )
        )
        self.profile.set_preferences(proxy_prefs)


class PerftestDesktop(Perftest):
    """Mixin class for Desktop-specific Perftest subclasses"""

    def setup_chrome_args(self, test):
        """Sets up chrome/chromium cmd-line arguments.

        Needs to be "implemented" here to deal with Python 2
        unittest failures.
        """
        raise NotImplementedError

    def desktop_chrome_args(self, test):
        """Returns cmd line options required to run pageload tests on Desktop Chrome
        and Chromium. Also add the cmd line options to turn on the proxy and
        ignore security certificate errors if using host localhost, 127.0.0.1.
        """
        chrome_args = ["--use-mock-keychain", "--no-default-browser-check"]

        if test.get("playback", False):
            pb_args = [
                "--proxy-server=%s:%d" % (self.playback.host, self.playback.port),
                "--proxy-bypass-list=localhost;127.0.0.1",
                "--ignore-certificate-errors",
            ]

            if not self.is_localhost:
                pb_args[0] = pb_args[0].replace("127.0.0.1", self.config["host"])

            chrome_args.extend(pb_args)

        if self.debug_mode:
            chrome_args.extend(["--auto-open-devtools-for-tabs"])

        return chrome_args

    def get_browser_meta(self):
        """Returns the browser name and version in a tuple (name, version).

        On desktop, we use mozversion but a fallback method also exists
        for non-firefox browsers, where mozversion is known to fail. The
        methods are OS-specific, with windows being the outlier.
        """
        browser_name = None
        browser_version = None

        try:
            meta = mozversion.get_version(binary=self.config["binary"])
            browser_name = meta.get("application_name")
            browser_version = meta.get("application_version")
        except Exception as e:
            LOG.warning(
                "Failed to get browser meta data through mozversion: %s-%s"
                % (e.__class__.__name__, e)
            )
            LOG.info("Attempting to get version through fallback method...")

            # Fall-back method to get browser version on desktop
            try:
                if (
                    "linux" in self.config["platform"]
                    or "mac" in self.config["platform"]
                ):
                    command = [self.config["binary"], "--version"]
                    proc = mozprocess.ProcessHandler(command)
                    proc.run(timeout=10, outputTimeout=10)
                    proc.wait()

                    bmeta = proc.output
                    meta_re = re.compile(r"([A-z\s]+)\s+([\w.]*)")
                    if len(bmeta) != 0:
                        match = meta_re.match(bmeta[0])
                        if match:
                            browser_name = self.config["app"]
                            browser_version = match.group(2)
                    else:
                        LOG.info("Couldn't get browser version and name")
                else:
                    # On windows we need to use wimc to get the version
                    command = r'wmic datafile where name="{0}"'.format(
                        self.config["binary"].replace("\\", r"\\")
                    )
                    bmeta = subprocess.check_output(command)

                    meta_re = re.compile(r"\s+([\d.a-z]+)\s+")
                    match = meta_re.findall(bmeta)
                    if len(match) > 0:
                        browser_name = self.config["app"]
                        browser_version = match[-1]
                    else:
                        LOG.info("Couldn't get browser version and name")
            except Exception as e:
                LOG.warning(
                    "Failed to get browser meta data through fallback method: %s-%s"
                    % (e.__class__.__name__, e)
                )

        if not browser_name:
            LOG.warning("Could not find a browser name")
        else:
            LOG.info("Browser name: %s" % browser_name)

        if not browser_version:
            LOG.warning("Could not find a browser version")
        else:
            LOG.info("Browser version: %s" % browser_version)

        return (browser_name, browser_version)
