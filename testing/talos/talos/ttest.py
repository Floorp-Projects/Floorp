# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""A generic means of running an URL based browser test
   follows the following steps
     - creates a profile
     - tests the profile
     - gets metrics for the current test environment
     - loads the url
     - collects info on any counters while test runs
     - waits for a 'dump' from the browser
"""

import os
import sys
import platform
import results
import subprocess
import utils
import mozcrash
import talosconfig
import shutil
import mozfile
import logging

from talos.utils import TalosError, TalosCrash, TalosRegression
from talos.talos_process import run_browser
from talos.ffsetup import FFSetup
from talos.cmanager import CounterManagement


class TTest(object):

    if platform.system() == "Linux":
        platform_type = 'linux_'
    elif platform.system() in ("Windows", "Microsoft"):
        if '5.1' in platform.version():  # winxp
            platform_type = 'win_'
        elif '6.1' in platform.version():  # w7
            platform_type = 'w7_'
        elif '6.2' in platform.version():  # w8
            platform_type = 'w8_'
        else:
            raise TalosError('unsupported windows version')
    elif platform.system() == "Darwin":
        platform_type = 'mac_'

    def check_for_crashes(self, browser_config, profile_dir, test_name):
        # check for minidumps
        minidumpdir = os.path.join(profile_dir, 'minidumps')
        found = mozcrash.check_for_crashes(minidumpdir,
                                           browser_config['symbols_path'],
                                           test_name=test_name)
        mozfile.remove(minidumpdir)

        if found:
            raise TalosCrash("Found crashes after test run, terminating test")

    def runTest(self, browser_config, test_config):
        """
            Runs an url based test on the browser as specified in the
            browser_config dictionary

        Args:
            browser_config:  Dictionary of configuration options for the
                             browser (paths, prefs, etc)
            test_config   :  Dictionary of configuration for the given
                             test (url, cycles, counters, etc)

        """

        logging.debug("operating with platform_type : %s", self.platform_type)
        with FFSetup(browser_config, test_config) as setup:
            return self._runTest(browser_config, test_config, setup)

    def _runTest(self, browser_config, test_config, setup):
        counters = test_config.get(self.platform_type + 'counters', [])
        resolution = test_config['resolution']

        # add the mainthread_io to the environment variable, as defined
        # in test.py configs
        here = os.path.dirname(os.path.realpath(__file__))
        if test_config['mainthread']:
            mainthread_io = os.path.join(here, "mainthread_io.log")
            setup.env['MOZ_MAIN_THREAD_IO_LOG'] = mainthread_io

        test_config['url'] = utils.interpolate(
            test_config['url'],
            profile=setup.profile_dir,
            firefox=browser_config['browser_path']
        )

        # setup global (cross-cycle) counters:
        # shutdown, responsiveness
        global_counters = {}
        if browser_config.get('xperf_path'):
            for c in test_config.get('xperf_counters', []):
                global_counters[c] = []

        if test_config['shutdown']:
            global_counters['shutdown'] = []
        if test_config.get('responsiveness') and \
                platform.system() != "Linux":
            # ignore responsiveness tests on linux until we fix
            # Bug 710296
            setup.env['MOZ_INSTRUMENT_EVENT_LOOP'] = '1'
            setup.env['MOZ_INSTRUMENT_EVENT_LOOP_THRESHOLD'] = '20'
            setup.env['MOZ_INSTRUMENT_EVENT_LOOP_INTERVAL'] = '10'
            global_counters['responsiveness'] = []

        # instantiate an object to hold test results
        test_results = results.TestResults(
            test_config,
            global_counters
        )

        for i in range(test_config['cycles']):
            logging.info("Running cycle %d/%d for %s test...",
                         i+1, test_config['cycles'], test_config['name'])

            # remove the browser  error file
            mozfile.remove(browser_config['error_filename'])

            # reinstall any file whose stability we need to ensure across
            # the cycles
            if test_config.get('reinstall', ''):
                for keep in test_config['reinstall']:
                    origin = os.path.join(test_config['profile_path'],
                                          keep)
                    dest = os.path.join(setup.profile_dir, keep)
                    logging.debug("Reinstalling %s on top of %s", origin,
                                  dest)
                    shutil.copy(origin, dest)

            # Run the test
            timeout = test_config.get('timeout', 7200)  # 2 hours default
            if setup.sps_profile:
                # When profiling, give the browser some extra time
                # to dump the profile.
                timeout += 5 * 60

            command_args = utils.GenerateBrowserCommandLine(
                browser_config["browser_path"],
                browser_config["extra_args"],
                setup.profile_dir,
                test_config['url'],
                profiling_info=(setup.sps_profile.profiling_info
                                if setup.sps_profile else None)
            )

            mainthread_error_count = 0
            if test_config['setup']:
                # Generate bcontroller.json for xperf
                talosconfig.generateTalosConfig(command_args,
                                                browser_config,
                                                test_config)
                subprocess.call(
                    ['python'] + test_config['setup'].split(),
                )

            mm_httpd = None

            if test_config['name'] == 'media_tests':
                from startup_test.media import media_manager
                mm_httpd = media_manager.run_server(
                    os.path.dirname(os.path.realpath(__file__))
                )

            counter_management = None
            if counters:
                counter_management = CounterManagement(
                    browser_config['process'],
                    counters,
                    resolution
                )

            try:
                pcontext = run_browser(
                    command_args,
                    timeout=timeout,
                    env=setup.env,
                    # start collecting counters as soon as possible
                    on_started=(counter_management.start
                                if counter_management else None),
                )
            finally:
                if counter_management:
                    counter_management.stop()
                if mm_httpd:
                    mm_httpd.stop()

            if test_config['mainthread']:
                rawlog = os.path.join(here, "mainthread_io.log")
                if os.path.exists(rawlog):
                    processedlog = \
                        os.path.join(here, 'mainthread_io.json')
                    xre_path = \
                        os.path.dirname(browser_config['browser_path'])
                    mtio_py = os.path.join(here, 'mainthreadio.py')
                    command = ['python', mtio_py, rawlog,
                               processedlog, xre_path]
                    mtio = subprocess.Popen(command,
                                            env=os.environ.copy(),
                                            stdout=subprocess.PIPE)
                    output, stderr = mtio.communicate()
                    for line in output.split('\n'):
                        if line.strip() == "":
                            continue

                        print line
                        mainthread_error_count += 1
                    mozfile.remove(rawlog)

            if test_config['cleanup']:
                # HACK: add the pid to support xperf where we require
                # the pid in post processing
                talosconfig.generateTalosConfig(command_args,
                                                browser_config,
                                                test_config,
                                                pid=pcontext.pid)
                subprocess.call(
                    [sys.executable] + test_config['cleanup'].split()
                )

            # For startup tests, we launch the browser multiple times
            # with the same profile
            for fname in ('sessionstore.js', '.parentlock',
                          'sessionstore.bak'):
                mozfile.remove(os.path.join(setup.profile_dir, fname))

            # check for xperf errors
            if os.path.exists(browser_config['error_filename']) or \
               mainthread_error_count > 0:
                raise TalosRegression(
                    "Talos has found a regression, if you have questions"
                    " ask for help in irc on #perf"
                )

            # add the results from the browser output
            test_results.add(
                '\n'.join(pcontext.output),
                counter_results=(counter_management.results()
                                 if counter_management
                                 else None)
            )

            if setup.sps_profile:
                setup.sps_profile.symbolicate(i)

            self.check_for_crashes(browser_config, setup.profile_dir,
                                   test_config['name'])

        # include global (cross-cycle) counters
        test_results.all_counter_results.extend(
            [{key: value} for key, value in global_counters.items()]
        )
        for c in test_results.all_counter_results:
            for key, value in c.items():
                logging.debug("COUNTER %r: %s", key, value)

        # return results
        return test_results
