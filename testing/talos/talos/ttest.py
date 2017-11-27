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
from __future__ import absolute_import, print_function

import json
import os
import platform
import shutil
import subprocess
import sys

import mozcrash
import mozfile
import results
import talosconfig
import utils
from mozlog import get_proxy_logger
from talos.cmanager import CounterManagement
from talos.ffsetup import FFSetup
from talos.talos_process import run_browser
from talos.utils import TalosCrash, TalosError, TalosRegression

LOG = get_proxy_logger()


class TTest(object):
    def check_for_crashes(self, browser_config, minidump_dir, test_name):
        # check for minidumps
        found = mozcrash.check_for_crashes(minidump_dir,
                                           browser_config['symbols_path'],
                                           test_name=test_name)
        mozfile.remove(minidump_dir)

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

        with FFSetup(browser_config, test_config) as setup:
            return self._runTest(browser_config, test_config, setup)

    @staticmethod
    def _get_counter_prefix():
        if platform.system() == "Linux":
            return 'linux'
        elif platform.system() in ("Windows", "Microsoft"):
            if '6.1' in platform.version():  # w7
                return 'w7'
            elif '6.2' in platform.version():  # w8
                return 'w8'
            # Bug 1264325 - FIXME: with python 2.7.11: reports win8 instead of 8.1
            elif '6.3' in platform.version():
                return 'w8'
            # Bug 1264325 - FIXME: with python 2.7.11: reports win8 instead of 10
            elif '10.0' in platform.version():
                return 'w8'
            else:
                raise TalosError('unsupported windows version')
        elif platform.system() == "Darwin":
            return 'mac'

    def _runTest(self, browser_config, test_config, setup):
        minidump_dir = os.path.join(setup.profile_dir, 'minidumps')
        counters = test_config.get('%s_counters' % self._get_counter_prefix(), [])
        resolution = test_config['resolution']

        # add the mainthread_io to the environment variable, as defined
        # in test.py configs
        here = os.path.dirname(os.path.realpath(__file__))
        if test_config['mainthread']:
            mainthread_io = os.path.join(here, "mainthread_io.log")
            setup.env['MOZ_MAIN_THREAD_IO_LOG'] = mainthread_io

        if browser_config['disable_stylo']:
            if browser_config['stylothreads']:
                raise TalosError("--disable-stylo conflicts with --stylo-threads")
            if browser_config['enable_stylo']:
                raise TalosError("--disable-stylo conflicts with --enable-stylo")

        # As we transition to Stylo, we need to set env vars and output data properly
        if browser_config['enable_stylo']:
            setup.env['STYLO_FORCE_ENABLED'] = '1'
        if browser_config['disable_stylo']:
            setup.env['STYLO_FORCE_DISABLED'] = '1'

        # During the Stylo transition, measure different number of threads
        if browser_config.get('stylothreads', 0) > 0:
            setup.env['STYLO_THREADS'] = str(browser_config['stylothreads'])

        # set url if there is one (i.e. receiving a test page, not a manifest/pageloader test)
        if test_config.get('url', None) is not None:
            test_config['url'] = utils.interpolate(
                test_config['url'],
                profile=setup.profile_dir,
                firefox=browser_config['browser_path']
            )

        # setup global (cross-cycle) responsiveness counters
        global_counters = {}
        if browser_config.get('xperf_path'):
            for c in test_config.get('xperf_counters', []):
                global_counters[c] = []

        if test_config.get('responsiveness') and \
           platform.system() != "Darwin":
            # ignore osx for now as per bug 1245793
            setup.env['MOZ_INSTRUMENT_EVENT_LOOP'] = '1'
            setup.env['MOZ_INSTRUMENT_EVENT_LOOP_THRESHOLD'] = '20'
            setup.env['MOZ_INSTRUMENT_EVENT_LOOP_INTERVAL'] = '10'
            global_counters['responsiveness'] = []

        setup.env['JSGC_DISABLE_POISONING'] = '1'
        setup.env['MOZ_DISABLE_NONLOCAL_CONNECTIONS'] = '1'

        # if using mitmproxy we must allow access to 'external' sites
        if browser_config.get('mitmproxy', False):
            LOG.info("Using mitmproxy so setting MOZ_DISABLE_NONLOCAL_CONNECTIONS to 0")
            setup.env['MOZ_DISABLE_NONLOCAL_CONNECTIONS'] = '0'

        # instantiate an object to hold test results
        test_results = results.TestResults(
            test_config,
            global_counters,
            browser_config.get('framework')
        )

        for i in range(test_config['cycles']):
            LOG.info("Running cycle %d/%d for %s test..."
                     % (i+1, test_config['cycles'], test_config['name']))

            # remove the browser  error file
            mozfile.remove(browser_config['error_filename'])

            # reinstall any file whose stability we need to ensure across
            # the cycles
            if test_config.get('reinstall', ''):
                for keep in test_config['reinstall']:
                    origin = os.path.join(test_config['profile_path'],
                                          keep)
                    dest = os.path.join(setup.profile_dir, keep)
                    LOG.debug("Reinstalling %s on top of %s"
                              % (origin, dest))
                    shutil.copy(origin, dest)

            # Run the test
            timeout = test_config.get('timeout', 7200)  # 2 hours default
            if setup.gecko_profile:
                # When profiling, give the browser some extra time
                # to dump the profile.
                timeout += 5 * 60
                # store profiling info for pageloader; too late to add it as browser pref
                setup.env["TPPROFILINGINFO"] = json.dumps(setup.gecko_profile.profiling_info)

            command_args = utils.GenerateBrowserCommandLine(
                browser_config["browser_path"],
                browser_config["extra_args"],
                setup.profile_dir,
                test_config['url'],
                profiling_info=(setup.gecko_profile.profiling_info
                                if setup.gecko_profile else None)
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
                    minidump_dir,
                    timeout=timeout,
                    env=setup.env,
                    # start collecting counters as soon as possible
                    on_started=(counter_management.start
                                if counter_management else None),
                )
            except:
                self.check_for_crashes(browser_config, minidump_dir,
                                       test_config['name'])
                raise
            finally:
                if counter_management:
                    counter_management.stop()

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

                        print(line)
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

            if setup.gecko_profile:
                setup.gecko_profile.symbolicate(i)

            self.check_for_crashes(browser_config, minidump_dir,
                                   test_config['name'])

        # include global (cross-cycle) counters
        test_results.all_counter_results.extend(
            [{key: value} for key, value in global_counters.items()]
        )
        for c in test_results.all_counter_results:
            for key, value in c.items():
                LOG.debug("COUNTER %r: %s" % (key, value))

        # return results
        return test_results
