#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import json
import os
import posixpath
import shutil
import signal
import sys
import tempfile
import time

import requests

import mozcrash
import mozinfo
from mozdevice import ADBDevice
from mozlog import commandline, get_default_logger
from mozprofile import create_profile
from mozproxy import get_playback
from mozrunner import runners

# need this so raptor imports work both from /raptor and via mach
here = os.path.abspath(os.path.dirname(__file__))
paths = [here]
if os.environ.get('SCRIPTSPATH') is not None:
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
from cmdline import (parse_args,
                     FIREFOX_ANDROID_APPS,
                     CHROMIUM_DISTROS)
from control_server import RaptorControlServer
from gecko_profile import GeckoProfile
from gen_test_config import gen_test_config
from outputhandler import OutputHandler
from manifest import get_raptor_test_list
from memory import generate_android_memory_profile
from mozproxy import get_playback
from power import init_android_power_test, finish_android_power_test
from results import RaptorResultsHandler
from utils import view_gecko_profile


class SignalHandler:

    def __init__(self):
        signal.signal(signal.SIGINT, self.handle_signal)
        signal.signal(signal.SIGTERM, self.handle_signal)

    def handle_signal(self, signum, frame):
        raise SignalHandlerException("Program aborted due to signal %s" % signum)


class SignalHandlerException(Exception):
    pass


class Raptor(object):
    """Container class for Raptor"""

    def __init__(self, app, binary, run_local=False, obj_path=None, profile_class=None,
                 gecko_profile=False, gecko_profile_interval=None, gecko_profile_entries=None,
                 symbols_path=None, host=None, power_test=False, memory_test=False,
                 is_release_build=False, debug_mode=False, post_startup_delay=None,
                 interrupt_handler=None, e10s=True, **kwargs):

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
            'is_release_build': is_release_build,
            'enable_control_server_wait': memory_test,
            'e10s': e10s,
        }

        self.raptor_venv = os.path.join(os.getcwd(), 'raptor-venv')
        self.log = get_default_logger(component='raptor-main')
        self.control_server = None
        self.playback = None
        self.benchmark = None
        self.benchmark_port = 0
        self.gecko_profiler = None
        self.post_startup_delay = post_startup_delay
        self.device = None
        self.profile_class = profile_class or app
        self.firefox_android_apps = FIREFOX_ANDROID_APPS
        self.interrupt_handler = interrupt_handler

        # debug mode is currently only supported when running locally
        self.debug_mode = debug_mode if self.config['run_local'] else False

        # if running debug-mode reduce the pause after browser startup
        if self.debug_mode:
            self.post_startup_delay = min(self.post_startup_delay, 3000)
            self.log.info("debug-mode enabled, reducing post-browser startup pause to %d ms"
                          % self.post_startup_delay)

        self.log.info("main raptor init, config is: %s" % str(self.config))

        # create results holder
        self.results_handler = RaptorResultsHandler()

        self.build_browser_profile()
        self.start_control_server()

    @property
    def profile_data_dir(self):
        if 'MOZ_DEVELOPER_REPO_DIR' in os.environ:
            return os.path.join(os.environ['MOZ_DEVELOPER_REPO_DIR'], 'testing', 'profiles')
        if build:
            return os.path.join(build.topsrcdir, 'testing', 'profiles')
        return os.path.join(here, 'profile_data')

    def check_for_crashes(self):
        raise NotImplementedError

    def run_test_setup(self, test):
        self.log.info("starting raptor test: %s" % test['name'])
        self.log.info("test settings: %s" % str(test))
        self.log.info("raptor config: %s" % str(self.config))

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

        if test.get("preferences") is not None:
            self.set_browser_test_prefs(test['preferences'])

        # if 'alert_on' was provided in the test INI, add to our config for results/output
        self.config['subtest_alert_on'] = test.get('alert_on')

    def run_tests(self, tests, test_names):
        try:
            for test in tests:
                self.run_test(test, timeout=int(test['page_timeout']))

            return self.process_results(test_names)

        finally:
            self.clean_up()

    def run_test(self, test, timeout=None):
        raise NotImplementedError()

    def wait_for_test_finish(self, test, timeout):
        # convert timeout to seconds and account for page cycles
        timeout = int(timeout / 1000) * int(test.get('page_cycles', 1))
        # account for the pause the raptor webext runner takes after browser startup
        # and the time an exception is propagated through the framework
        timeout += (int(self.post_startup_delay / 1000) + 10)

        # if geckoProfile enabled, give browser more time for profiling
        if self.config['gecko_profile'] is True:
            timeout += 5 * 60

        elapsed_time = 0
        while not self.control_server._finished:
            if self.config['enable_control_server_wait']:
                response = self.control_server_wait_get()
                if response == 'webext_status/__raptor_shutdownBrowser':
                    if self.config['memory_test']:
                        generate_android_memory_profile(self, test['name'])
                    self.control_server_wait_continue()
            time.sleep(1)
            # we only want to force browser-shutdown on timeout if not in debug mode;
            # in debug-mode we leave the browser running (require manual shutdown)
            if not self.debug_mode:
                elapsed_time += 1
                if elapsed_time > (timeout) - 5:  # stop 5 seconds early
                    self.log.info("application timed out after {} seconds".format(timeout))
                    self.control_server.wait_for_quit()
                    break

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

    def set_browser_test_prefs(self, raw_prefs):
        # add test specific preferences
        self.log.info("setting test-specific Firefox preferences")
        self.profile.set_preferences(json.loads(raw_prefs))

    def build_browser_profile(self):
        self.profile = create_profile(self.profile_class)

        # Merge extra profile data from testing/profiles
        with open(os.path.join(self.profile_data_dir, 'profiles.json'), 'r') as fh:
            base_profiles = json.load(fh)['raptor']

        for profile in base_profiles:
            path = os.path.join(self.profile_data_dir, profile)
            self.log.info("Merging profile: {}".format(path))
            self.profile.merge(path)

        # add profile dir to our config
        self.config['local_profile_dir'] = self.profile.profile

    def start_control_server(self):
        self.control_server = RaptorControlServer(self.results_handler, self.debug_mode)
        self.control_server.start()

        if self.config['enable_control_server_wait']:
            self.control_server_wait_set('webext_status/__raptor_shutdownBrowser')

    def get_playback_config(self, test):
        platform = self.config['platform']
        playback_dir = os.path.join(here, 'playback')

        self.config.update({
            'playback_tool': test.get('playback'),
            'playback_version': test.get('playback_version', "2.0.2"),
            'playback_binary_zip': test.get('playback_binary_zip_%s' % platform),
            'playback_pageset_zip': test.get('playback_pageset_zip_%s' % platform),
            'playback_binary_manifest': test.get('playback_binary_manifest'),
            'playback_pageset_manifest': test.get('playback_pageset_manifest'),
        })
        for key in ('playback_pageset_manifest', 'playback_pageset_zip'):
            if self.config.get(key) is None:
                continue
            self.config[key] = os.path.join(playback_dir, self.config[key])

        self.log.info("test uses playback tool: %s " % self.config['playback_tool'])

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
        chrome_apps = CHROMIUM_DISTROS + ["chrome-android", "chromium-android"]
        if self.config['app'] in chrome_apps:
            self.profile.addons.remove(self.raptor_webext)

    def get_proxy_command_for_mitm(self, test, version):
        # Generate Mitmproxy playback args
        script = os.path.join(here, "playback", "alternate-server-replay-{}.py".format(version))
        recordings = test.get("playback_recordings")
        if recordings:
            recording_paths = []
            proxy_dir = self.playback.mozproxy_dir
            for recording in recordings.split():
                if not recording:
                    continue
                recording_paths.append(os.path.join(proxy_dir, recording))

        # this part is platform-specific
        if mozinfo.os == "win":
            script = script.replace("\\", "\\\\\\")
            recording_paths = [recording_path.replace("\\", "\\\\\\")
                               for recording_path in recording_paths]

        if version == "2.0.2":
            self.playback.config['playback_tool_args'] = ["--replay-kill-extra",
                                                          "--script",
                                                          '""{} {}""'.
                                                          format(script,
                                                                 " ".join(recording_paths))]
        elif version == "4.0.4":
            self.playback.config['playback_tool_args'] = ["--scripts", script,
                                                          "--set",
                                                          "server_replay={}".
                                                          format(" ".join(recording_paths))]
        else:
            raise Exception("Mitmproxy version is unknown!")

    def start_playback(self, test):
        # creating the playback tool
        self.get_playback_config(test)
        self.playback = get_playback(self.config, self.device)

        self.get_proxy_command_for_mitm(test, self.config['playback_version'])

        # let's start it!
        self.playback.start()

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
        if self.config['enable_control_server_wait']:
            self.control_server_wait_clear('all')

        self.control_server.stop()
        self.log.info("finished")

    def control_server_wait_set(self, state):
        response = requests.post("http://127.0.0.1:%s/" % self.control_server.port,
                                 json={"type": "wait-set", "data": state})
        return response.content

    def control_server_wait_timeout(self, timeout):
        response = requests.post("http://127.0.0.1:%s/" % self.control_server.port,
                                 json={"type": "wait-timeout", "data": timeout})
        return response.content

    def control_server_wait_get(self):
        response = requests.post("http://127.0.0.1:%s/" % self.control_server.port,
                                 json={"type": "wait-get", "data": ""})
        return response.content

    def control_server_wait_continue(self):
        response = requests.post("http://127.0.0.1:%s/" % self.control_server.port,
                                 json={"type": "wait-continue", "data": ""})
        return response.content

    def control_server_wait_clear(self, state):
        response = requests.post("http://127.0.0.1:%s/" % self.control_server.port,
                                 json={"type": "wait-clear", "data": state})
        return response.content


class RaptorDesktop(Raptor):

    def __init__(self, *args, **kwargs):
        super(RaptorDesktop, self).__init__(*args, **kwargs)

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

    def launch_desktop_browser(self, test):
        raise NotImplementedError

    def start_runner_proc(self):
        # launch the browser via our previously-created runner
        self.runner.start()
        proc = self.runner.process_handler
        self.output_handler.proc = proc

        # give our control server the browser process so it can shut it down later
        self.control_server.browser_proc = proc

    def run_test(self, test, timeout=None):
        # tests will be run warm (i.e. NO browser restart between page-cycles)
        # unless otheriwse specified in the test INI by using 'cold = true'
        if test.get('cold', False) is True:
            self.run_test_cold(test, timeout)
        else:
            self.run_test_warm(test, timeout)

    def run_test_cold(self, test, timeout=None):
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
        self.log.info("test %s is running in cold mode; browser WILL be restarted between "
                      "page cycles" % test['name'])

        for test['browser_cycle'] in range(1, test['expected_browser_cycles'] + 1):

            self.log.info("begin browser cycle %d of %d for test %s"
                          % (test['browser_cycle'], test['expected_browser_cycles'], test['name']))

            self.run_test_setup(test)

            if test['browser_cycle'] == 1:

                if test.get('playback') is not None:
                    self.start_playback(test)

                if self.config['host'] not in ('localhost', '127.0.0.1'):
                    self.delete_proxy_settings_from_profile()

            else:
                # initial browser profile was already created before run_test was called;
                # now additional browser cycles we want to create a new one each time
                self.build_browser_profile()

                self.run_test_setup(test)

            # now start the browser/app under test
            self.launch_desktop_browser(test)

            # set our control server flag to indicate we are running the browser/app
            self.control_server._finished = False

            self.wait_for_test_finish(test, timeout)

        self.run_test_teardown()

    def run_test_warm(self, test, timeout=None):
        self.run_test_setup(test)

        try:
            if test.get('playback') is not None:
                self.start_playback(test)

            if self.config['host'] not in ('localhost', '127.0.0.1'):
                self.delete_proxy_settings_from_profile()

            # start the browser/app under test
            self.launch_desktop_browser(test)

            # set our control server flag to indicate we are running the browser/app
            self.control_server._finished = False

            self.wait_for_test_finish(test, timeout)

        finally:
            self.run_test_teardown()

    def run_test_teardown(self):
        # browser should be closed by now but this is a backup-shutdown (if not in debug-mode)
        if not self.debug_mode:
            if self.runner.is_running():
                self.runner.stop()
        else:
            # in debug mode, and running locally, leave the browser running
            if self.config['run_local']:
                self.log.info("* debug-mode enabled - please shutdown the browser manually...")
                self.runner.wait(timeout=None)

        super(RaptorDesktop, self).run_test_teardown()

    def check_for_crashes(self):
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

        if self.config['is_release_build'] and test.get('playback') is not None:
            self.enable_non_local_connections()

        # if geckoProfile is enabled, initialize it
        if self.config['gecko_profile'] is True:
            self._init_gecko_profiling(test)
            # tell the control server the gecko_profile dir; the control server will
            # receive the actual gecko profiles from the web ext and will write them
            # to disk; then profiles are picked up by gecko_profile.symbolicate
            self.control_server.gecko_profile_dir = self.gecko_profiler.gecko_profile_dir


class RaptorDesktopChrome(RaptorDesktop):

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

        if test.get('playback') is not None:
            self.setup_chrome_desktop_for_playback()

        self.start_runner_proc()

    def set_browser_test_prefs(self, raw_prefs):
        # add test-specific preferences
        self.log.info("preferences were configured for the test, however \
                        we currently do not install them on non-Firefox browsers.")


class RaptorAndroid(Raptor):

    def __init__(self, app, binary, activity=None, intent=None, **kwargs):
        super(RaptorAndroid, self).__init__(app, binary, profile_class="firefox", **kwargs)

        self.config.update({
            'activity': activity,
            'intent': intent,
        })

        self.remote_test_root = os.path.abspath(os.path.join(os.sep, 'sdcard', 'raptor'))
        self.remote_profile = os.path.join(self.remote_test_root, "profile")

    def set_reverse_port(self, port):
        tcp_port = "tcp:{}".format(port)
        self.device.create_socket_connection('reverse', tcp_port, tcp_port)

    def set_reverse_ports(self, is_benchmark=False):
        # Make services running on the host available to the device
        if self.config['host'] in ('localhost', '127.0.0.1'):
            self.log.info("making the raptor control server port available to device")
            self.set_reverse_port(self.control_server.port)

        if self.config['host'] in ('localhost', '127.0.0.1'):
            self.log.info("making the raptor playback server port available to device")
            self.set_reverse_port(8080)

        if is_benchmark and self.config['host'] in ('localhost', '127.0.0.1'):
            self.log.info("making the raptor benchmarks server port available to device")
            self.set_reverse_port(self.benchmark_port)

    def setup_adb_device(self):
        if self.device is None:
            self.device = ADBDevice(verbose=True)
            self.tune_performance()

        self.log.info("creating remote root folder for raptor: %s" % self.remote_test_root)
        self.device.rm(self.remote_test_root, force=True, recursive=True)
        self.device.mkdir(self.remote_test_root)
        self.device.chmod(self.remote_test_root, recursive=True, root=True)

        self.clear_app_data()

    def tune_performance(self):
        """Set various performance-oriented parameters, to reduce jitter.

        For more information, see https://bugzilla.mozilla.org/show_bug.cgi?id=1547135.
        """
        self.log.info("tuning android device performance")
        self.set_svc_power_stayon()
        if (self.device._have_su or self.device._have_android_su):
            self.log.info("executing additional tuning commands requiring root")
            device_name = self.device.shell_output('getprop ro.product.model')
            # all commands require root shell from here on
            self.set_scheduler()
            self.set_virtual_memory_parameters()
            self.turn_off_services()
            self.set_cpu_performance_parameters(device_name)
            self.set_gpu_performance_parameters(device_name)
            self.set_kernel_performance_parameters()
        self.device.clear_logcat()
        self.log.info("android device performance tuning complete")

    def _set_value_and_check_exitcode(self, file_name, value, root=False):
        self.log.info('setting {} to {}'.format(file_name, value))
        process = self.device.shell(' '.join(['echo', str(value), '>', str(file_name)]), root=root)
        if process.exitcode == 0:
            self.log.info('successfully set {} to {}'.format(file_name, value))
        else:
            self.log.warning('command failed with exitcode {}'.format(str(process.exitcode)))

    def set_svc_power_stayon(self):
        self.log.info('set device to stay awake on usb')
        self.device.shell('svc power stayon usb')

    def set_scheduler(self):
        self.log.info('setting scheduler to noop')
        scheduler_location = '/sys/block/sda/queue/scheduler'

        self._set_value_and_check_exitcode(scheduler_location, 'noop')

    def turn_off_services(self):
        services = [
            'mpdecision',
            'thermal-engine',
            'thermald',
        ]
        for service in services:
            self.log.info(' '.join(['turning off service:', service]))
            self.device.shell(' '.join(['stop', service]), root=True)

        services_list_output = self.device.shell_output('service list')
        for service in services:
            if service not in services_list_output:
                self.log.info(' '.join(['successfully terminated:', service]))
            else:
                self.log.warning(' '.join(['failed to terminate:', service]))

    def disable_animations(self):
        self.log.info('disabling animations')
        commands = {
            'animator_duration_scale': 0.0,
            'transition_animation_scale': 0.0,
            'window_animation_scale': 0.0
        }

        for key, value in commands.items():
            command = ' '.join(['settings', 'put', 'global', key, str(value)])
            self.log.info('setting {} to {}'.format(key, value))
            self.device.shell(command)

    def restore_animations(self):
        # animation settings are not restored to default by reboot
        self.log.info('restoring animations')
        commands = {
            'animator_duration_scale': 1.0,
            'transition_animation_scale': 1.0,
            'window_animation_scale': 1.0
        }

        for key, value in commands.items():
            command = ' '.join(['settings', 'put', 'global', key, str(value)])
            self.device.shell(command)

    def set_virtual_memory_parameters(self):
        self.log.info('setting virtual memory parameters')
        commands = {
            '/proc/sys/vm/swappiness': 0,
            '/proc/sys/vm/dirty_ratio': 85,
            '/proc/sys/vm/dirty_background_ratio': 70
        }

        for key, value in commands.items():
            self._set_value_and_check_exitcode(key, value, root=True)

    def set_cpu_performance_parameters(self, device_name):
        self.log.info('setting cpu performance parameters')
        commands = {}

        if device_name == 'Pixel 2':
            # MSM8998 (4x 2.35GHz, 4x 1.9GHz)
            # values obtained from:
            #   /sys/devices/system/cpu/cpufreq/policy0/scaling_available_frequencies
            #   /sys/devices/system/cpu/cpufreq/policy4/scaling_available_frequencies
            commands.update({
                '/sys/devices/system/cpu/cpufreq/policy0/scaling_governor': 'performance',
                '/sys/devices/system/cpu/cpufreq/policy4/scaling_governor': 'performance',
                '/sys/devices/system/cpu/cpufreq/policy0/scaling_min_freq': '1900800',
                '/sys/devices/system/cpu/cpufreq/policy4/scaling_min_freq': '2457600',
            })
        elif device_name == 'Moto G (5)':
            # MSM8937(8x 1.4GHz)
            # values obtained from:
            #   /sys/devices/system/cpu/cpufreq/policy0/scaling_available_frequencies
            for x in xrange(0, 8):
                commands.update({
                    '/sys/devices/system/cpu/cpu{}/'
                    'cpufreq/scaling_governor'.format(x): 'performance',
                    '/sys/devices/system/cpu/cpu{}/'
                    'cpufreq/scaling_min_freq'.format(x): '1401000'
                })
        else:
            pass

        for key, value in commands.items():
            self._set_value_and_check_exitcode(key, value, root=True)

    def set_gpu_performance_parameters(self, device_name):
        self.log.info('setting gpu performance parameters')
        commands = {
            '/sys/class/kgsl/kgsl-3d0/bus_split': '0',
            '/sys/class/kgsl/kgsl-3d0/force_bus_on': '1',
            '/sys/class/kgsl/kgsl-3d0/force_rail_on': '1',
            '/sys/class/kgsl/kgsl-3d0/force_clk_on': '1',
            '/sys/class/kgsl/kgsl-3d0/force_no_nap': '1',
            '/sys/class/kgsl/kgsl-3d0/idle_timer': '1000000',
        }

        if device_name == 'Pixel 2':
            # Adreno 540 (710MHz)
            # values obtained from:
            #   /sys/devices/soc/5000000.qcom,kgsl-3d0/kgsl/kgsl-3d0/max_clk_mhz
            commands.update({
                '/sys/devices/soc/5000000.qcom,kgsl-3d0/devfreq/'
                '5000000.qcom,kgsl-3d0/governor': 'performance',
                '/sys/devices/soc/soc:qcom,kgsl-busmon/devfreq/'
                'soc:qcom,kgsl-busmon/governor': 'performance',
                '/sys/devices/soc/5000000.qcom,kgsl-3d0/kgsl/kgsl-3d0/min_clock_mhz': '710',
            })
        elif device_name == 'Moto G (5)':
            # Adreno 505 (450MHz)
            # values obtained from:
            #   /sys/devices/soc/1c00000.qcom,kgsl-3d0/kgsl/kgsl-3d0/max_clock_mhz
            commands.update({
                '/sys/devices/soc/1c00000.qcom,kgsl-3d0/devfreq/'
                '1c00000.qcom,kgsl-3d0/governor': 'performance',
                '/sys/devices/soc/1c00000.qcom,kgsl-3d0/kgsl/kgsl-3d0/min_clock_mhz': '450',
            })
        else:
            pass
        for key, value in commands.items():
            self._set_value_and_check_exitcode(key, value, root=True)

    def set_kernel_performance_parameters(self):
        self.log.info('setting kernel performance parameters')
        commands = {
            '/sys/kernel/debug/msm-bus-dbg/shell-client/update_request': '1',
            '/sys/kernel/debug/msm-bus-dbg/shell-client/mas': '1',
            '/sys/kernel/debug/msm-bus-dbg/shell-client/ab': '0',
            '/sys/kernel/debug/msm-bus-dbg/shell-client/slv': '512',
        }
        for key, value in commands.items():
            self._set_value_and_check_exitcode(key, value, root=True)

    def clear_app_data(self):
        self.log.info("clearing %s app data" % self.config['binary'])
        self.device.shell("pm clear %s" % self.config['binary'])

    def copy_profile_to_device(self):
        """Copy the profile to the device, and update permissions of all files."""
        if not self.device.is_app_installed(self.config['binary']):
            raise Exception('%s is not installed' % self.config['binary'])

        try:
            self.log.info("copying profile to device: %s" % self.remote_profile)
            self.device.rm(self.remote_profile, force=True, recursive=True)
            # self.device.mkdir(self.remote_profile)
            self.device.push(self.profile.profile, self.remote_profile)
            self.device.chmod(self.remote_profile, recursive=True, root=True)

        except Exception:
            self.log.error("Unable to copy profile to device.")
            raise

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

    def launch_firefox_android_app(self, test_name):
        self.log.info("starting %s" % self.config['app'])

        extra_args = ["-profile", self.remote_profile,
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

                # Additional command line arguments that the app will read and use (e.g.
                # with a custom profile)
                extras = {}
                if extra_args:
                    extras['args'] = " ".join(extra_args)

                # add e10s=True
                extras['use_multiprocess'] = self.config['e10s']

                self.device.launch_application(self.config['binary'],
                                               self.config['activity'],
                                               self.config['intent'],
                                               extras=extras,
                                               url='about:blank',
                                               fail_if_running=False)

            # Check if app has started and it's running
            if not self.device.process_exist(self.config['binary']):
                raise Exception("Error launching %s. App did not start properly!" %
                                self.config['binary'])
        except Exception as e:
            self.log.error("Exception launching %s" % self.config['binary'])
            self.log.error("Exception: %s %s" % (type(e).__name__, str(e)))
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
                self.log.info("copying %s to %s" % (_source, _dest))
                shutil.copyfile(_source, _dest)
            else:
                self.log.critical("unable to find ssl cert db file: %s" % _source)

    def run_tests(self, tests, test_names):
        self.setup_adb_device()

        return super(RaptorAndroid, self).run_tests(tests, test_names)

    def run_test_setup(self, test):
        super(RaptorAndroid, self).run_test_setup(test)

        is_benchmark = test.get('type') == "benchmark"
        self.set_reverse_ports(is_benchmark=is_benchmark)

    def run_test_teardown(self):
        self.log.info('removing reverse socket connections')
        self.device.remove_socket_connections('reverse')

        super(RaptorAndroid, self).run_test_teardown()

    def run_test(self, test, timeout=None):
        # tests will be run warm (i.e. NO browser restart between page-cycles)
        # unless otheriwse specified in the test INI by using 'cold = true'
        try:
            if test.get('cold', False) is True:
                self.run_test_cold(test, timeout)
            else:
                self.run_test_warm(test, timeout)

        except SignalHandlerException:
            self.device.stop_application(self.config['binary'])

        finally:
            if self.config['power_test']:
                finish_android_power_test(self, test['name'])

            self.run_test_teardown()

    def run_test_cold(self, test, timeout=None):
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
        self.log.info("test %s is running in cold mode; browser WILL be restarted between "
                      "page cycles" % test['name'])

        if self.config['power_test']:
            init_android_power_test(self)

        for test['browser_cycle'] in range(1, test['expected_browser_cycles'] + 1):

            self.log.info("begin browser cycle %d of %d for test %s"
                          % (test['browser_cycle'], test['expected_browser_cycles'], test['name']))

            self.run_test_setup(test)

            # clear the android app data before the next app startup
            self.clear_app_data()

            if test['browser_cycle'] == 1:
                if test.get('playback') is not None:
                    self.start_playback(test)

                    # an ssl cert db has now been created in the profile; copy it out so we
                    # can use the same cert db in future test cycles / browser restarts
                    local_cert_db_dir = tempfile.mkdtemp()
                    self.log.info("backing up browser ssl cert db that was created via certutil")
                    self.copy_cert_db(self.config['local_profile_dir'], local_cert_db_dir)

                if self.config['host'] not in ('localhost', '127.0.0.1'):
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
                    self.log.info("copying existing ssl cert db into new browser profile")
                    self.copy_cert_db(local_cert_db_dir, self.config['local_profile_dir'])

                self.run_test_setup(test)

            if test.get('playback') is not None:
                self.turn_on_android_app_proxy()

            self.copy_profile_to_device()

            # now start the browser/app under test
            self.launch_firefox_android_app(test['name'])

            # set our control server flag to indicate we are running the browser/app
            self.control_server._finished = False

            self.wait_for_test_finish(test, timeout)

            # in debug mode, and running locally, leave the browser running
            if self.debug_mode and self.config['run_local']:
                self.log.info("* debug-mode enabled - please shutdown the browser manually...")
                self.runner.wait(timeout=None)

            # break test execution if a exception is present
            if len(self.results_handler.page_timeout_list) > 0:
                break

    def run_test_warm(self, test, timeout=None):
        self.log.info("test %s is running in warm mode; browser will NOT be restarted between "
                      "page cycles" % test['name'])
        if self.config['power_test']:
            init_android_power_test(self)

        self.run_test_setup(test)

        if test.get('playback') is not None:
            self.start_playback(test)

        if self.config['host'] not in ('localhost', '127.0.0.1'):
            self.delete_proxy_settings_from_profile()

        if test.get('playback') is not None:
            self.turn_on_android_app_proxy()

        self.clear_app_data()
        self.copy_profile_to_device()

        # now start the browser/app under test
        self.launch_firefox_android_app(test['name'])

        # set our control server flag to indicate we are running the browser/app
        self.control_server._finished = False

        self.wait_for_test_finish(test, timeout)

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
            remote_dir = posixpath.join(self.remote_profile, 'minidumps')
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

    def clean_up(self):
        self.log.info("removing test folder for raptor: %s" % self.remote_test_root)
        self.device.rm(self.remote_test_root, force=True, recursive=True)

        super(RaptorAndroid, self).clean_up()


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
    elif args.app in CHROMIUM_DISTROS:
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
                          memory_test=args.memory_test,
                          is_release_build=args.is_release_build,
                          debug_mode=args.debug_mode,
                          post_startup_delay=args.post_startup_delay,
                          activity=args.activity,
                          intent=args.intent,
                          interrupt_handler=SignalHandler(),
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
            if raptor_test.get("type") == 'pageload':
                message.append(("pending metrics", _page['pending_metrics']))

            LOG.critical(" ".join("%s: %s" % (subject, msg) for subject, msg in message))
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
