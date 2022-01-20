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
import mozproxy.utils as mpu
import mozversion
import six

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
mpu.LOG = RaptorLogger(component="raptor-mitmproxy")

try:
    from mozbuild.base import MozbuildObject

    build = MozbuildObject.from_environment(cwd=here)
except ImportError:
    build = None

POST_DELAY_CONDPROF = 1000
POST_DELAY_DEBUG = 3000
POST_DELAY_DEFAULT = 30000


@six.add_metaclass(ABCMeta)
class Perftest(object):
    """Abstract base class for perftests that execute via a subharness,
    either Raptor or browsertime."""

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
        gecko_profile_extra_threads=None,
        gecko_profile_threads=None,
        gecko_profile_features=None,
        symbols_path=None,
        host=None,
        power_test=False,
        cpu_test=False,
        cold=False,
        memory_test=False,
        live_sites=False,
        is_release_build=False,
        debug_mode=False,
        post_startup_delay=POST_DELAY_DEFAULT,
        interrupt_handler=None,
        e10s=True,
        enable_webrender=False,
        results_handler_class=RaptorResultsHandler,
        device_name=None,
        disable_perf_tuning=False,
        conditioned_profile=None,
        chimera=False,
        extra_prefs={},
        environment={},
        project="mozilla-central",
        verbose=False,
        **kwargs
    ):
        self._dirs_to_remove = []
        self.verbose = verbose
        self.page_count = []

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
            "gecko_profile_extra_threads": gecko_profile_extra_threads,
            "gecko_profile_threads": gecko_profile_threads,
            "gecko_profile_features": gecko_profile_features,
            "symbols_path": symbols_path,
            "host": host,
            "power_test": power_test,
            "memory_test": memory_test,
            "cpu_test": cpu_test,
            "cold": cold,
            "live_sites": live_sites,
            "is_release_build": is_release_build,
            "enable_control_server_wait": memory_test or cpu_test,
            "e10s": e10s,
            "enable_webrender": enable_webrender,
            "device_name": device_name,
            "enable_fission": extra_prefs.get("fission.autostart", False),
            "disable_perf_tuning": disable_perf_tuning,
            "conditioned_profile": conditioned_profile,
            "chimera": chimera,
            "extra_prefs": extra_prefs,
            "environment": environment,
            "project": project,
            "verbose": verbose,
        }

        self.firefox_android_apps = FIREFOX_ANDROID_APPS

        # We are deactivating the conditioned profiles for:
        # - win10-aarch64 : no support for geckodriver see 1582757
        # - reference browser: no conditioned profiles created see 1606767
        if (
            self.config["platform"] == "win" and self.config["processor"] == "aarch64"
        ) or self.config["binary"] == "org.mozilla.reference.browser.raptor":
            self.config["conditioned_profile"] = None

        if self.config["conditioned_profile"]:
            LOG.info("Using a conditioned profile.")
        else:
            LOG.info("Using an empty profile.")

        # To differentiate between chrome/firefox failures, we
        # set an app variable in the logger which prefixes messages
        # with the app name
        if self.config["app"] in ("chrome", "chrome-m", "chromium"):
            LOG.set_app(self.config["app"])

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
        self.run_local = self.config["run_local"]
        self.debug_mode = debug_mode if self.run_local else False

        # For the post startup delay, we want to max it to 1s when using the
        # conditioned profiles.
        if self.config.get("conditioned_profile"):
            self.post_startup_delay = min(post_startup_delay, POST_DELAY_CONDPROF)
        else:
            # if running debug-mode reduce the pause after browser startup
            if self.debug_mode:
                self.post_startup_delay = min(post_startup_delay, POST_DELAY_DEBUG)
            else:
                self.post_startup_delay = post_startup_delay

        if self.config["enable_webrender"]:
            self.config["environment"]["MOZ_WEBRENDER"] = "1"
        else:
            self.config["environment"]["MOZ_WEBRENDER"] = "0"

        LOG.info("Post startup delay set to %d ms" % self.post_startup_delay)
        LOG.info("main raptor init, config is: %s" % str(self.config))
        self.build_browser_profile()

        # Crashes counter
        self.crashes = 0

    def _get_temp_dir(self):
        tempdir = tempfile.mkdtemp()
        self._dirs_to_remove.append(tempdir)
        return tempdir

    @property
    def is_localhost(self):
        return self.config.get("host") in ("localhost", "127.0.0.1")

    @property
    def conditioned_profile_copy(self):
        """Returns a copy of the original conditioned profile that was created."""
        condprof_copy = os.path.join(self._get_temp_dir(), "profile")
        shutil.copytree(
            self.conditioned_profile_dir,
            condprof_copy,
            ignore=shutil.ignore_patterns("lock"),
        )
        LOG.info("Created a conditioned-profile copy: %s" % condprof_copy)
        return condprof_copy

    def build_conditioned_profile(self):
        # Late import so python-test doesn't import it
        import asyncio
        from condprof.runner import Runner

        # The following import patchs an issue with invalid
        # content-type, see bug 1655869
        from condprof import patch  # noqa

        if not getattr(self, "browsertime"):
            raise Exception(
                "Building conditioned profiles within a test is only supported "
                "when using Browsertime."
            )

        geckodriver = getattr(self, "browsertime_geckodriver", None)
        if not geckodriver:
            geckodriver = (
                sys.platform.startswith("win") and "geckodriver.exe" or "geckodriver"
            )

        scenario = self.config.get("conditioned_profile")
        runner = Runner(
            profile=None,
            firefox=self.config.get("binary"),
            geckodriver=geckodriver,
            archive=None,
            device_name=self.config.get("device_name"),
            strict=True,
            visible=True,
            force_new=True,
            skip_logs=True,
        )

        if self.config.get("is_release_build", False):
            # Enable non-local connections for building the conditioned profile
            self.enable_non_local_connections()

        if scenario == "settled-youtube":
            runner.prepare(scenario, "youtube")

            loop = asyncio.get_event_loop()
            loop.run_until_complete(runner.one_run(scenario, "youtube"))
        else:
            runner.prepare(scenario, "default")

            loop = asyncio.get_event_loop()
            loop.run_until_complete(runner.one_run(scenario, "default"))

        if self.config.get("is_release_build", False):
            self.disable_non_local_connections()

        self.conditioned_profile_dir = runner.env.profile
        return self.conditioned_profile_copy

    def get_conditioned_profile(self, binary=None):
        """Downloads a platform-specific conditioned profile, using the
        condprofile client API; returns a self.conditioned_profile_dir"""
        if self.conditioned_profile_dir:
            # We already have a directory, so provide a copy that
            # will get deleted after it's done with
            return self.conditioned_profile_copy

        # Build the conditioned profile before the test
        if not self.config.get("conditioned_profile").startswith("artifact:"):
            return self.build_conditioned_profile()

        # create a temp file to help ensure uniqueness
        temp_download_dir = self._get_temp_dir()
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

        # when running under mozharness, the --project value
        # is set to match the project (try, mozilla-central, etc.)
        # By default it's mozilla-central, even on local runs.
        # We use it to prioritize conditioned profiles indexed
        # into the same project when it runs on the CI
        repo = self.config["project"]

        # we fall back to mozilla-central in all cases. If it
        # was already mozilla-central, we fall back to try
        alternate_repo = "mozilla-central" if repo != "mozilla-central" else "try"
        LOG.info("Getting profile from project %s" % repo)

        profile_scenario = self.config.get("conditioned_profile").replace(
            "artifact:", ""
        )
        try:
            cond_prof_target_dir = get_profile(
                temp_download_dir, platform, profile_scenario, repo=repo
            )
        except ProfileNotFoundError:
            cond_prof_target_dir = get_profile(
                temp_download_dir, platform, profile_scenario, repo=alternate_repo
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
                    cond_prof_target_dir, temp_download_dir, platform, profile_scenario
                )
            )
            raise OSError

        LOG.info(
            "Original self.conditioned_profile_dir is now set: {}".format(
                self.conditioned_profile_dir
            )
        )
        return self.conditioned_profile_copy

    def build_browser_profile(self):
        if (
            self.config["app"] in ["chrome", "chromium", "chrome-m"]
            or self.config.get("conditioned_profile") is None
        ):
            self.profile = create_profile(self.profile_class)
        else:
            # use mozprofile to create a profile for us, from our conditioned profile's path
            self.profile = create_profile(
                self.profile_class, profile=self.get_conditioned_profile()
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

    def process_results(self, tests, test_names):
        # when running locally output results in build/raptor.json; when running
        # in production output to a local.json to be turned into tc job artifact
        raptor_json_path = os.path.join(self.artifact_dir, "raptor.json")
        if not self.config.get("run_local", False):
            raptor_json_path = os.path.join(os.getcwd(), "local.json")

        self.config["raptor_json_path"] = raptor_json_path
        self.config["artifact_dir"] = self.artifact_dir
        self.config["page_count"] = self.page_count
        res = self.results_handler.summarize_and_output(self.config, tests, test_names)

        # gecko profiling symbolication
        if self.config["gecko_profile"]:
            self.gecko_profiler.symbolicate()
            # clean up the temp gecko profiling folders
            LOG.info("cleaning up after gecko profiling")
            self.gecko_profiler.clean()

        return res

    @abstractmethod
    def set_browser_test_prefs(self):
        pass

    @abstractmethod
    def check_for_crashes(self):
        pass

    def clean_up(self):
        for dir_to_rm in self._dirs_to_remove:
            if not os.path.exists(dir_to_rm):
                continue
            LOG.info("Removing temporary directory: {}".format(dir_to_rm))
            shutil.rmtree(dir_to_rm, ignore_errors=True)
        self._dirs_to_remove = []

    def get_page_timeout_list(self):
        return self.results_handler.page_timeout_list

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

        playback_dir = os.path.join(here, "tooltool-manifests", "playback")

        self.config.update(
            {
                "playback_tool": test.get("playback"),
                "playback_version": test.get("playback_version", "7.0.4"),
                "playback_files": [
                    os.path.join(playback_dir, test.get("playback_pageset_manifest"))
                ],
            }
        )

        LOG.info("test uses playback tool: %s " % self.config["playback_tool"])

        self.playback = get_playback(self.config)

        # let's start it!
        self.playback.start()

    def _init_gecko_profiling(self, test):
        LOG.info("initializing gecko profiler")
        upload_dir = os.getenv("MOZ_UPLOAD_DIR")
        if not upload_dir:
            LOG.critical("Profiling ignored because MOZ_UPLOAD_DIR was not set")
        else:
            self.gecko_profiler = GeckoProfile(upload_dir, self.config, test)

    def disable_non_local_connections(self):
        # For Firefox we need to set MOZ_DISABLE_NONLOCAL_CONNECTIONS=1 env var before startup
        # when testing release builds from mozilla-beta/release. This is because of restrictions
        # on release builds that require webextensions to be signed unless this env var is set
        LOG.info("setting MOZ_DISABLE_NONLOCAL_CONNECTIONS=1")
        os.environ["MOZ_DISABLE_NONLOCAL_CONNECTIONS"] = "1"

    def enable_non_local_connections(self):
        # pageload tests need to be able to access non-local connections via mitmproxy
        LOG.info("setting MOZ_DISABLE_NONLOCAL_CONNECTIONS=0")
        os.environ["MOZ_DISABLE_NONLOCAL_CONNECTIONS"] = "0"


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
            from mozdevice import ADBDeviceFactory

            device = ADBDeviceFactory(verbose=True)
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
                raise Exception(
                    "Could not determine version for Google Chrome for Android"
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
            self.device.push(self.profile.profile, self.remote_profile)
            self.device.chmod(self.remote_profile, recursive=True)

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

    def __init__(self, *args, **kwargs):
        super(PerftestDesktop, self).__init__(*args, **kwargs)
        if self.config["enable_webrender"]:
            self.config["environment"]["MOZ_ACCELERATED"] = "1"

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
                        match = meta_re.match(bmeta[0].decode("utf-8"))
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
                    match = meta_re.findall(bmeta.decode("utf-8"))
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
