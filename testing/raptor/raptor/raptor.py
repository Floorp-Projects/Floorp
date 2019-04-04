#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import json
import os
import posixpath
import shutil
import sys
import tempfile
import time

import mozcrash
import mozinfo
from mozdevice import ADBDevice
from mozlog import commandline, get_default_logger
from mozprofile import create_profile
from mozrunner import runners

# need this so raptor imports work both from /raptor and via mach
here = os.path.abspath(os.path.dirname(__file__))
paths = [here]
if os.environ.get('SCRIPTSPATH', None) is not None:
    # in production it is env SCRIPTS_PATH
    paths.append(os.environ['SCRIPTSPATH'])
else:
    # locally it's in source tree
    paths.append(os.path.join(here, '..', '..', 'mozharness'))

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
from cmdline import parse_args
from control_server import RaptorControlServer
from gen_test_config import gen_test_config
from outputhandler import OutputHandler
from manifest import get_raptor_test_list
from mozproxy import get_playback
from results import RaptorResultsHandler
from gecko_profile import GeckoProfile
from power import init_geckoview_power_test, finish_geckoview_power_test
from utils import view_gecko_profile


class Raptor(object):
    """Container class for Raptor"""

    def __init__(self, app, binary, run_local=False, obj_path=None,
                 gecko_profile=False, gecko_profile_interval=None, gecko_profile_entries=None,
                 symbols_path=None, host=None, power_test=False, is_release_build=False,
                 debug_mode=False, activity=None):

        # Override the magic --host HOST_IP with the value of the environment variable.
        if host == 'HOST_IP':
            host = os.environ['HOST_IP']
        self.config = {}
        self.config['app'] = app
        self.config['binary'] = binary
        self.config['platform'] = mozinfo.os
        self.config['processor'] = mozinfo.processor
        self.config['run_local'] = run_local
        self.config['obj_path'] = obj_path
        self.config['gecko_profile'] = gecko_profile
        self.config['gecko_profile_interval'] = gecko_profile_interval
        self.config['gecko_profile_entries'] = gecko_profile_entries
        self.config['symbols_path'] = symbols_path
        self.config['host'] = host
        self.config['power_test'] = power_test
        self.config['is_release_build'] = is_release_build
        self.raptor_venv = os.path.join(os.getcwd(), 'raptor-venv')
        self.log = get_default_logger(component='raptor-main')
        self.control_server = None
        self.playback = None
        self.benchmark = None
        self.benchmark_port = 0
        self.gecko_profiler = None
        self.post_startup_delay = 30000
        self.device = None
        self.profile_class = app
        self.firefox_android_apps = ['fennec', 'geckoview', 'refbrow', 'fenix']

        # debug mode is currently only supported when running locally
        self.debug_mode = debug_mode if self.config['run_local'] else False

        # if running debug-mode reduce the pause after browser startup
        if self.debug_mode:
            self.post_startup_delay = 3000
            self.log.info("debug-mode enabled, reducing post-browser startup pause to %d ms"
                          % self.post_startup_delay)

        self.log.info("main raptor init, config is: %s" % str(self.config))

        # create results holder
        self.results_handler = RaptorResultsHandler()

    @property
    def profile_data_dir(self):
        if 'MOZ_DEVELOPER_REPO_DIR' in os.environ:
            return os.path.join(os.environ['MOZ_DEVELOPER_REPO_DIR'], 'testing', 'profiles')
        if build:
            return os.path.join(build.topsrcdir, 'testing', 'profiles')
        return os.path.join(here, 'profile_data')

    def run_test_setup(self, test):
        self.log.info("starting raptor test: %s" % test['name'])
        self.log.info("test settings: %s" % str(test))
        self.log.info("raptor config: %s" % str(self.config))

        if test.get('type') == "benchmark":
            self.serve_benchmark_source(test)

        gen_test_config(self.config['app'],
                        test['name'],
                        self.control_server.port,
                        self.post_startup_delay,
                        host=self.config['host'],
                        b_port=self.benchmark_port,
                        debug_mode=1 if self.debug_mode else 0)

        self.install_raptor_webext()

        if test.get("preferences", None) is not None:
            self.set_browser_test_prefs(test['preferences'])

        # if 'alert_on' was provided in the test INI, add to our config for results/output
        self.config['subtest_alert_on'] = test.get('alert_on', None)

    def set_browser_test_prefs(self, raw_prefs):
        # add test specific preferences
        if self.config['app'] == "firefox":
            self.log.info("setting test-specific browser preferences")
            self.profile.set_preferences(json.loads(raw_prefs))
        else:
            self.log.info("preferences were configured for the test, however \
                          we currently do not install them on non Firefox browsers.")

    def run_test_teardown(self):
        self.check_for_crashes()

        if self.playback is not None:
            self.playback.stop()

        self.remove_raptor_webext()

        # gecko profiling symbolication
        if self.config['gecko_profile'] is True:
            self.gecko_profiler.symbolicate()
            # clean up the temp gecko profiling folders
            self.log.info("cleaning up after gecko profiling")
            self.gecko_profiler.clean()

    def create_browser_profile(self):
        self.profile = create_profile(self.profile_class)

        # Merge in base profiles
        with open(os.path.join(self.profile_data_dir, 'profiles.json'), 'r') as fh:
            base_profiles = json.load(fh)['raptor']

        for name in base_profiles:
            path = os.path.join(self.profile_data_dir, name)
            self.log.info("Merging profile: {}".format(path))
            self.profile.merge(path)

        # add profile dir to our config
        self.config['local_profile_dir'] = self.profile.profile

    def start_control_server(self):
        self.control_server = RaptorControlServer(self.results_handler, self.debug_mode)
        self.control_server.start()

        # for android we must make the control server available to the device
        if self.config['app'] in self.firefox_android_apps and \
                self.config['host'] in ('localhost', '127.0.0.1'):
            self.log.info("making the raptor control server port available to device")
            _tcp_port = "tcp:%s" % self.control_server.port
            self.device.create_socket_connection('reverse', _tcp_port, _tcp_port)

    def get_playback_config(self, test):
        self.config['playback_tool'] = test.get('playback')
        self.log.info("test uses playback tool: %s " % self.config['playback_tool'])
        platform = self.config['platform']
        self.config['playback_binary_zip'] = test.get('playback_binary_zip_%s' % platform)
        self.config['playback_pageset_zip'] = test.get('playback_pageset_zip_%s' % platform)
        playback_dir = os.path.join(here, 'playback')
        self.config['playback_binary_manifest'] = test.get('playback_binary_manifest')
        self.config['playback_pageset_manifest'] = test.get('playback_pageset_manifest')
        for key in ('playback_pageset_manifest', 'playback_pageset_zip'):
            if self.config.get(key) is None:
                continue
            self.config[key] = os.path.join(playback_dir, self.config[key])

    def serve_benchmark_source(self, test):
        # benchmark-type tests require the benchmark test to be served out
        self.benchmark = Benchmark(self.config, test)
        self.benchmark_port = int(self.benchmark.port)

        # for android we must make the benchmarks server available to the device
        if self.config['app'] in self.firefox_android_apps and \
                self.config['host'] in ('localhost', '127.0.0.1'):
            self.log.info("making the raptor benchmarks server port available to device")
            _tcp_port = "tcp:%s" % self.benchmark_port
            self.device.create_socket_connection('reverse', _tcp_port, _tcp_port)

    def install_raptor_webext(self):
        # must intall raptor addon each time because we dynamically update some content
        # the webext is installed into the browser profile
        # note: for chrome the addon is just a list of paths that ultimately are added
        # to the chromium command line '--load-extension' argument
        self.raptor_webext = os.path.join(webext_dir, 'raptor')
        self.log.info("installing webext %s" % self.raptor_webext)
        self.profile.addons.install(self.raptor_webext)

        # on firefox we can get an addon id; chrome addon actually is just cmd line arg
        try:
            self.webext_id = self.profile.addons.addon_details(self.raptor_webext)['id']
        except AttributeError:
            self.webext_id = None

    def remove_raptor_webext(self):
        # remove the raptor webext; as it must be reloaded with each subtest anyway
        self.log.info("removing webext %s" % self.raptor_webext)
        if self.config['app'] in ['firefox', 'geckoview', 'fennec', 'refbrow', 'fenix']:
            self.profile.addons.remove_addon(self.webext_id)

        # for chrome the addon is just a list (appended to cmd line)
        if self.config['app'] in ["chrome", "chrome-android"]:
            self.profile.addons.remove(self.raptor_webext)

    def set_test_browser_prefs(self, test_prefs):
        # if the test has any specific browser prefs specified, set those in browser profiile
        if self.config['app'] == "firefox":
            self.profile.set_preferences(json.loads(test_prefs))
        else:
            self.log.info("preferences were configured for the test, \
                          but we do not install them on non Firefox browsers.")

    def start_playback(self, test):
        # creating the playback tool
        self.get_playback_config(test)
        self.playback = get_playback(self.config, self.device)

        # finish its configuration
        script = os.path.join(here, "playback", "alternate-server-replay.py")
        recordings = test.get("playback_recordings")
        if recordings:
            script_args = []
            proxy_dir = self.playback.mozproxy_dir
            for recording in recordings.split():
                if not recording:
                    continue
                script_args.append(os.path.join(proxy_dir, recording))
            script = '""%s %s""' % (script, " ".join(script_args))

        # this part is platform-specific
        if mozinfo.os == "win":
            script = script.replace("\\", "\\\\\\")

        self.playback.config['playback_tool_args'] = ["-s", script]

        # let's start it!
        self.playback.start()

        # for android we must make the playback server available to the device
        if self.config['app'] in self.firefox_android_apps and self.config['host'] \
                in ('localhost', '127.0.0.1'):
            self.log.info("making the raptor playback server port available to device")
            _tcp_port = "tcp:8080"
            self.device.create_socket_connection('reverse', _tcp_port, _tcp_port)

    def delete_proxy_settings_from_profile(self):
        # Must delete the proxy settings from the profile if running
        # the test with a host different from localhost.
        userjspath = os.path.join(self.profile.profile, 'user.js')
        with open(userjspath) as userjsfile:
            prefs = userjsfile.readlines()
        prefs = [pref for pref in prefs if 'network.proxy' not in pref]
        with open(userjspath, 'w') as userjsfile:
            userjsfile.writelines(prefs)

    def _init_gecko_profiling(self, test):
        self.log.info("initializing gecko profiler")
        upload_dir = os.getenv('MOZ_UPLOAD_DIR')
        if not upload_dir:
            self.log.critical("Profiling ignored because MOZ_UPLOAD_DIR was not set")
        else:
            self.gecko_profiler = GeckoProfile(upload_dir,
                                               self.config,
                                               test)

    def wait_for_test_finish(self, test, timeout):
        # convert timeout to seconds and account for page cycles
        timeout = int(timeout / 1000) * int(test['page_cycles'])
        # account for the pause the raptor webext runner takes after browser startup
        timeout += (int(self.post_startup_delay / 1000) + 3)

        # if geckoProfile enabled, give browser more time for profiling
        if self.config['gecko_profile'] is True:
            timeout += 5 * 60

        elapsed_time = 0
        while not self.control_server._finished:
            time.sleep(1)
            # we only want to force browser-shutdown on timeout if not in debug mode;
            # in debug-mode we leave the browser running (require manual shutdown)
            if not self.debug_mode:
                elapsed_time += 1
                if elapsed_time > (timeout) - 5:  # stop 5 seconds early
                    self.log.info("application timed out after {} seconds".format(timeout))
                    self.control_server.wait_for_quit()
                    break

    def process_results(self, test_names):
        # when running locally output results in build/raptor.json; when running
        # in production output to a local.json to be turned into tc job artifact
        if self.config.get('run_local', False):
            if 'MOZ_DEVELOPER_REPO_DIR' in os.environ:
                raptor_json_path = os.path.join(os.environ['MOZ_DEVELOPER_REPO_DIR'],
                                                'testing', 'mozharness', 'build', 'raptor.json')
            else:
                raptor_json_path = os.path.join(here, 'raptor.json')
        else:
            raptor_json_path = os.path.join(os.getcwd(), 'local.json')

        self.config['raptor_json_path'] = raptor_json_path
        return self.results_handler.summarize_and_output(self.config, test_names)

    def get_page_timeout_list(self):
        return self.results_handler.page_timeout_list

    def clean_up(self):
        self.control_server.stop()
        if self.config['app'] not in self.firefox_android_apps:
            self.runner.stop()
        elif self.config['app'] in self.firefox_android_apps:
            self.log.info('removing reverse socket connections')
            self.device.remove_socket_connections('reverse')
        else:
            pass
        self.log.info("finished")


class RaptorDesktop(Raptor):
    def __init__(self, app, binary, run_local=False, obj_path=None,
                 gecko_profile=False, gecko_profile_interval=None, gecko_profile_entries=None,
                 symbols_path=None, host=None, power_test=False, is_release_build=False,
                 debug_mode=False, activity=None):
        Raptor.__init__(self, app, binary, run_local, obj_path, gecko_profile,
                        gecko_profile_interval, gecko_profile_entries, symbols_path,
                        host, power_test, is_release_build, debug_mode)

    def create_browser_handler(self):
        # create the desktop browser runner
        self.log.info("creating browser runner using mozrunner")
        self.output_handler = OutputHandler()
        process_args = {
            'processOutputLine': [self.output_handler],
        }
        runner_cls = runners[self.config['app']]
        self.runner = runner_cls(
            self.config['binary'], profile=self.profile, process_args=process_args,
            symbols_path=self.config['symbols_path'])

    def start_runner_proc(self):
        # launch the browser via our previously-created runner
        self.runner.start()
        proc = self.runner.process_handler
        self.output_handler.proc = proc

        # give our control server the browser process so it can shut it down later
        self.control_server.browser_proc = proc

    def run_test(self, test, timeout=None):
        self.run_test_setup(test)

        if test.get('playback', None) is not None:
            self.start_playback(test)

        if self.config['host'] not in ('localhost', '127.0.0.1'):
            self.delete_proxy_settings_from_profile()

        # now start the browser/app under test
        self.launch_desktop_browser(test)

        # set our control server flag to indicate we are running the browser/app
        self.control_server._finished = False

        self.wait_for_test_finish(test, timeout)

        self.run_test_teardown()

        # browser should be closed by now but this is a backup-shutdown (if not in debug-mode)
        if not self.debug_mode:
            if self.runner.is_running():
                self.runner.stop()
        else:
            # in debug mode, and running locally, leave the browser running
            if self.config['run_local']:
                self.log.info("* debug-mode enabled - please shutdown the browser manually...")
                self.runner.wait(timeout=None)

    def check_for_crashes(self):
        try:
            self.runner.check_for_crashes()
        except NotImplementedError:  # not implemented for Chrome
            pass


class RaptorDesktopFirefox(RaptorDesktop):
    def __init__(self, app, binary, run_local=False, obj_path=None,
                 gecko_profile=False, gecko_profile_interval=None, gecko_profile_entries=None,
                 symbols_path=None, host=None, power_test=False, is_release_build=False,
                 debug_mode=False, activity=None):
        RaptorDesktop.__init__(self, app, binary, run_local, obj_path, gecko_profile,
                               gecko_profile_interval, gecko_profile_entries, symbols_path,
                               host, power_test, is_release_build, debug_mode)

    def disable_non_local_connections(self):
        # For Firefox we need to set MOZ_DISABLE_NONLOCAL_CONNECTIONS=1 env var before startup
        # when testing release builds from mozilla-beta/release. This is because of restrictions
        # on release builds that require webextensions to be signed unless this env var is set
        self.log.info("setting MOZ_DISABLE_NONLOCAL_CONNECTIONS=1")
        os.environ['MOZ_DISABLE_NONLOCAL_CONNECTIONS'] = "1"

    def enable_non_local_connections(self):
        # pageload tests need to be able to access non-local connections via mitmproxy
        self.log.info("setting MOZ_DISABLE_NONLOCAL_CONNECTIONS=0")
        os.environ['MOZ_DISABLE_NONLOCAL_CONNECTIONS'] = "0"

    def launch_desktop_browser(self, test):
        self.log.info("starting %s" % self.config['app'])
        if self.config['is_release_build']:
            self.disable_non_local_connections()

        # if running debug-mode, tell Firefox to open the browser console on startup
        if self.debug_mode:
            self.runner.cmdargs.extend(['-jsconsole'])

        self.start_runner_proc()

        if self.config['is_release_build'] and test.get('playback', None) is not None:
            self.enable_non_local_connections()

        # if geckoProfile is enabled, initialize it
        if self.config['gecko_profile'] is True:
            self._init_gecko_profiling(test)
            # tell the control server the gecko_profile dir; the control server will
            # receive the actual gecko profiles from the web ext and will write them
            # to disk; then profiles are picked up by gecko_profile.symbolicate
            self.control_server.gecko_profile_dir = self.gecko_profiler.gecko_profile_dir


class RaptorDesktopChrome(RaptorDesktop):
    def __init__(self, app, binary, run_local=False, obj_path=None,
                 gecko_profile=False, gecko_profile_interval=None, gecko_profile_entries=None,
                 symbols_path=None, host=None, power_test=False, is_release_build=False,
                 debug_mode=False, activity=None):
        RaptorDesktop.__init__(self, app, binary, run_local, obj_path, gecko_profile,
                               gecko_profile_interval, gecko_profile_entries, symbols_path,
                               host, power_test, is_release_build, debug_mode)

    def setup_chrome_desktop_for_playback(self):
        # if running a pageload test on google chrome, add the cmd line options
        # to turn on the proxy and ignore security certificate errors
        # if using host localhost, 127.0.0.1.
        chrome_args = [
            '--proxy-server=127.0.0.1:8080',
            '--proxy-bypass-list=localhost;127.0.0.1',
            '--ignore-certificate-errors',
        ]
        if self.config['host'] not in ('localhost', '127.0.0.1'):
            chrome_args[0] = chrome_args[0].replace('127.0.0.1', self.config['host'])
        if ' '.join(chrome_args) not in ' '.join(self.runner.cmdargs):
            self.runner.cmdargs.extend(chrome_args)

    def launch_desktop_browser(self, test):
        self.log.info("starting %s" % self.config['app'])
        # some chromium-specfic cmd line opts required
        self.runner.cmdargs.extend(['--use-mock-keychain', '--no-default-browser-check'])

        # if running in debug-mode, open the devtools on the raptor test tab
        if self.debug_mode:
            self.runner.cmdargs.extend(['--auto-open-devtools-for-tabs'])

        if test.get('playback', None) is not None:
            self.setup_chrome_desktop_for_playback()

        self.start_runner_proc()


class RaptorAndroid(Raptor):
    def __init__(self, app, binary, run_local=False, obj_path=None,
                 gecko_profile=False, gecko_profile_interval=None, gecko_profile_entries=None,
                 symbols_path=None, host=None, power_test=False, is_release_build=False,
                 debug_mode=False, activity=None):
        Raptor.__init__(self, app, binary, run_local, obj_path, gecko_profile,
                        gecko_profile_interval, gecko_profile_entries, symbols_path, host,
                        power_test, is_release_build, debug_mode)

        # on android, when creating the browser profile, we want to use a 'firefox' type profile
        self.profile_class = "firefox"
        self.config['activity'] = activity

    def create_browser_handler(self):
        # create the android device handler; it gets initiated and sets up adb etc
        self.log.info("creating android device handler using mozdevice")
        self.device = ADBDevice(verbose=True)
        self.device.clear_logcat()
        self.clear_app_data()

    def clear_app_data(self):
        self.log.info("clearing %s app data" % self.config['binary'])
        self.device.shell("pm clear %s" % self.config['binary'])

    def create_raptor_sdcard_folder(self):
        # for android/geckoview, create a top-level raptor folder on the device
        # sdcard; if it already exists remove it so we start fresh each time
        self.device_raptor_dir = "/sdcard/raptor"
        self.config['device_raptor_dir'] = self.device_raptor_dir
        if self.device.is_dir(self.device_raptor_dir):
            self.log.info("deleting existing device raptor dir: %s" % self.device_raptor_dir)
            self.device.rm(self.device_raptor_dir, recursive=True)
        self.log.info("creating raptor folder on sdcard: %s" % self.device_raptor_dir)
        self.device.mkdir(self.device_raptor_dir)
        self.device.chmod(self.device_raptor_dir, recursive=True)

    def copy_profile_onto_device(self):
        # for geckoview/fennec we must copy the profile onto the device and set perms
        if not self.device.is_app_installed(self.config['binary']):
            raise Exception('%s is not installed' % self.config['binary'])
        self.device_profile = os.path.join(self.device_raptor_dir, "profile")

        if self.device.is_dir(self.device_profile):
            self.log.info("deleting existing device profile folder: %s" % self.device_profile)
            self.device.rm(self.device_profile, recursive=True)
        self.log.info("creating profile folder on device: %s" % self.device_profile)
        self.device.mkdir(self.device_profile)

        self.log.info("copying firefox profile onto the device")
        self.log.info("note: the profile folder being copied is: %s" % self.profile.profile)
        self.log.info('the adb push cmd copies that profile dir to a new temp dir before copy')
        self.device.push(self.profile.profile, self.device_profile)
        self.device.chmod(self.device_profile, recursive=True)

    def turn_on_android_app_proxy(self):
        # for geckoview/android pageload playback we can't use a policy to turn on the
        # proxy; we need to set prefs instead; note that the 'host' may be different
        # than '127.0.0.1' so we must set the prefs accordingly
        self.log.info("setting profile prefs to turn on the android app proxy")
        proxy_prefs = {}
        proxy_prefs["network.proxy.type"] = 1
        proxy_prefs["network.proxy.http"] = self.config['host']
        proxy_prefs["network.proxy.http_port"] = 8080
        proxy_prefs["network.proxy.ssl"] = self.config['host']
        proxy_prefs["network.proxy.ssl_port"] = 8080
        proxy_prefs["network.proxy.no_proxies_on"] = self.config['host']
        self.profile.set_preferences(proxy_prefs)

    def launch_firefox_android_app(self):
        self.log.info("starting %s" % self.config['app'])

        extra_args = ["-profile", self.device_profile,
                      "--es", "env0", "LOG_VERBOSE=1",
                      "--es", "env1", "R_LOG_LEVEL=6"]

        try:
            # make sure the android app is not already running
            self.device.stop_application(self.config['binary'])

            if self.config['app'] == "fennec":
                self.device.launch_fennec(self.config['binary'],
                                          extra_args=extra_args,
                                          url='about:blank',
                                          fail_if_running=False)
            else:
                self.device.launch_activity(self.config['binary'],
                                            self.config['activity'],
                                            extra_args=extra_args,
                                            url='about:blank',
                                            e10s=True,
                                            fail_if_running=False)
        except Exception as e:
            self.log.error("Exception launching %s" % self.config['binary'])
            self.log.error("Exception: %s %s" % (type(e).__name__, str(e)))
            if self.config['power_test']:
                finish_geckoview_power_test(self)
            raise

        # give our control server the device and app info
        self.control_server.device = self.device
        self.control_server.app_name = self.config['binary']

    def run_test(self, test, timeout=None):
        if self.config['power_test']:
            init_geckoview_power_test(self)

        self.run_test_setup(test)
        self.create_raptor_sdcard_folder()

        if test.get('playback', None) is not None:
            self.start_playback(test)

        if self.config['host'] not in ('localhost', '127.0.0.1'):
            self.delete_proxy_settings_from_profile()

        if test.get('playback', None) is not None:
            self.turn_on_android_app_proxy()

        self.copy_profile_onto_device()

        # now start the browser/app under test
        self.launch_firefox_android_app()

        # set our control server flag to indicate we are running the browser/app
        self.control_server._finished = False

        self.wait_for_test_finish(test, timeout)

        if self.config['power_test']:
            finish_geckoview_power_test(self)

        self.run_test_teardown()

        # in debug mode, and running locally, leave the browser running
        if self.debug_mode and self.config['run_local']:
            self.log.info("* debug-mode enabled - please shutdown the browser manually...")
            self.runner.wait(timeout=None)

    def check_for_crashes(self):
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
            remote_dir = posixpath.join(self.device_profile, 'minidumps')
            if not self.device.is_dir(remote_dir):
                self.log.error("No crash directory (%s) found on remote device" % remote_dir)
                return
            self.device.pull(remote_dir, dump_dir)
            mozcrash.log_crashes(self.log, dump_dir, self.config['symbols_path'])
        finally:
            try:
                shutil.rmtree(dump_dir)
            except Exception:
                self.log.warning("unable to remove directory: %s" % dump_dir)


def main(args=sys.argv[1:]):
    args = parse_args()
    commandline.setup_logging('raptor', args, {'tbpl': sys.stdout})
    LOG = get_default_logger(component='raptor-main')

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
        LOG.critical("abort: no tests found")
        sys.exit(1)

    LOG.info("raptor tests scheduled to run:")
    for next_test in raptor_test_list:
        LOG.info(next_test['name'])

    if args.app == "firefox":
        raptor_class = RaptorDesktopFirefox
    elif args.app == "chrome":
        raptor_class = RaptorDesktopChrome
    else:
        raptor_class = RaptorAndroid

    raptor = raptor_class(args.app,
                          args.binary,
                          run_local=args.run_local,
                          obj_path=args.obj_path,
                          gecko_profile=args.gecko_profile,
                          gecko_profile_interval=args.gecko_profile_interval,
                          gecko_profile_entries=args.gecko_profile_entries,
                          symbols_path=args.symbols_path,
                          host=args.host,
                          power_test=args.power_test,
                          is_release_build=args.is_release_build,
                          debug_mode=args.debug_mode,
                          activity=args.activity)

    raptor.create_browser_profile()
    raptor.create_browser_handler()
    raptor.start_control_server()

    for next_test in raptor_test_list:
        raptor.run_test(next_test, timeout=int(next_test['page_timeout']))

    success = raptor.process_results(raptor_test_names)
    raptor.clean_up()

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
            LOG.critical("TEST-UNEXPECTED-FAIL: test '%s' timed out loading test page: %s "
                         "pending metrics: %s"
                         % (_page['test_name'],
                            _page['url'],
                            _page['pending_metrics']))
        os.sys.exit(1)

    # when running raptor locally with gecko profiling on, use the view-gecko-profile
    # tool to automatically load the latest gecko profile in profiler.firefox.com
    if args.gecko_profile and args.run_local:
        if os.environ.get('DISABLE_PROFILE_LAUNCH', '0') == '1':
            LOG.info("Not launching profiler.firefox.com because DISABLE_PROFILE_LAUNCH=1")
        else:
            view_gecko_profile(args.binary)


if __name__ == "__main__":
    main()
