#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from abc import ABCMeta, abstractmethod
import json
import os
import posixpath
import re
import shutil
import signal
import six
import subprocess
import sys
import tempfile
import time
import tarfile

import requests

import mozcrash
import mozinfo
import mozprocess
import mozproxy.utils as mpu
import mozversion
from condprof.client import get_profile, ProfileNotFoundError
from condprof.util import get_current_platform
from logger.logger import RaptorLogger
from mozdevice import ADBDevice
from mozlog import commandline
from mozpower import MozPower
from mozprofile import create_profile
from mozprofile.cli import parse_preferences
from mozproxy import get_playback
from mozrunner import runners

# need this so raptor imports work both from /raptor and via mach
here = os.path.abspath(os.path.dirname(__file__))
paths = [here]

webext_dir = os.path.join(here, '..', 'webext')
paths.append(webext_dir)

for path in paths:
    if not os.path.exists(path):
        raise IOError("%s does not exist. " % path)
    sys.path.insert(0, path)

try:
    from mozbuild.base import MozbuildObject

    build = MozbuildObject.from_environment(cwd=here)
except ImportError:
    build = None

from benchmark import Benchmark
from cmdline import (parse_args,
                     FIREFOX_ANDROID_APPS,
                     CHROMIUM_DISTROS)
from control_server import RaptorControlServer
from gecko_profile import GeckoProfile
from gen_test_config import gen_test_config
from outputhandler import OutputHandler
from manifest import get_raptor_test_list
from memory import generate_android_memory_profile
from performance_tuning import tune_performance
from power import init_android_power_test, finish_android_power_test
from results import RaptorResultsHandler, BrowsertimeResultsHandler
from utils import view_gecko_profile, write_yml_file
from cpu import start_android_cpu_profiler

LOG = RaptorLogger(component='raptor-main')
# Bug 1547943 - Intermittent mozrunner.errors.RunnerNotStartedError
# - mozproxy.utils LOG displayed INFO messages even when LOG.error() was used in mitm.py
mpu.LOG = RaptorLogger(component='raptor-mitmproxy')

DEFAULT_CHROMEVERSION = '77'


class SignalHandler:

    def __init__(self):
        signal.signal(signal.SIGINT, self.handle_signal)
        signal.signal(signal.SIGTERM, self.handle_signal)

    def handle_signal(self, signum, frame):
        raise SignalHandlerException("Program aborted due to signal %s" % signum)


class SignalHandlerException(Exception):
    pass


class Perftest(object):
    """Abstract base class for perftests that execute via a subharness,
either Raptor or browsertime."""

    __metaclass__ = ABCMeta

    def __init__(self, app, binary, run_local=False, noinstall=False,
                 obj_path=None, profile_class=None, installerpath=None,
                 gecko_profile=False, gecko_profile_interval=None, gecko_profile_entries=None,
                 symbols_path=None, host=None, power_test=False, cpu_test=False, memory_test=False,
                 is_release_build=False, debug_mode=False, post_startup_delay=None,
                 interrupt_handler=None, e10s=True, enable_webrender=False,
                 results_handler_class=RaptorResultsHandler, no_conditioned_profile=False,
                 device_name=None, extra_prefs={}, **kwargs):

        # Override the magic --host HOST_IP with the value of the environment variable.
        if host == 'HOST_IP':
            host = os.environ['HOST_IP']

        self.config = {
            'app': app,
            'binary': binary,
            'platform': mozinfo.os,
            'processor': mozinfo.processor,
            'run_local': run_local,
            'obj_path': obj_path,
            'gecko_profile': gecko_profile,
            'gecko_profile_interval': gecko_profile_interval,
            'gecko_profile_entries': gecko_profile_entries,
            'symbols_path': symbols_path,
            'host': host,
            'power_test': power_test,
            'memory_test': memory_test,
            'cpu_test': cpu_test,
            'is_release_build': is_release_build,
            'enable_control_server_wait': memory_test or cpu_test,
            'e10s': e10s,
            'enable_webrender': enable_webrender,
            'no_conditioned_profile': no_conditioned_profile,
            'device_name': device_name,
            'enable_fission': extra_prefs.get('fission.autostart', False),
            'extra_prefs': extra_prefs
        }

        self.firefox_android_apps = FIREFOX_ANDROID_APPS
        # See bug 1582757; until we support aarch64 conditioned-profile builds, fall back
        # to mozrunner-created profiles
        self.no_condprof = ((self.config['platform'] == 'win'
                             and self.config['processor'] == 'aarch64') or
                            self.config['no_conditioned_profile'])

        # We can never use e10s on fennec
        if self.config['app'] == 'fennec':
            self.config['e10s'] = False

        self.browser_name = None
        self.browser_version = None

        self.raptor_venv = os.path.join(os.getcwd(), 'raptor-venv')
        self.installerpath = installerpath
        self.playback = None
        self.benchmark = None
        self.benchmark_port = 0
        self.gecko_profiler = None
        self.post_startup_delay = post_startup_delay
        self.device = None
        self.profile_class = profile_class or app
        self.conditioned_profile_dir = None
        self.interrupt_handler = interrupt_handler
        self.results_handler = results_handler_class(**self.config)

        self.browser_name, self.browser_version = self.get_browser_meta()

        browser_name, browser_version = self.get_browser_meta()
        self.results_handler.add_browser_meta(self.config['app'], browser_version)

        # debug mode is currently only supported when running locally
        self.debug_mode = debug_mode if self.config['run_local'] else False

        # if running debug-mode reduce the pause after browser startup
        if self.debug_mode:
            self.post_startup_delay = min(self.post_startup_delay, 3000)
            LOG.info("debug-mode enabled, reducing post-browser startup pause to %d ms"
                     % self.post_startup_delay)
        # if using conditioned profiles in CI, reduce default browser-settle
        # time down to 1 second, from 30
        if not self.no_condprof and not self.config['run_local'] \
                and post_startup_delay == 30000:
            self.post_startup_delay = 1000
            LOG.info("using conditioned profiles, so reducing post_startup_delay to %d ms"
                     % self.post_startup_delay)
        LOG.info("main raptor init, config is: %s" % str(self.config))

        self.build_browser_profile()

    @property
    def is_localhost(self):
        return self.config.get('host') in ('localhost', '127.0.0.1')

    def get_conditioned_profile(self):
        """Downloads a platform-specific conditioned profile, using the
        condprofile client API; returns a self.conditioned_profile_dir"""

        # create a temp file to help ensure uniqueness
        temp_download_dir = tempfile.mkdtemp()
        LOG.info("Making temp_download_dir from inside get_conditioned_profile {}"
                 .format(temp_download_dir))
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
        try:
            cond_prof_target_dir = get_profile(temp_download_dir, platform, "settled")
        except ProfileNotFoundError:
            # If we can't find the profile on mozilla-central, we look on try
            cond_prof_target_dir = get_profile(temp_download_dir, platform,
                                               "settled", repo="try")
        # now get the full directory path to our fetched conditioned profile
        self.conditioned_profile_dir = os.path.join(temp_download_dir, cond_prof_target_dir)
        if not os.path.exists(cond_prof_target_dir):
            LOG.critical("Can't find target_dir {}, from get_profile()"
                         "temp_download_dir {}, platform {}, settled"
                         .format(cond_prof_target_dir, temp_download_dir, platform))
            raise OSError

        LOG.info("self.conditioned_profile_dir is now set: {}"
                 .format(self.conditioned_profile_dir))
        shutil.rmtree(temp_download_dir)

        return self.conditioned_profile_dir

    def build_browser_profile(self):
        if self.no_condprof:
            self.profile = create_profile(self.profile_class)
        else:
            self.get_conditioned_profile()
            # use mozprofile to create a profile for us, from our conditioned profile's path
            self.profile = create_profile(self.profile_class, profile=self.conditioned_profile_dir)
        # Merge extra profile data from testing/profiles
        with open(os.path.join(self.profile_data_dir, 'profiles.json'), 'r') as fh:
            base_profiles = json.load(fh)['raptor']

        for profile in base_profiles:
            path = os.path.join(self.profile_data_dir, profile)
            LOG.info("Merging profile: {}".format(path))
            self.profile.merge(path)

        if self.config['extra_prefs'].get('fission.autostart', False):
            LOG.info('Enabling fission via browser preferences')
            LOG.info('Browser preferences: {}'.format(self.config['extra_prefs']))
        self.profile.set_preferences(self.config['extra_prefs'])

        # share the profile dir with the config and the control server
        self.config['local_profile_dir'] = self.profile.profile
        LOG.info('Local browser profile: {}'.format(self.profile.profile))

    @property
    def profile_data_dir(self):
        if 'MOZ_DEVELOPER_REPO_DIR' in os.environ:
            return os.path.join(os.environ['MOZ_DEVELOPER_REPO_DIR'], 'testing', 'profiles')
        if build:
            return os.path.join(build.topsrcdir, 'testing', 'profiles')
        return os.path.join(here, 'profile_data')

    @property
    def artifact_dir(self):
        artifact_dir = os.getcwd()
        if self.config.get('run_local', False):
            if 'MOZ_DEVELOPER_REPO_DIR' in os.environ:
                artifact_dir = os.path.join(os.environ['MOZ_DEVELOPER_REPO_DIR'],
                                            'testing', 'mozharness', 'build')
            else:
                artifact_dir = here
        elif os.getenv('MOZ_UPLOAD_DIR'):
            artifact_dir = os.getenv('MOZ_UPLOAD_DIR')
        return artifact_dir

    @abstractmethod
    def run_test_setup(self, test):
        LOG.info("starting test: %s" % test['name'])

        # if 'alert_on' was provided in the test INI, add to our config for results/output
        self.config['subtest_alert_on'] = test.get('alert_on')

        if test.get('playback') is not None and self.playback is None:
            self.start_playback(test)

        if test.get("preferences") is not None:
            self.set_browser_test_prefs(test['preferences'])

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
                    self.run_test(test, timeout=int(test.get('page_timeout')))
                except RuntimeError as e:
                    LOG.critical("Tests failed to finish! Application timed out.")
                    LOG.error(e)
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
        if self.config['gecko_profile']:
            self.gecko_profiler.symbolicate()
            # clean up the temp gecko profiling folders
            LOG.info("cleaning up after gecko profiling")
            self.gecko_profiler.clean()

    def process_results(self, tests, test_names):
        # when running locally output results in build/raptor.json; when running
        # in production output to a local.json to be turned into tc job artifact
        raptor_json_path = os.path.join(self.artifact_dir, 'raptor.json')
        if not self.config.get('run_local', False):
            raptor_json_path = os.path.join(os.getcwd(), 'local.json')

        self.config['raptor_json_path'] = raptor_json_path
        self.config['artifact_dir'] = self.artifact_dir
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
            LOG.info("No playback recordings specified in the test; so not getting recording info")
            return

        for r in _recording_paths:
            json_path = '{}.json'.format(r.split('.')[0])

            if os.path.exists(json_path):
                with open(json_path) as f:
                    recording_date = json.loads(f.read()).get('recording_date')

                    if recording_date is not None:
                        LOG.info('Playback recording date: {} '.
                                 format(recording_date.split(' ')[0]))
                    else:
                        LOG.info('Playback recording date not available')
            else:
                LOG.info('Playback recording information not available')

    def get_playback_config(self, test):
        platform = self.config['platform']
        playback_dir = os.path.join(here, 'playback')

        self.config.update({
            'playback_tool': test.get('playback'),
            'playback_version': test.get('playback_version', "4.0.4"),
            'playback_binary_zip': test.get('playback_binary_zip_%s' % platform),
            'playback_pageset_zip': test.get('playback_pageset_zip_%s' % platform),
            'playback_binary_manifest': test.get('playback_binary_manifest'),
            'playback_pageset_manifest': test.get('playback_pageset_manifest'),
        })

        for key in ('playback_pageset_manifest', 'playback_pageset_zip'):
            if self.config.get(key) is None:
                continue
            self.config[key] = os.path.join(playback_dir, self.config[key])

        LOG.info("test uses playback tool: %s " % self.config['playback_tool'])

    def delete_proxy_settings_from_profile(self):
        # Must delete the proxy settings from the profile if running
        # the test with a host different from localhost.
        userjspath = os.path.join(self.profile.profile, 'user.js')
        with open(userjspath) as userjsfile:
            prefs = userjsfile.readlines()
        prefs = [pref for pref in prefs if 'network.proxy' not in pref]
        with open(userjspath, 'w') as userjsfile:
            userjsfile.writelines(prefs)

    def start_playback(self, test):
        # creating the playback tool
        self.get_playback_config(test)
        self.playback = get_playback(self.config)

        self.playback.config['playback_files'] = self.get_recording_paths(test)

        # let's start it!
        self.playback.start()

        self.log_recording_dates(test)

    def _init_gecko_profiling(self, test):
        LOG.info("initializing gecko profiler")
        upload_dir = os.getenv('MOZ_UPLOAD_DIR')
        if not upload_dir:
            LOG.critical("Profiling ignored because MOZ_UPLOAD_DIR was not set")
        else:
            self.gecko_profiler = GeckoProfile(upload_dir,
                                               self.config,
                                               test)


class PerftestDesktop(Perftest):
    """Mixin class for Desktop-specific Perftest subclasses"""

    def setup_chrome_args(self, test):
        '''Sets up chrome/chromium cmd-line arguments.

        Needs to be "implemented" here to deal with Python 2
        unittest failures.
        '''
        raise NotImplementedError

    def desktop_chrome_args(self, test):
        '''Returns cmd line options required to run pageload tests on Desktop Chrome
        and Chromium. Also add the cmd line options to turn on the proxy and
        ignore security certificate errors if using host localhost, 127.0.0.1.
        '''
        chrome_args = [
            '--use-mock-keychain',
            '--no-default-browser-check',
        ]

        if test.get('playback', False):
            pb_args = [
                '--proxy-server=%s:%d' % (self.playback.host, self.playback.port),
                '--proxy-bypass-list=localhost;127.0.0.1',
                '--ignore-certificate-errors',
            ]

            if not self.is_localhost:
                pb_args[0] = pb_args[0].replace('127.0.0.1', self.config['host'])

            chrome_args.extend(pb_args)

        if self.debug_mode:
            chrome_args.extend(['--auto-open-devtools-for-tabs'])

        return chrome_args

    def get_browser_meta(self):
        '''Returns the browser name and version in a tuple (name, version).

        On desktop, we use mozversion but a fallback method also exists
        for non-firefox browsers, where mozversion is known to fail. The
        methods are OS-specific, with windows being the outlier.
        '''
        browser_name = None
        browser_version = None

        try:
            meta = mozversion.get_version(binary=self.config['binary'])
            browser_name = meta.get('application_name')
            browser_version = meta.get('application_version')
        except Exception as e:
            LOG.warning(
                "Failed to get browser meta data through mozversion: %s-%s" %
                (e.__class__.__name__, e)
            )
            LOG.info("Attempting to get version through fallback method...")

            # Fall-back method to get browser version on desktop
            try:
                if 'linux' in self.config['platform'] or 'mac' in self.config['platform']:
                    command = [self.config['binary'], '--version']
                    proc = mozprocess.ProcessHandler(command)
                    proc.run(timeout=10, outputTimeout=10)
                    proc.wait()

                    bmeta = proc.output
                    meta_re = re.compile(r"([A-z\s]+)\s+([\w.]*)")
                    if len(bmeta) != 0:
                        match = meta_re.match(bmeta[0])
                        if match:
                            browser_name = self.config['app']
                            browser_version = match.group(2)
                    else:
                        LOG.info("Couldn't get browser version and name")
                else:
                    # On windows we need to use wimc to get the version
                    command = r'wmic datafile where name="{0}"'.format(
                        self.config['binary'].replace('\\', r"\\")
                    )
                    bmeta = subprocess.check_output(command)

                    meta_re = re.compile(r"\s+([\d.a-z]+)\s+")
                    match = meta_re.findall(bmeta)
                    if len(match) > 0:
                        browser_name = self.config['app']
                        browser_version = match[-1]
                    else:
                        LOG.info("Couldn't get browser version and name")
            except Exception as e:
                LOG.warning(
                    "Failed to get browser meta data through fallback method: %s-%s" %
                    (e.__class__.__name__, e)
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


class PerftestAndroid(Perftest):
    """Mixin class for Android-specific Perftest subclasses."""

    def setup_chrome_args(self, test):
        '''Sets up chrome/chromium cmd-line arguments.

        Needs to be "implemented" here to deal with Python 2
        unittest failures.
        '''
        raise NotImplementedError

    def get_browser_meta(self):
        '''Returns the browser name and version in a tuple (name, version).

        Uses mozversion as the primary method to get this meta data and for
        android this is the only method which exists to get this data. With android,
        we use the installerpath attribute to determine this and this only works
        with Firefox browsers.
        '''
        browser_name = None
        browser_version = None

        if self.config['app'] in self.firefox_android_apps:
            try:
                meta = mozversion.get_version(binary=self.installerpath)
                browser_name = meta.get('application_name')
                browser_version = meta.get('application_version')
            except Exception as e:
                LOG.warning(
                    "Failed to get android browser meta data through mozversion: %s-%s" %
                    (e.__class__.__name__, e)
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
        self.device.create_socket_connection('reverse', tcp_port, tcp_port)

    def set_reverse_ports(self):
        if self.is_localhost:

            # only raptor-webext uses the control server
            if self.config.get('browsertime', False) is False:
                LOG.info("making the raptor control server port available to device")
                self.set_reverse_port(self.control_server.port)

            if self.playback:
                LOG.info("making the raptor playback server port available to device")
                self.set_reverse_port(self.playback.port)

            if self.benchmark:
                LOG.info("making the raptor benchmarks server port available to device")
                self.set_reverse_port(self.benchmark_port)
        else:
            LOG.info("Reverse port forwarding is uded only on local devices")

    def build_browser_profile(self):
        super(PerftestAndroid, self).build_browser_profile()

        # Merge in the Android profile.
        path = os.path.join(self.profile_data_dir, 'raptor-android')
        LOG.info("Merging profile: {}".format(path))
        self.profile.merge(path)
        self.profile.set_preferences({'browser.tabs.remote.autostart': self.config['e10s']})

    def clear_app_data(self):
        LOG.info("clearing %s app data" % self.config['binary'])
        self.device.shell("pm clear %s" % self.config['binary'])

    def set_debug_app_flag(self):
        # required so release apks will read the android config.yml file
        LOG.info("setting debug-app flag for %s" % self.config['binary'])
        self.device.shell("am set-debug-app --persistent %s" % self.config['binary'])

    def copy_profile_to_device(self):
        """Copy the profile to the device, and update permissions of all files."""
        if not self.device.is_app_installed(self.config['binary']):
            raise Exception('%s is not installed' % self.config['binary'])

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
        proxy_prefs["network.proxy.no_proxies_on"] = self.config['host']

        LOG.info("setting profile prefs to turn on the android app proxy: {}".format(proxy_prefs))
        self.profile.set_preferences(proxy_prefs)


class Browsertime(Perftest):
    """Abstract base class for Browsertime"""

    __metaclass__ = ABCMeta

    @property
    @abstractmethod
    def browsertime_args(self):
        pass

    def __init__(self, app, binary, process_handler=None, **kwargs):
        self.process_handler = process_handler or mozprocess.ProcessHandler
        for key in list(kwargs):
            if key.startswith('browsertime_'):
                value = kwargs.pop(key)
                setattr(self, key, value)

        def klass(**config):
            root_results_dir = os.path.join(os.environ.get('MOZ_UPLOAD_DIR', os.getcwd()),
                                            'browsertime-results')
            return BrowsertimeResultsHandler(config, root_results_dir=root_results_dir)

        super(Browsertime, self).__init__(app, binary, results_handler_class=klass, **kwargs)
        LOG.info("cwd: '{}'".format(os.getcwd()))
        self.config['browsertime'] = True

        # For debugging.
        for k in ("browsertime_node",
                  "browsertime_browsertimejs",
                  "browsertime_ffmpeg",
                  "browsertime_geckodriver",
                  "browsertime_chromedriver"):
            try:
                if not self.browsertime_video and k == "browsertime_ffmpeg":
                    continue
                LOG.info("{}: {}".format(k, getattr(self, k)))
                LOG.info("{}: {}".format(k, os.stat(getattr(self, k))))
            except Exception as e:
                LOG.info("{}: {}".format(k, e))

    def build_browser_profile(self):
        super(Browsertime, self).build_browser_profile()
        self.remove_mozprofile_delimiters_from_profile()

    def remove_mozprofile_delimiters_from_profile(self):
        # Perftest.build_browser_profile uses mozprofile to create the profile and merge in prefs;
        # while merging, mozprofile adds in special delimiters; these delimiters are not recognized
        # by selenium-webdriver ultimately causing Firefox launch to fail. So we must remove these
        # delimiters from the browser profile before passing into btime via firefox.profileTemplate
        LOG.info("Removing mozprofile delimiters from browser profile")
        userjspath = os.path.join(self.profile.profile, 'user.js')
        try:
            with open(userjspath) as userjsfile:
                lines = userjsfile.readlines()
            lines = [line for line in lines if not line.startswith('#MozRunner')]
            with open(userjspath, 'w') as userjsfile:
                userjsfile.writelines(lines)
        except Exception as e:
            LOG.critical("Exception {} while removing mozprofile delimiters".format(e))

    def set_browser_test_prefs(self, raw_prefs):
        # add test specific preferences
        LOG.info("setting test-specific Firefox preferences")
        self.profile.set_preferences(json.loads(raw_prefs))
        self.remove_mozprofile_delimiters_from_profile()

    def run_test_setup(self, test):
        super(Browsertime, self).run_test_setup(test)

        if test.get('type') == "benchmark":
            # benchmark-type tests require the benchmark test to be served out
            self.benchmark = Benchmark(self.config, test)
            test['test_url'] = test['test_url'].replace('<host>', self.benchmark.host)
            test['test_url'] = test['test_url'].replace('<port>', self.benchmark.port)

        if test.get('playback') is not None and self.playback is None:
            self.start_playback(test)

        # TODO: geckodriver/chromedriver from tasks.
        self.driver_paths = []
        if self.browsertime_geckodriver:
            self.driver_paths.extend(['--firefox.geckodriverPath', self.browsertime_geckodriver])
        if self.browsertime_chromedriver:
            if not self.config.get('run_local', None) or '{}' in self.browsertime_chromedriver:
                if self.browser_version:
                    bvers = str(self.browser_version)
                    chromedriver_version = bvers.split('.')[0]
                else:
                    chromedriver_version = DEFAULT_CHROMEVERSION

                self.browsertime_chromedriver = self.browsertime_chromedriver.format(
                    chromedriver_version
                )

            self.driver_paths.extend([
                '--chrome.chromedriverPath',
                self.browsertime_chromedriver
            ])

        LOG.info('test: {}'.format(test))

    def run_test_teardown(self, test):
        super(Browsertime, self).run_test_teardown(test)

        # if we were using a playback tool, stop it
        if self.playback is not None:
            self.playback.stop()
            self.playback = None

    def check_for_crashes(self):
        super(Browsertime, self).check_for_crashes()

    def clean_up(self):
        super(Browsertime, self).clean_up()

    def _compose_cmd(self, test, timeout):
        browsertime_script = [os.path.join(os.path.dirname(__file__), "..",
                              "browsertime", "browsertime_pageload.js")]

        btime_args = self.browsertime_args
        if self.config['app'] in ('chrome', 'chromium'):
            btime_args.extend(self.setup_chrome_args(test))

        browsertime_script.extend(btime_args)

        # pass a few extra options to the browsertime script
        # XXX maybe these should be in the browsertime_args() func
        browsertime_script.extend(["--browsertime.page_cycles",
                                   str(test.get("page_cycles", 1))])
        browsertime_script.extend(["--browsertime.url", test["test_url"]])

        # Raptor's `pageCycleDelay` delay (ms) between pageload cycles
        browsertime_script.extend(["--browsertime.page_cycle_delay", "1000"])

        # Raptor's `post startup delay` is settle time after the browser has started
        browsertime_script.extend(["--browsertime.post_startup_delay",
                                   str(self.post_startup_delay)])

        browsertime_options = ['--firefox.profileTemplate', str(self.profile.profile),
                               '--skipHar',
                               '--video', self.browsertime_video and 'true' or 'false',
                               '--visualMetrics', 'false',
                               # url load timeout (milliseconds)
                               '--timeouts.pageLoad', str(timeout),
                               # running browser scripts timeout (milliseconds)
                               '--timeouts.script', str(timeout * int(test.get("page_cycles", 1))),
                               '-vvv',
                               '--resultDir', self.results_handler.result_dir_for_test(test)]

        # have browsertime use our newly-created conditioned-profile path
        if not self.no_condprof:
            self.profile.profile = self.conditioned_profile_dir

        if self.config['gecko_profile']:
            self.config['browsertime_result_dir'] = self.results_handler.result_dir_for_test(test)
            self._init_gecko_profiling(test)
            browsertime_options.append('--firefox.geckoProfiler')

            for option, browser_time_option in (('gecko_profile_interval',
                                                 '--firefox.geckoProfilerParams.interval'),
                                                ('gecko_profile_entries',
                                                 '--firefox.geckoProfilerParams.bufferSize')):
                value = self.config.get(option)
                if value is None:
                    value = test.get(option)
                if value is not None:
                    browsertime_options.extend([browser_time_option, str(value)])

        return ([self.browsertime_node, self.browsertime_browsertimejs] +
                self.driver_paths +
                browsertime_script +
                # -n option for the browsertime to restart the browser
                browsertime_options +
                ['-n', str(test.get('browser_cycles', 1))])

    def _compute_process_timeout(self, test, timeout):
        # bt_timeout will be the overall browsertime cmd/session timeout (seconds)
        # browsertime deals with page cycles internally, so we need to give it a timeout
        # value that includes all page cycles
        bt_timeout = int(timeout / 1000) * int(test.get("page_cycles", 1))

        # the post-startup-delay is a delay after the browser has started, to let it settle
        # it's handled within browsertime itself by loading a 'preUrl' (about:blank) and having a
        # delay after that ('preURLDelay') as the post-startup-delay, so we must add that in sec
        bt_timeout += int(self.post_startup_delay / 1000)

        # add some time for browser startup, time for the browsertime measurement code
        # to be injected/invoked, and for exceptions to bubble up; be generous
        bt_timeout += 20

        # browsertime also handles restarting the browser/running all of the browser cycles;
        # so we need to multiply our bt_timeout by the number of browser cycles
        bt_timeout = bt_timeout * int(test.get('browser_cycles', 1))

        # if geckoProfile enabled, give browser more time for profiling
        if self.config['gecko_profile'] is True:
            bt_timeout += 5 * 60
        return bt_timeout

    def run_test(self, test, timeout):
        self.run_test_setup(test)
        # timeout is a single page-load timeout value (ms) from the test INI
        # this will be used for btime --timeouts.pageLoad
        cmd = self._compose_cmd(test, timeout)

        if test.get('type') == "benchmark":
            cmd.extend(['--script',
                        os.path.join(os.path.dirname(__file__), "..",
                                     "browsertime", "browsertime_benchmark.js")
                        ])

        LOG.info('timeout (s): {}'.format(timeout))
        LOG.info('browsertime cwd: {}'.format(os.getcwd()))
        LOG.info('browsertime cmd: {}'.format(" ".join(cmd)))
        if self.browsertime_video:
            LOG.info('browsertime_ffmpeg: {}'.format(self.browsertime_ffmpeg))

        # browsertime requires ffmpeg on the PATH for `--video=true`.
        # It's easier to configure the PATH here than at the TC level.
        env = dict(os.environ)
        if self.browsertime_video and self.browsertime_ffmpeg:
            ffmpeg_dir = os.path.dirname(os.path.abspath(self.browsertime_ffmpeg))
            old_path = env.setdefault('PATH', '')
            new_path = os.pathsep.join([ffmpeg_dir, old_path])
            if isinstance(new_path, six.text_type):
                # Python 2 doesn't like unicode in the environment.
                new_path = new_path.encode('utf-8', 'strict')
            env['PATH'] = new_path

        LOG.info('PATH: {}'.format(env['PATH']))

        try:
            proc = self.process_handler(cmd, env=env)
            proc.run(timeout=self._compute_process_timeout(test, timeout),
                     outputTimeout=2*60)
            proc.wait()

        except Exception as e:
            LOG.critical("Error while attempting to run browsertime: %s" % str(e))
            raise


class BrowsertimeDesktop(PerftestDesktop, Browsertime):
    def __init__(self, *args, **kwargs):
        super(BrowsertimeDesktop, self).__init__(*args, **kwargs)

    @property
    def browsertime_args(self):
        binary_path = self.config['binary']
        LOG.info('binary_path: {}'.format(binary_path))

        if self.config['app'] == 'chrome':
            return ['--browser', self.config['app'], '--chrome.binaryPath', binary_path]
        return ['--browser', self.config['app'], '--firefox.binaryPath', binary_path]

    def setup_chrome_args(self, test):
        # Setup required chrome arguments
        chrome_args = self.desktop_chrome_args(test)

        # Add this argument here, it's added by mozrunner
        # for raptor
        chrome_args.append('--no-first-run')

        btime_chrome_args = []
        for arg in chrome_args:
            btime_chrome_args.extend([
                '--chrome.args=' + str(arg.replace("'", '"'))
            ])

        return btime_chrome_args


class BrowsertimeAndroid(PerftestAndroid, Browsertime):
    '''
    When running raptor-browsertime tests on android, we create the profile (and set the proxy
    prefs in the profile is using playback) but we don't need to copy it onto the device because
    geckodriver takes care of that. We tell browsertime to use our profile (we pass it in with
    the firefox.profileTemplate arg); browsertime creates a copy of that and passes that into
    geckodriver. Geckodriver then takes the profile and copies it onto the mobile device's sdcard
    for us; and then it even writes the geckoview app config.yaml file onto the device, which
    points the app to the profile on the sdcard. Therefore raptor doesn't have to copy the profile
    onto the scard (and create the config.yaml) file ourselves. Also note when using playback, the
    nss certificate db is created as usual when mitmproxy is started (and saved in the profile) so
    it is already included in the profile that browsertime/geckodriver copies onto the device.
    '''
    def __init__(self, app, binary, activity=None, intent=None, **kwargs):
        super(BrowsertimeAndroid, self).__init__(app, binary, profile_class="firefox", **kwargs)

        self.config.update({
            'activity': activity,
            'intent': intent,
        })

        self.remote_test_root = os.path.abspath(os.path.join(os.sep, 'sdcard', 'raptor'))
        self.remote_profile = os.path.join(self.remote_test_root, "profile")

    @property
    def browsertime_args(self):
        args_list = ['--browser', 'firefox', '--android',
                     # Work around a `selenium-webdriver` issue where Browsertime
                     # fails to find a Firefox binary even though we're going to
                     # actually do things on an Android device.
                     '--firefox.binaryPath', self.browsertime_node,
                     '--firefox.android.package', self.config['binary'],
                     '--firefox.android.activity', self.config['activity']]

        # if running on Fenix we must add the intent as we use a special non-default one there
        if self.config['app'] == "fenix" and self.config.get('intent') is not None:
            args_list.extend(['--firefox.android.intentArgument=-a'])
            args_list.extend(['--firefox.android.intentArgument', self.config['intent']])
            args_list.extend(['--firefox.android.intentArgument=-d'])
            args_list.extend(['--firefox.android.intentArgument', str('about:blank')])

        return args_list

    def build_browser_profile(self):
        super(BrowsertimeAndroid, self).build_browser_profile()

        # Merge in the Android profile.
        path = os.path.join(self.profile_data_dir, 'raptor-android')
        LOG.info("Merging profile: {}".format(path))
        self.profile.merge(path)
        self.profile.set_preferences({'browser.tabs.remote.autostart': self.config['e10s']})

        # There's no great way to have "after" advice in Python, so we do this
        # in super and then again here since the profile merging re-introduces
        # the "#MozRunner" delimiters.
        self.remove_mozprofile_delimiters_from_profile()

    def setup_adb_device(self):
        if self.device is None:
            self.device = ADBDevice(verbose=True)
            tune_performance(self.device, log=LOG)

        self.clear_app_data()
        self.set_debug_app_flag()

    def run_test_setup(self, test):
        super(BrowsertimeAndroid, self).run_test_setup(test)

        self.set_reverse_ports()
        self.turn_on_android_app_proxy()
        self.remove_mozprofile_delimiters_from_profile()

    def run_tests(self, tests, test_names):
        self.setup_adb_device()

        return super(BrowsertimeAndroid, self).run_tests(tests, test_names)

    def run_test_teardown(self, test):
        LOG.info('removing reverse socket connections')
        self.device.remove_socket_connections('reverse')

        super(BrowsertimeAndroid, self).run_test_teardown(test)


class Raptor(Perftest):
    """Container class for Raptor"""

    def __init__(self, *args, **kwargs):
        self.raptor_webext = None
        self.control_server = None
        self.cpu_profiler = None

        super(Raptor, self).__init__(*args, **kwargs)

        # set up the results handler
        self.results_handler = RaptorResultsHandler(
            gecko_profile=self.config.get('gecko_profile'),
            power_test=self.config.get('power_test'),
            cpu_test=self.config.get('cpu_test'),
            memory_test=self.config.get('memory_test'),
            no_conditioned_profile=self.config['no_conditioned_profile'],
            extra_prefs=self.config.get('extra_prefs')
        )
        browser_name, browser_version = self.get_browser_meta()
        self.results_handler.add_browser_meta(self.config['app'], browser_version)

        self.start_control_server()

    def run_test_setup(self, test):
        super(Raptor, self).run_test_setup(test)

        LOG.info("starting raptor test: %s" % test['name'])
        LOG.info("test settings: %s" % str(test))
        LOG.info("raptor config: %s" % str(self.config))

        if test.get('type') == "benchmark":
            self.serve_benchmark_source(test)

        gen_test_config(
            self.config['app'],
            test['name'],
            self.control_server.port,
            self.post_startup_delay,
            host=self.config['host'],
            b_port=self.benchmark_port,
            debug_mode=1 if self.debug_mode else 0,
            browser_cycle=test.get('browser_cycle', 1),
        )

        self.install_raptor_webext()

    def wait_for_test_finish(self, test, timeout):
        # this is a 'back-stop' i.e. if for some reason Raptor doesn't finish for some
        # serious problem; i.e. the test was unable to send a 'page-timeout' to the control
        # server, etc. Therefore since this is a 'back-stop' we want to be generous here;
        # we don't want this timeout occurring unless abosultely necessary

        # convert timeout to seconds and account for page cycles
        timeout = int(timeout / 1000) * int(test.get('page_cycles', 1))
        # account for the pause the raptor webext runner takes after browser startup
        # and the time an exception is propagated through the framework
        timeout += (int(self.post_startup_delay / 1000) + 10)

        # for page-load tests we don't start the page-timeout timer until the pageload.js content
        # is successfully injected and invoked; which differs per site being tested; therefore we
        # need to be generous here - let's add 10 seconds extra per page-cycle
        if test.get('type') == "pageload":
            timeout += (10 * int(test.get('page_cycles', 1)))

        # if geckoProfile enabled, give browser more time for profiling
        if self.config['gecko_profile'] is True:
            timeout += 5 * 60

        # we also need to give time for results processing, not just page/browser cycles!
        timeout += 60

        elapsed_time = 0
        while not self.control_server._finished:
            if self.config['enable_control_server_wait']:
                response = self.control_server_wait_get()
                if response == 'webext_shutdownBrowser':
                    if self.config['memory_test']:
                        generate_android_memory_profile(self, test['name'])
                    if self.cpu_profiler:
                        self.cpu_profiler.generate_android_cpu_profile(test['name'])

                    self.control_server_wait_continue()
            time.sleep(1)
            # we only want to force browser-shutdown on timeout if not in debug mode;
            # in debug-mode we leave the browser running (require manual shutdown)
            if not self.debug_mode:
                elapsed_time += 1
                if elapsed_time > (timeout) - 5:  # stop 5 seconds early
                    self.control_server.wait_for_quit()
                    raise RuntimeError("Test failed to finish. "
                                       "Application timed out after {} seconds".format(timeout))

    def run_test_teardown(self, test):
        super(Raptor, self).run_test_teardown(test)

        if self.playback is not None:
            self.playback.stop()
            self.playback = None

        self.remove_raptor_webext()

    def set_browser_test_prefs(self, raw_prefs):
        # add test specific preferences
        LOG.info("setting test-specific Firefox preferences")
        self.profile.set_preferences(json.loads(raw_prefs))

    def build_browser_profile(self):
        super(Raptor, self).build_browser_profile()

        if self.control_server:
            # The control server and the browser profile are not well factored
            # at this time, so the start-up process overlaps.  Accommodate.
            self.control_server.user_profile = self.profile

    def start_control_server(self):
        self.control_server = RaptorControlServer(self.results_handler, self.debug_mode)
        self.control_server.user_profile = self.profile
        self.control_server.start()

        if self.config['enable_control_server_wait']:
            self.control_server_wait_set('webext_shutdownBrowser')

    def serve_benchmark_source(self, test):
        # benchmark-type tests require the benchmark test to be served out
        self.benchmark = Benchmark(self.config, test)
        self.benchmark_port = int(self.benchmark.port)

    def install_raptor_webext(self):
        # must intall raptor addon each time because we dynamically update some content
        # the webext is installed into the browser profile
        # note: for chrome the addon is just a list of paths that ultimately are added
        # to the chromium command line '--load-extension' argument
        self.raptor_webext = os.path.join(webext_dir, 'raptor')
        LOG.info("installing webext %s" % self.raptor_webext)
        self.profile.addons.install(self.raptor_webext)

        # on firefox we can get an addon id; chrome addon actually is just cmd line arg
        try:
            self.webext_id = self.profile.addons.addon_details(self.raptor_webext)['id']
        except AttributeError:
            self.webext_id = None

    def remove_raptor_webext(self):
        # remove the raptor webext; as it must be reloaded with each subtest anyway
        if not self.raptor_webext:
            LOG.info("raptor webext not installed - not attempting removal")
            return

        LOG.info("removing webext %s" % self.raptor_webext)
        if self.config['app'] in ['firefox', 'geckoview', 'fennec', 'refbrow', 'fenix']:
            self.profile.addons.remove_addon(self.webext_id)

        # for chrome the addon is just a list (appended to cmd line)
        chrome_apps = CHROMIUM_DISTROS + ["chrome-android", "chromium-android"]
        if self.config['app'] in chrome_apps:
            self.profile.addons.remove(self.raptor_webext)

    def clean_up(self):
        super(Raptor, self).clean_up()

        if self.config['enable_control_server_wait']:
            self.control_server_wait_clear('all')

        self.control_server.stop()
        LOG.info("finished")

    def control_server_wait_set(self, state):
        response = requests.post("http://127.0.0.1:%s/" % self.control_server.port,
                                 json={"type": "wait-set", "data": state})
        return response.text

    def control_server_wait_timeout(self, timeout):
        response = requests.post("http://127.0.0.1:%s/" % self.control_server.port,
                                 json={"type": "wait-timeout", "data": timeout})
        return response.text

    def control_server_wait_get(self):
        response = requests.post("http://127.0.0.1:%s/" % self.control_server.port,
                                 json={"type": "wait-get", "data": ""})
        return response.text

    def control_server_wait_continue(self):
        response = requests.post("http://127.0.0.1:%s/" % self.control_server.port,
                                 json={"type": "wait-continue", "data": ""})
        return response.text

    def control_server_wait_clear(self, state):
        response = requests.post("http://127.0.0.1:%s/" % self.control_server.port,
                                 json={"type": "wait-clear", "data": state})
        return response.text


class RaptorDesktop(PerftestDesktop, Raptor):

    def __init__(self, *args, **kwargs):
        super(RaptorDesktop, self).__init__(*args, **kwargs)

        # create the desktop browser runner
        LOG.info("creating browser runner using mozrunner")
        self.output_handler = OutputHandler()
        process_args = {
            'processOutputLine': [self.output_handler],
        }
        runner_cls = runners[self.config['app']]
        self.runner = runner_cls(
            self.config['binary'], profile=self.profile, process_args=process_args,
            symbols_path=self.config['symbols_path'])

        if self.config['enable_webrender']:
            self.runner.env['MOZ_WEBRENDER'] = '1'
            self.runner.env['MOZ_ACCELERATED'] = '1'
        else:
            self.runner.env['MOZ_WEBRENDER'] = '0'

    def launch_desktop_browser(self, test):
        raise NotImplementedError

    def start_runner_proc(self):
        # launch the browser via our previously-created runner
        self.runner.start()

        proc = self.runner.process_handler
        self.output_handler.proc = proc

        # give our control server the browser process so it can shut it down later
        self.control_server.browser_proc = proc

    def run_test(self, test, timeout):
        # tests will be run warm (i.e. NO browser restart between page-cycles)
        # unless otheriwse specified in the test INI by using 'cold = true'
        mozpower_measurer = None
        if self.config.get('power_test', False):
            powertest_name = test['name'].replace('/', '-').replace('\\', '-')
            output_dir = os.path.join(self.artifact_dir, 'power-measurements-%s' % powertest_name)
            test_dir = os.path.join(output_dir, powertest_name)

            try:
                if not os.path.exists(output_dir):
                    os.mkdir(output_dir)
                if not os.path.exists(test_dir):
                    os.mkdir(test_dir)
            except Exception:
                LOG.critical("Could not create directories to store power testing data.")
                raise

            # Start power measurements with IPG creating a power usage log
            # every 30 seconds with 1 data point per second (or a 1000 milli-
            # second sampling rate).
            mozpower_measurer = MozPower(
                ipg_measure_duration=30,
                sampling_rate=1000,
                output_file_path=os.path.join(test_dir, 'power-usage')
            )
            mozpower_measurer.initialize_power_measurements()

        if test.get('cold', False) is True:
            self.__run_test_cold(test, timeout)
        else:
            self.__run_test_warm(test, timeout)

        if mozpower_measurer:
            mozpower_measurer.finalize_power_measurements(test_name=test['name'])
            perfherder_data = mozpower_measurer.get_perfherder_data()

            if not self.config.get('run_local', False):
                # when not running locally, zip the data and delete the folder which
                # was placed in the zip
                powertest_name = test['name'].replace('/', '-').replace('\\', '-')
                power_data_path = os.path.join(
                    self.artifact_dir,
                    'power-measurements-%s' % powertest_name
                )
                shutil.make_archive(power_data_path, 'zip', power_data_path)
                shutil.rmtree(power_data_path)

            for data_type in perfherder_data:
                self.control_server.submit_supporting_data(
                    perfherder_data[data_type]
                )

    def __run_test_cold(self, test, timeout):
        '''
        Run the Raptor test but restart the entire browser app between page-cycles.

        Note: For page-load tests, playback will only be started once - at the beginning of all
        browser cycles, and then stopped after all cycles are finished. That includes the import
        of the mozproxy ssl cert and turning on the browser proxy.

        Since we're running in cold-mode, before this point (in manifest.py) the
        'expected-browser-cycles' value was already set to the initial 'page-cycles' value;
        and the 'page-cycles' value was set to 1 as we want to perform one page-cycle per
        browser restart.

        The 'browser-cycle' value is the current overall browser start iteration. The control
        server will receive the current 'browser-cycle' and the 'expected-browser-cycles' in
        each results set received; and will pass that on as part of the results so that the
        results processing will know results for multiple browser cycles are being received.

        The default will be to run in warm mode; unless 'cold = true' is set in the test INI.
        '''
        LOG.info("test %s is running in cold mode; browser WILL be restarted between "
                 "page cycles" % test['name'])

        for test['browser_cycle'] in range(1, test['expected_browser_cycles'] + 1):

            LOG.info("begin browser cycle %d of %d for test %s"
                     % (test['browser_cycle'], test['expected_browser_cycles'], test['name']))

            self.run_test_setup(test)

            if test['browser_cycle'] == 1:

                if not self.is_localhost:
                    self.delete_proxy_settings_from_profile()

            else:
                # initial browser profile was already created before run_test was called;
                # now additional browser cycles we want to create a new one each time
                self.build_browser_profile()

                # Update runner profile
                self.runner.profile = self.profile

                self.run_test_setup(test)

            # now start the browser/app under test
            self.launch_desktop_browser(test)

            # set our control server flag to indicate we are running the browser/app
            self.control_server._finished = False

            self.wait_for_test_finish(test, timeout)

    def __run_test_warm(self, test, timeout):
        self.run_test_setup(test)

        if not self.is_localhost:
            self.delete_proxy_settings_from_profile()

        # start the browser/app under test
        self.launch_desktop_browser(test)

        # set our control server flag to indicate we are running the browser/app
        self.control_server._finished = False

        self.wait_for_test_finish(test, timeout)

    def run_test_teardown(self, test):
        # browser should be closed by now but this is a backup-shutdown (if not in debug-mode)
        if not self.debug_mode:
            if self.runner.is_running():
                self.runner.stop()
        else:
            # in debug mode, and running locally, leave the browser running
            if self.config['run_local']:
                LOG.info("* debug-mode enabled - please shutdown the browser manually...")
                self.runner.wait(timeout=None)

        super(RaptorDesktop, self).run_test_teardown(test)

    def check_for_crashes(self):
        super(RaptorDesktop, self).check_for_crashes()

        try:
            self.runner.check_for_crashes()
        except NotImplementedError:  # not implemented for Chrome
            pass

    def clean_up(self):
        self.runner.stop()

        super(RaptorDesktop, self).clean_up()


class RaptorDesktopFirefox(RaptorDesktop):

    def disable_non_local_connections(self):
        # For Firefox we need to set MOZ_DISABLE_NONLOCAL_CONNECTIONS=1 env var before startup
        # when testing release builds from mozilla-beta/release. This is because of restrictions
        # on release builds that require webextensions to be signed unless this env var is set
        LOG.info("setting MOZ_DISABLE_NONLOCAL_CONNECTIONS=1")
        os.environ['MOZ_DISABLE_NONLOCAL_CONNECTIONS'] = "1"

    def enable_non_local_connections(self):
        # pageload tests need to be able to access non-local connections via mitmproxy
        LOG.info("setting MOZ_DISABLE_NONLOCAL_CONNECTIONS=0")
        os.environ['MOZ_DISABLE_NONLOCAL_CONNECTIONS'] = "0"

    def launch_desktop_browser(self, test):
        LOG.info("starting %s" % self.config['app'])
        if self.config['is_release_build']:
            self.disable_non_local_connections()

        # if running debug-mode, tell Firefox to open the browser console on startup
        if self.debug_mode:
            self.runner.cmdargs.extend(['-jsconsole'])

        self.start_runner_proc()

        if self.config['is_release_build'] and test.get('playback') is not None:
            self.enable_non_local_connections()

        # if geckoProfile is enabled, initialize it
        if self.config['gecko_profile'] is True:
            self._init_gecko_profiling(test)
            # tell the control server the gecko_profile dir; the control server
            # will receive the filename of the stored gecko profile from the web
            # extension, and will move it out of the browser user profile to
            # this directory; where it is picked-up by gecko_profile.symbolicate
            self.control_server.gecko_profile_dir = self.gecko_profiler.gecko_profile_dir


class RaptorDesktopChrome(RaptorDesktop):

    def setup_chrome_args(self, test):
        # Setup chrome args and add them to the runner's args
        chrome_args = self.desktop_chrome_args(test)
        if ' '.join(chrome_args) not in ' '.join(self.runner.cmdargs):
            self.runner.cmdargs.extend(chrome_args)

    def launch_desktop_browser(self, test):
        LOG.info("starting %s" % self.config['app'])

        # Setup chrome/chromium specific arguments then start the runner
        self.setup_chrome_args(test)
        self.start_runner_proc()

    def set_browser_test_prefs(self, raw_prefs):
        # add test-specific preferences
        LOG.info("preferences were configured for the test, however \
                        we currently do not install them on non-Firefox browsers.")


class RaptorAndroid(PerftestAndroid, Raptor):

    def __init__(self, app, binary, activity=None, intent=None, **kwargs):
        super(RaptorAndroid, self).__init__(app, binary, profile_class="firefox", **kwargs)

        self.config.update({
            'activity': activity,
            'intent': intent,
        })

        self.remote_test_root = os.path.abspath(os.path.join(os.sep, 'sdcard', 'raptor'))
        self.remote_profile = os.path.join(self.remote_test_root, "profile")
        self.os_baseline_data = None
        self.power_test_time = None
        self.screen_off_timeout = 0
        self.screen_brightness = 127
        self.app_launched = False

    def setup_adb_device(self):
        if self.device is None:
            self.device = ADBDevice(verbose=True)
            tune_performance(self.device, log=LOG)

        LOG.info("creating remote root folder for raptor: %s" % self.remote_test_root)
        self.device.rm(self.remote_test_root, force=True, recursive=True)
        self.device.mkdir(self.remote_test_root)
        self.device.chmod(self.remote_test_root, recursive=True, root=True)

        self.clear_app_data()
        self.set_debug_app_flag()

    def write_android_app_config(self):
        # geckoview supports having a local on-device config file; use this file
        # to tell the app to use the specified browser profile, as well as other opts
        # on-device: /data/local/tmp/com.yourcompany.yourapp-geckoview-config.yaml
        # https://mozilla.github.io/geckoview/tutorials/automation.html#configuration-file-format

        # only supported for geckoview apps
        if self.config['app'] == "fennec":
            return
        LOG.info("creating android app config.yml")

        yml_config_data = dict(
            args=['--profile', self.remote_profile, 'use_multiprocess', self.config['e10s']],
            env=dict(
                LOG_VERBOSE=1,
                R_LOG_LEVEL=6,
                MOZ_WEBRENDER=int(self.config['enable_webrender']),
            )
        )

        yml_name = '%s-geckoview-config.yaml' % self.config['binary']
        yml_on_host = os.path.join(tempfile.mkdtemp(), yml_name)
        write_yml_file(yml_on_host, yml_config_data)
        yml_on_device = os.path.join('/data', 'local', 'tmp', yml_name)

        try:
            LOG.info("copying %s to device: %s" % (yml_on_host, yml_on_device))
            self.device.rm(yml_on_device, force=True, recursive=True)
            self.device.push(yml_on_host, yml_on_device)

        except Exception:
            LOG.critical("failed to push %s to device!" % yml_on_device)
            raise

    def log_android_device_temperature(self):
        try:
            # retrieve and log the android device temperature
            thermal_zone0 = self.device.shell_output('cat sys/class/thermal/thermal_zone0/temp')
            thermal_zone0 = float(thermal_zone0)
            zone_type = self.device.shell_output('cat sys/class/thermal/thermal_zone0/type')
            LOG.info("(thermal_zone0) device temperature: %.3f zone type: %s"
                     % (thermal_zone0 / 1000, zone_type))
        except Exception as exc:
            LOG.warning("Unexpected error: {} - {}"
                        .format(exc.__class__.__name__, exc))

    def launch_firefox_android_app(self, test_name):
        LOG.info("starting %s" % self.config['app'])

        extra_args = ["-profile", self.remote_profile,
                      "--es", "env0", "LOG_VERBOSE=1",
                      "--es", "env1", "R_LOG_LEVEL=6",
                      "--es", "env2", "MOZ_WEBRENDER=%d" % self.config['enable_webrender']]

        try:
            # make sure the android app is not already running
            self.device.stop_application(self.config['binary'])

            if self.config['app'] == "fennec":
                self.device.launch_fennec(self.config['binary'],
                                          extra_args=extra_args,
                                          url='about:blank',
                                          fail_if_running=False)
            else:

                # command line 'extra' args not used with geckoview apps; instead we use
                # an on-device config.yml file (see write_android_app_config)

                self.device.launch_application(self.config['binary'],
                                               self.config['activity'],
                                               self.config['intent'],
                                               extras=None,
                                               url='about:blank',
                                               fail_if_running=False)

            # Check if app has started and it's running
            if not self.device.process_exist(self.config['binary']):
                raise Exception("Error launching %s. App did not start properly!" %
                                self.config['binary'])
            self.app_launched = True
        except Exception as e:
            LOG.error("Exception launching %s" % self.config['binary'])
            LOG.error("Exception: %s %s" % (type(e).__name__, str(e)))
            if self.config['power_test']:
                finish_android_power_test(self, test_name)
            raise

        # give our control server the device and app info
        self.control_server.device = self.device
        self.control_server.app_name = self.config['binary']

    def copy_cert_db(self, source_dir, target_dir):
        # copy browser cert db (that was previously created via certutil) from source to target
        cert_db_files = ['pkcs11.txt', 'key4.db', 'cert9.db']
        for next_file in cert_db_files:
            _source = os.path.join(source_dir, next_file)
            _dest = os.path.join(target_dir, next_file)
            if os.path.exists(_source):
                LOG.info("copying %s to %s" % (_source, _dest))
                shutil.copyfile(_source, _dest)
            else:
                LOG.critical("unable to find ssl cert db file: %s" % _source)

    def run_tests(self, tests, test_names):
        self.setup_adb_device()

        return super(RaptorAndroid, self).run_tests(tests, test_names)

    def run_test_setup(self, test):
        super(RaptorAndroid, self).run_test_setup(test)
        self.set_reverse_ports()

    def run_test_teardown(self, test):
        LOG.info('removing reverse socket connections')
        self.device.remove_socket_connections('reverse')

        super(RaptorAndroid, self).run_test_teardown(test)

    def run_test(self, test, timeout):
        # tests will be run warm (i.e. NO browser restart between page-cycles)
        # unless otheriwse specified in the test INI by using 'cold = true'
        try:

            if self.config['power_test']:
                # gather OS baseline data
                init_android_power_test(self)
                LOG.info("Running OS baseline, pausing for 1 minute...")
                time.sleep(60)
                finish_android_power_test(self, 'os-baseline', os_baseline=True)

                # initialize for the test
                init_android_power_test(self)

            if test.get('cold', False) is True:
                self.__run_test_cold(test, timeout)
            else:
                self.__run_test_warm(test, timeout)

        except SignalHandlerException:
            self.device.stop_application(self.config['binary'])

        finally:
            if self.config['power_test']:
                finish_android_power_test(self, test['name'])

    def __run_test_cold(self, test, timeout):
        '''
        Run the Raptor test but restart the entire browser app between page-cycles.

        Note: For page-load tests, playback will only be started once - at the beginning of all
        browser cycles, and then stopped after all cycles are finished. The proxy is set via prefs
        in the browser profile so those will need to be set again in each new profile/cycle.
        Note that instead of using the certutil tool each time to create a db and import the
        mitmproxy SSL cert (it's done in mozbase/mozproxy) we will simply copy the existing
        cert db from the first cycle's browser profile into the new clean profile; this way
        we don't have to re-create the cert db on each browser cycle.

        Since we're running in cold-mode, before this point (in manifest.py) the
        'expected-browser-cycles' value was already set to the initial 'page-cycles' value;
        and the 'page-cycles' value was set to 1 as we want to perform one page-cycle per
        browser restart.

        The 'browser-cycle' value is the current overall browser start iteration. The control
        server will receive the current 'browser-cycle' and the 'expected-browser-cycles' in
        each results set received; and will pass that on as part of the results so that the
        results processing will know results for multiple browser cycles are being received.

        The default will be to run in warm mode; unless 'cold = true' is set in the test INI.
        '''
        LOG.info("test %s is running in cold mode; browser WILL be restarted between "
                 "page cycles" % test['name'])

        for test['browser_cycle'] in range(1, test['expected_browser_cycles'] + 1):

            LOG.info("begin browser cycle %d of %d for test %s"
                     % (test['browser_cycle'], test['expected_browser_cycles'], test['name']))

            self.run_test_setup(test)

            self.clear_app_data()
            self.set_debug_app_flag()

            if test['browser_cycle'] == 1:
                if test.get('playback') is not None:

                    # an ssl cert db has now been created in the profile; copy it out so we
                    # can use the same cert db in future test cycles / browser restarts
                    local_cert_db_dir = tempfile.mkdtemp()
                    LOG.info("backing up browser ssl cert db that was created via certutil")
                    self.copy_cert_db(self.config['local_profile_dir'], local_cert_db_dir)

                if not self.is_localhost:
                    self.delete_proxy_settings_from_profile()

            else:
                # double-check to ensure app has been shutdown
                self.device.stop_application(self.config['binary'])

                # initial browser profile was already created before run_test was called;
                # now additional browser cycles we want to create a new one each time
                self.build_browser_profile()

                if test.get('playback') is not None:
                    # get cert db from previous cycle profile and copy into new clean profile
                    # this saves us from having to start playback again / recreate cert db etc.
                    LOG.info("copying existing ssl cert db into new browser profile")
                    self.copy_cert_db(local_cert_db_dir, self.config['local_profile_dir'])

                self.run_test_setup(test)

            if test.get('playback') is not None:
                self.turn_on_android_app_proxy()

            self.copy_profile_to_device()
            self.log_android_device_temperature()

            # write android app config.yml
            self.write_android_app_config()

            # now start the browser/app under test
            self.launch_firefox_android_app(test['name'])

            # set our control server flag to indicate we are running the browser/app
            self.control_server._finished = False

            if self.config['cpu_test']:
                # start measuring CPU usage
                self.cpu_profiler = start_android_cpu_profiler(self)

            self.wait_for_test_finish(test, timeout)

            # in debug mode, and running locally, leave the browser running
            if self.debug_mode and self.config['run_local']:
                LOG.info("* debug-mode enabled - please shutdown the browser manually...")
                self.runner.wait(timeout=None)

            # break test execution if a exception is present
            if len(self.results_handler.page_timeout_list) > 0:
                break

    def __run_test_warm(self, test, timeout):
        LOG.info("test %s is running in warm mode; browser will NOT be restarted between "
                 "page cycles" % test['name'])

        self.run_test_setup(test)

        if not self.is_localhost:
            self.delete_proxy_settings_from_profile()

        if test.get('playback') is not None:
            self.turn_on_android_app_proxy()

        self.clear_app_data()
        self.set_debug_app_flag()
        self.copy_profile_to_device()
        self.log_android_device_temperature()

        # write android app config.yml
        self.write_android_app_config()

        # now start the browser/app under test
        self.launch_firefox_android_app(test['name'])

        # set our control server flag to indicate we are running the browser/app
        self.control_server._finished = False

        if self.config['cpu_test']:
            # start measuring CPU usage
            self.cpu_profiler = start_android_cpu_profiler(self)

        self.wait_for_test_finish(test, timeout)

        # in debug mode, and running locally, leave the browser running
        if self.debug_mode and self.config['run_local']:
            LOG.info("* debug-mode enabled - please shutdown the browser manually...")

    def check_for_crashes(self):
        super(RaptorAndroid, self).check_for_crashes()

        if not self.app_launched:
            LOG.info("skipping check_for_crashes: application has not been launched")
            return
        self.app_launched = False

        # Turn off verbose to prevent logcat from being inserted into the main log.
        verbose = self.device._verbose
        self.device._verbose = False
        logcat = self.device.get_logcat()
        self.device._verbose = verbose
        if logcat:
            if mozcrash.check_for_java_exception(logcat, "raptor"):
                return
        try:
            dump_dir = tempfile.mkdtemp()
            remote_dir = posixpath.join(self.remote_profile, 'minidumps')
            if not self.device.is_dir(remote_dir):
                return
            self.device.pull(remote_dir, dump_dir)
            mozcrash.log_crashes(LOG, dump_dir, self.config['symbols_path'])
        finally:
            try:
                shutil.rmtree(dump_dir)
            except Exception:
                LOG.warning("unable to remove directory: %s" % dump_dir)

    def clean_up(self):
        LOG.info("removing test folder for raptor: %s" % self.remote_test_root)
        self.device.rm(self.remote_test_root, force=True, recursive=True)

        super(RaptorAndroid, self).clean_up()


def main(args=sys.argv[1:]):
    args = parse_args()

    args.extra_prefs = parse_preferences(args.extra_prefs or [])

    if args.enable_fission:
        args.extra_prefs.update({
            'fission.autostart': True,
            'dom.serviceWorkers.parent_intercept': True,
            'browser.tabs.documentchannel': True,
        })

    if args.extra_prefs and args.extra_prefs.get('fission.autostart', False):
        args.enable_fission = True

    commandline.setup_logging('raptor', args, {'tbpl': sys.stdout})

    LOG.info("raptor-start")

    if args.debug_mode:
        LOG.info("debug-mode enabled")

    LOG.info("received command line arguments: %s" % str(args))

    # if a test name specified on command line, and it exists, just run that one
    # otherwise run all available raptor tests that are found for this browser
    raptor_test_list = get_raptor_test_list(args, mozinfo.os)
    raptor_test_names = [raptor_test['name'] for raptor_test in raptor_test_list]

    # ensure we have at least one valid test to run
    if len(raptor_test_list) == 0:
        LOG.critical("this test is not targeted for {}".format(args.app))
        sys.exit(1)

    LOG.info("raptor tests scheduled to run:")
    for next_test in raptor_test_list:
        LOG.info(next_test['name'])

    if not args.browsertime:
        if args.app == "firefox":
            raptor_class = RaptorDesktopFirefox
        elif args.app in CHROMIUM_DISTROS:
            raptor_class = RaptorDesktopChrome
        else:
            raptor_class = RaptorAndroid
    else:
        def raptor_class(*inner_args, **inner_kwargs):
            outer_kwargs = vars(args)
            # peel off arguments that are specific to browsertime
            for key in outer_kwargs.keys():
                if key.startswith('browsertime_'):
                    value = outer_kwargs.pop(key)
                    inner_kwargs[key] = value

            if args.app == "firefox" or args.app in CHROMIUM_DISTROS:
                klass = BrowsertimeDesktop
            else:
                klass = BrowsertimeAndroid

            return klass(*inner_args, **inner_kwargs)

    raptor = raptor_class(args.app,
                          args.binary,
                          run_local=args.run_local,
                          noinstall=args.noinstall,
                          installerpath=args.installerpath,
                          obj_path=args.obj_path,
                          gecko_profile=args.gecko_profile,
                          gecko_profile_interval=args.gecko_profile_interval,
                          gecko_profile_entries=args.gecko_profile_entries,
                          symbols_path=args.symbols_path,
                          host=args.host,
                          power_test=args.power_test,
                          cpu_test=args.cpu_test,
                          memory_test=args.memory_test,
                          is_release_build=args.is_release_build,
                          debug_mode=args.debug_mode,
                          post_startup_delay=args.post_startup_delay,
                          activity=args.activity,
                          intent=args.intent,
                          interrupt_handler=SignalHandler(),
                          enable_webrender=args.enable_webrender,
                          extra_prefs=args.extra_prefs or {},
                          device_name=args.device_name,
                          no_conditioned_profile=args.no_conditioned_profile
                          )

    success = raptor.run_tests(raptor_test_list, raptor_test_names)

    if not success:
        # didn't get test results; test timed out or crashed, etc. we want job to fail
        LOG.critical("TEST-UNEXPECTED-FAIL: no raptor test results were found for %s" %
                     ', '.join(raptor_test_names))
        os.sys.exit(1)

    # if we have results but one test page timed out (i.e. one tp6 test page didn't load
    # but others did) we still dumped PERFHERDER_DATA for the successfull pages but we
    # want the overall test job to marked as a failure
    pages_that_timed_out = raptor.get_page_timeout_list()
    if len(pages_that_timed_out) > 0:
        for _page in pages_that_timed_out:
            message = [("TEST-UNEXPECTED-FAIL", "test '%s'" % _page['test_name']),
                       ("timed out loading test page", _page['url'])]
            if _page.get('pending_metrics') is not None:
                message.append(("pending metrics", _page['pending_metrics']))

            LOG.critical(" ".join("%s: %s" % (subject, msg) for subject, msg in message))
        os.sys.exit(1)

    # if we're running browsertime in the CI, we want to zip the result dir
    if args.browsertime and not args.run_local:
        result_dir = raptor.results_handler.result_dir()
        if os.path.exists(result_dir):
            LOG.info("Creating tarball at %s" % result_dir + ".tgz")
            with tarfile.open(result_dir + ".tgz", "w:gz") as tar:
                tar.add(result_dir, arcname=os.path.basename(result_dir))
            LOG.info("Removing %s" % result_dir)
            shutil.rmtree(result_dir)

    # when running raptor locally with gecko profiling on, use the view-gecko-profile
    # tool to automatically load the latest gecko profile in profiler.firefox.com
    if args.gecko_profile and args.run_local:
        if os.environ.get('DISABLE_PROFILE_LAUNCH', '0') == '1':
            LOG.info("Not launching profiler.firefox.com because DISABLE_PROFILE_LAUNCH=1")
        else:
            view_gecko_profile(args.binary)


if __name__ == "__main__":
    main()
