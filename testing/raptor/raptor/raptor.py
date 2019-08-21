#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from abc import ABCMeta, abstractmethod
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
from logger.logger import RaptorLogger
from mozdevice import ADBDevice
from mozlog import commandline
from mozprofile import create_profile
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
from results import RaptorResultsHandler
from utils import view_gecko_profile, write_yml_file
from cpu import generate_android_cpu_profile

LOG = RaptorLogger(component='raptor-main')


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
                 obj_path=None, profile_class=None,
                 gecko_profile=False, gecko_profile_interval=None, gecko_profile_entries=None,
                 symbols_path=None, host=None, power_test=False, cpu_test=False, memory_test=False,
                 is_release_build=False, debug_mode=False, post_startup_delay=None,
                 interrupt_handler=None, e10s=True, enable_webrender=False, **kwargs):

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
            'enable_control_server_wait': memory_test,
            'e10s': e10s,
            'enable_webrender': enable_webrender,
        }
        # We can never use e10s on fennec
        if self.config['app'] == 'fennec':
            self.config['e10s'] = False

        self.raptor_venv = os.path.join(os.getcwd(), 'raptor-venv')
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
            LOG.info("debug-mode enabled, reducing post-browser startup pause to %d ms"
                     % self.post_startup_delay)

        LOG.info("main raptor init, config is: %s" % str(self.config))

        # setup the control server
        self.results_handler = RaptorResultsHandler(self.config)

        self.build_browser_profile()

    def build_browser_profile(self):
        self.profile = create_profile(self.profile_class)

        # Merge extra profile data from testing/profiles
        with open(os.path.join(self.profile_data_dir, 'profiles.json'), 'r') as fh:
            base_profiles = json.load(fh)['raptor']

        for profile in base_profiles:
            path = os.path.join(self.profile_data_dir, profile)
            LOG.info("Merging profile: {}".format(path))
            self.profile.merge(path)

        # share the profile dir with the config and the control server
        self.config['local_profile_dir'] = self.profile.profile

    @property
    def profile_data_dir(self):
        if 'MOZ_DEVELOPER_REPO_DIR' in os.environ:
            return os.path.join(os.environ['MOZ_DEVELOPER_REPO_DIR'], 'testing', 'profiles')
        if build:
            return os.path.join(build.topsrcdir, 'testing', 'profiles')
        return os.path.join(here, 'profile_data')

    @abstractmethod
    def check_for_crashes(self):
        pass

    @abstractmethod
    def run_test_setup(self, test):
        LOG.info("starting test: %s" % test['name'])

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
            return self.process_results(test_names)
        finally:
            self.clean_up()

    @abstractmethod
    def run_test(self, test, timeout):
        raise NotImplementedError()

    @abstractmethod
    def run_test_teardown(self, test):
        self.check_for_crashes()

        # gecko profiling symbolication
        if self.config['gecko_profile'] is True:
            self.gecko_profiler.symbolicate()
            # clean up the temp gecko profiling folders
            LOG.info("cleaning up after gecko profiling")
            self.gecko_profiler.clean()

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

    @abstractmethod
    def clean_up(self):
        pass

    def get_page_timeout_list(self):
        return self.results_handler.page_timeout_list


class Raptor(Perftest):
    """Container class for Raptor"""

    def __init__(self, *args, **kwargs):
        self.raptor_webext = None
        self.control_server = None

        super(Raptor, self).__init__(*args, **kwargs)

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

        if test.get("preferences") is not None:
            self.set_browser_test_prefs(test['preferences'])

        # if 'alert_on' was provided in the test INI, add to our config for results/output
        self.config['subtest_alert_on'] = test.get('alert_on')

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
        # By default we are connecting to upstream. In the future we might want
        # to flip that default to false so all tests will stop connecting to
        # the upstream server.
        upstream = test.get("playback_upstream_cert", "true")
        self.config["playback_upstream_cert"] = upstream.lower() in ("true", "1")

        for key in ('playback_pageset_manifest', 'playback_pageset_zip'):
            if self.config.get(key) is None:
                continue
            self.config[key] = os.path.join(playback_dir, self.config[key])

        LOG.info("test uses playback tool: %s " % self.config['playback_tool'])

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

    def start_playback(self, test):
        # creating the playback tool
        self.get_playback_config(test)
        self.playback = get_playback(self.config, self.device)

        self.playback.config['playback_files'] = self.get_recording_paths(test)

        # let's start it!
        self.playback.start()

        self.log_recording_dates(test)

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
        for r in self.get_recording_paths(test):
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
        LOG.info("initializing gecko profiler")
        upload_dir = os.getenv('MOZ_UPLOAD_DIR')
        if not upload_dir:
            LOG.critical("Profiling ignored because MOZ_UPLOAD_DIR was not set")
        else:
            self.gecko_profiler = GeckoProfile(upload_dir,
                                               self.config,
                                               test)

    def clean_up(self):
        super(Raptor, self).clean_up()

        if self.config['enable_control_server_wait']:
            self.control_server_wait_clear('all')

        self.control_server.stop()
        LOG.info("finished")

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
        if test.get('cold', False) is True:
            self.__run_test_cold(test, timeout)
        else:
            self.__run_test_warm(test, timeout)

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

    def __run_test_warm(self, test, timeout):
        self.run_test_setup(test)

        if test.get('playback') is not None:
            self.start_playback(test)

        if self.config['host'] not in ('localhost', '127.0.0.1'):
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
        LOG.info("starting %s" % self.config['app'])
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
        LOG.info("preferences were configured for the test, however \
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
        self.os_baseline_data = None
        self.power_test_time = None
        self.screen_off_timeout = 0
        self.screen_brightness = 127
        self.app_launched = False

    def set_reverse_port(self, port):
        tcp_port = "tcp:{}".format(port)
        self.device.create_socket_connection('reverse', tcp_port, tcp_port)

    def set_reverse_ports(self, is_benchmark=False):
        # Make services running on the host available to the device
        if self.config['host'] in ('localhost', '127.0.0.1'):
            LOG.info("making the raptor control server port available to device")
            self.set_reverse_port(self.control_server.port)

        if self.config['host'] in ('localhost', '127.0.0.1'):
            LOG.info("making the raptor playback server port available to device")
            self.set_reverse_port(8080)

        if is_benchmark and self.config['host'] in ('localhost', '127.0.0.1'):
            LOG.info("making the raptor benchmarks server port available to device")
            self.set_reverse_port(self.benchmark_port)

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

    def build_browser_profile(self):
        super(RaptorAndroid, self).build_browser_profile()

        # Merge in the android profile
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
        LOG.info("setting profile prefs to turn on the android app proxy")
        proxy_prefs = {}
        proxy_prefs["network.proxy.type"] = 1
        proxy_prefs["network.proxy.http"] = self.config['host']
        proxy_prefs["network.proxy.http_port"] = 8080
        proxy_prefs["network.proxy.ssl"] = self.config['host']
        proxy_prefs["network.proxy.ssl_port"] = 8080
        proxy_prefs["network.proxy.no_proxies_on"] = self.config['host']
        self.profile.set_preferences(proxy_prefs)

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

        is_benchmark = test.get('type') == "benchmark"
        self.set_reverse_ports(is_benchmark=is_benchmark)

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
                    self.start_playback(test)

                    # an ssl cert db has now been created in the profile; copy it out so we
                    # can use the same cert db in future test cycles / browser restarts
                    local_cert_db_dir = tempfile.mkdtemp()
                    LOG.info("backing up browser ssl cert db that was created via certutil")
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

            # If we are measuring CPU, let's grab a snapshot
            if self.config['cpu_test']:
                generate_android_cpu_profile(self, test['name'])

            # set our control server flag to indicate we are running the browser/app
            self.control_server._finished = False

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

        if test.get('playback') is not None:
            self.start_playback(test)

        if self.config['host'] not in ('localhost', '127.0.0.1'):
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

        # If we are collecting CPU info, let's grab the details
        if self.config['cpu_test']:
            generate_android_cpu_profile(self, test['name'])

        # set our control server flag to indicate we are running the browser/app
        self.control_server._finished = False

        self.wait_for_test_finish(test, timeout)

        # in debug mode, and running locally, leave the browser running
        if self.debug_mode and self.config['run_local']:
            LOG.info("* debug-mode enabled - please shutdown the browser manually...")
            self.runner.wait(timeout=None)

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
                LOG.error("No crash directory (%s) found on remote device" % remote_dir)
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

    if args.app == "firefox":
        raptor_class = RaptorDesktopFirefox
    elif args.app in CHROMIUM_DISTROS:
        raptor_class = RaptorDesktopChrome
    else:
        raptor_class = RaptorAndroid

    raptor = raptor_class(args.app,
                          args.binary,
                          run_local=args.run_local,
                          noinstall=args.noinstall,
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
