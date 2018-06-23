#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import copy
import datetime
import glob
import os
import re
import sys
import signal
import subprocess
import time
import tempfile

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.log import FATAL
from mozharness.base.script import BaseScript, PreScriptAction, PostScriptAction
from mozharness.mozilla.automation import TBPL_RETRY, EXIT_STATUS_DICT
from mozharness.mozilla.mozbase import MozbaseMixin
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options
from mozharness.mozilla.testing.codecoverage import CodeCoverageMixin


class AndroidHardwareTest(TestingMixin, BaseScript, MozbaseMixin,
                          CodeCoverageMixin):
    config_options = [[
        ["--test-suite"],
        {"action": "store",
         "dest": "test_suite",
         "default": None
         }
    ], [
        ["--adb-path"],
        {"action": "store",
         "dest": "adb_path",
         "default": None,
         "help": "Path to adb",
         }
    ], [
        ["--total-chunk"],
        {"action": "store",
         "dest": "total_chunks",
         "default": None,
         "help": "Number of total chunks",
         }
    ], [
        ["--this-chunk"],
        {"action": "store",
         "dest": "this_chunk",
         "default": None,
         "help": "Number of this chunk",
         }
    ], [
        ["--log-raw-level"],
        {"action": "store",
         "dest": "log_raw_level",
         "default": "info",
         "help": "Set log level (debug|info|warning|error|critical|fatal)",
         }
    ], [
        ["--log-tbpl-level"],
        {"action": "store",
         "dest": "log_tbpl_level",
         "default": "info",
         "help": "Set log level (debug|info|warning|error|critical|fatal)",
         }
    ]] + copy.deepcopy(testing_config_options)

    app_name = None

    def __init__(self, require_config_file=False):
        super(AndroidHardwareTest, self).__init__(
            config_options=self.config_options,
            all_actions=['clobber',
                         'download-and-extract',
                         'create-virtualenv',
                         'verify-device',
                         'install',
                         'run-tests',
                         ],
            require_config_file=require_config_file,
            config={
                'virtualenv_modules': [],
                'virtualenv_requirements': [],
                'require_test_zip': True,
                # IP address of the host as seen from the device.
                'remote_webserver': os.environ['HOST_IP'],
            }
        )

        # these are necessary since self.config is read only
        c = self.config
        abs_dirs = self.query_abs_dirs()
        self.adb_path = self.query_exe('adb')
        self.logcat_file = None
        self.logcat_proc = None
        self.installer_url = c.get('installer_url')
        self.installer_path = c.get('installer_path')
        self.test_url = c.get('test_url')
        self.test_packages_url = c.get('test_packages_url')
        self.test_manifest = c.get('test_manifest')
        self.robocop_path = os.path.join(abs_dirs['abs_work_dir'], "robocop.apk")
        self.minidump_stackwalk_path = c.get("minidump_stackwalk_path")
        self.device_name = os.environ['DEVICE_NAME']
        self.device_serial = os.environ['DEVICE_SERIAL']
        self.device_ip = os.environ['DEVICE_IP']
        self.test_suite = c.get('test_suite')
        self.this_chunk = c.get('this_chunk')
        self.total_chunks = c.get('total_chunks')
        if self.test_suite and self.test_suite not in self.config["suite_definitions"]:
            # accept old-style test suite name like "mochitest-3"
            m = re.match("(.*)-(\d*)", self.test_suite)
            if m:
                self.test_suite = m.group(1)
                if self.this_chunk is None:
                    self.this_chunk = m.group(2)
        self.sdk_level = None
        self.xre_path = None
        self.log_raw_level = c.get('log_raw_level')
        self.log_tbpl_level = c.get('log_tbpl_level')

    def _query_tests_dir(self):
        dirs = self.query_abs_dirs()
        try:
            test_dir = self.config["suite_definitions"][self.test_suite]["testsdir"]
        except Exception:
            test_dir = self.test_suite
        return os.path.join(dirs['abs_test_install_dir'], test_dir)

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(AndroidHardwareTest, self).query_abs_dirs()
        dirs = {}
        dirs['abs_test_install_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'tests')
        dirs['abs_test_bin_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'tests', 'bin')
        dirs['abs_xre_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'hostutils')
        dirs['abs_modules_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'modules')
        dirs['abs_blob_upload_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'blobber_upload_dir')
        dirs['abs_mochitest_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'mochitest')
        dirs['abs_reftest_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'reftest')
        dirs['abs_xpcshell_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'xpcshell')
        dirs['abs_marionette_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'marionette', 'harness', 'marionette_harness')
        dirs['abs_marionette_tests_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'marionette', 'tests', 'testing',
            'marionette', 'harness', 'marionette_harness', 'tests')

        for key in dirs.keys():
            if key not in abs_dirs:
                abs_dirs[key] = dirs[key]
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    @PreScriptAction('create-virtualenv')
    def _pre_create_virtualenv(self, action):
        dirs = self.query_abs_dirs()
        requirements = None
        if self.test_suite == 'mochitest-media':
            # mochitest-media is the only thing that needs this
            requirements = os.path.join(dirs['abs_mochitest_dir'],
                                        'websocketprocessbridge',
                                        'websocketprocessbridge_requirements.txt')
        elif self.test_suite == 'marionette':
            requirements = os.path.join(dirs['abs_test_install_dir'],
                                        'config', 'marionette_requirements.txt')
        if requirements:
            self.register_virtualenv_module(requirements=[requirements],
                                            two_pass=True)

    def _retry(self, max_attempts, interval, func, description, max_time=0):
        '''
        Execute func until it returns True, up to max_attempts times, waiting for
        interval seconds between each attempt. description is logged on each attempt.
        If max_time is specified, no further attempts will be made once max_time
        seconds have elapsed; this provides some protection for the case where
        the run-time for func is long or highly variable.
        '''
        status = False
        attempts = 0
        if max_time > 0:
            end_time = datetime.datetime.now() + datetime.timedelta(seconds=max_time)
        else:
            end_time = None
        while attempts < max_attempts and not status:
            if (end_time is not None) and (datetime.datetime.now() > end_time):
                self.info("Maximum retry run-time of %d seconds exceeded; "
                          "remaining attempts abandoned" % max_time)
                break
            if attempts != 0:
                self.info("Sleeping %d seconds" % interval)
                time.sleep(interval)
            attempts += 1
            self.info(">> %s: Attempt #%d of %d" % (description, attempts, max_attempts))
            status = func()
        return status

    def _run_with_timeout(self, timeout, cmd, quiet=False):
        timeout_cmd = ['timeout', '%s' % timeout] + cmd
        return self._run_proc(timeout_cmd, quiet=quiet)

    def _run_proc(self, cmd, quiet=False):
        self.info('Running %s' % subprocess.list2cmdline(cmd))
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        out, err = p.communicate()
        if out and not quiet:
            self.info('%s' % str(out.strip()))
        if err and not quiet:
            self.info('stderr: %s' % str(err.strip()))
        return out, err

    def _verify_adb(self):
        self.info('Verifying adb connectivity')
        self._run_with_timeout(180, [self.adb_path,
                                     '-s',
                                     self.device_serial,
                                     'wait-for-device'])
        return True

    def _verify_adb_device(self):
        out, _ = self._run_with_timeout(30, [self.adb_path, 'devices'])
        if (self.device_serial in out) and ("device" in out):
            return True
        return False

    def _is_boot_completed(self):
        boot_cmd = [self.adb_path, '-s', self.device_serial,
                    'shell', 'getprop', 'sys.boot_completed']
        out, _ = self._run_with_timeout(30, boot_cmd)
        if out.strip() == '1':
            return True
        return False

    def _verify_device(self):
        adb_ok = self._verify_adb()
        if not adb_ok:
            self.warning('Unable to communicate with adb')
            return False
        adb_device_ok = self._retry(4, 30, self._verify_adb_device,
                                    "Verify device visible to adb")
        if not adb_device_ok:
            self.warning('Unable to communicate with device via adb')
            return False
        boot_ok = self._retry(30, 10, self._is_boot_completed, "Verify Android boot completed",
                              max_time=330)
        if not boot_ok:
            self.warning('Unable to verify Android boot completion')
            return False
        return True

    def _install_fennec_apk(self):
        package = self._query_package_name()
        if package:
            cmd = [self.adb_path, '-s', self.device_serial,
                   'uninstall', package]
            out, err = self._run_with_timeout(300, cmd, True)
        install_ok = False
        if int(self.sdk_level) >= 23:
            cmd = [self.adb_path, '-s', self.device_serial, 'install', '-r', '-g',
                   self.installer_path]
        else:
            cmd = [self.adb_path, '-s', self.device_serial, 'install', '-r',
                   self.installer_path]
        out, err = self._run_with_timeout(300, cmd, True)
        if 'Success' in out or 'Success' in err:
            install_ok = True
        return install_ok

    def _install_robocop_apk(self):
        install_ok = False
        if int(self.sdk_level) >= 23:
            cmd = [self.adb_path, '-s', self.device_serial, 'install', '-r', '-g',
                   self.robocop_path]
        else:
            cmd = [self.adb_path, '-s', self.device_serial, 'install', '-r',
                   self.robocop_path]
        out, err = self._run_with_timeout(300, cmd, True)
        if 'Success' in out or 'Success' in err:
            install_ok = True
        return install_ok

    def _dump_host_state(self):
        self._run_proc(['ps', '-ef'])
        self._run_proc(['netstat', '-a', '-p', '-n', '-t', '-u'])

    def _kill_processes(self, process_name):
        p = subprocess.Popen(['ps', '-A'], stdout=subprocess.PIPE)
        out, err = p.communicate()
        self.info("Let's kill every process called %s" % process_name)
        for line in out.splitlines():
            if process_name in line:
                pid = int(line.split(None, 1)[0])
                self.info("Killing pid %d." % pid)
                os.kill(pid, signal.SIGKILL)

    def _restart_adbd(self):
        self._run_with_timeout(30, [self.adb_path, 'kill-server'])
        self._run_with_timeout(30, [self.adb_path, 'root'])

    def _screenshot(self, prefix):
        """
           Save a screenshot of the entire screen to the blob upload directory.
        """
        dirs = self.query_abs_dirs()
        utility = os.path.join(self.xre_path, "screentopng")
        if not os.path.exists(utility):
            self.warning("Unable to take screenshot: %s does not exist" % utility)
            return
        try:
            tmpfd, filename = tempfile.mkstemp(prefix=prefix, suffix='.png',
                                               dir=dirs['abs_blob_upload_dir'])
            os.close(tmpfd)
            self.info("Taking screenshot with %s; saving to %s" % (utility, filename))
            subprocess.call([utility, filename], env=self.query_env())
        except OSError, err:
            self.warning("Failed to take screenshot: %s" % err.strerror)

    def _query_package_name(self):
        if self.app_name is None:
            # For convenience, assume geckoview.test/geckoview_example when install
            # target looks like geckoview.
            if 'androidTest' in self.installer_path:
                self.app_name = 'org.mozilla.geckoview.test'
            elif 'geckoview' in self.installer_path:
                self.app_name = 'org.mozilla.geckoview_example'
        if self.app_name is None:
            # Find appname from package-name.txt - assumes download-and-extract
            # has completed successfully.
            # The app/package name will typically be org.mozilla.fennec,
            # but org.mozilla.firefox for release builds, and there may be
            # other variations. 'aapt dump badging <apk>' could be used as an
            # alternative to package-name.txt, but introduces a dependency
            # on aapt, found currently in the Android SDK build-tools component.
            apk_dir = self.abs_dirs['abs_work_dir']
            self.apk_path = os.path.join(apk_dir, self.installer_path)
            unzip = self.query_exe("unzip")
            package_path = os.path.join(apk_dir, 'package-name.txt')
            unzip_cmd = [unzip, '-q', '-o',  self.apk_path]
            self.run_command(unzip_cmd, cwd=apk_dir, halt_on_failure=True)
            self.app_name = str(self.read_from_file(package_path, verbose=True)).rstrip()
        return self.app_name

    def preflight_install(self):
        # in the base class, this checks for mozinstall, but we don't use it
        pass

    def _build_command(self):
        c = self.config
        dirs = self.query_abs_dirs()

        if self.test_suite not in self.config["suite_definitions"]:
            self.fatal("Key '%s' not defined in the config!" % self.test_suite)

        cmd = [
            self.query_python_path('python'),
            '-u',
            os.path.join(
                self._query_tests_dir(),
                self.config["suite_definitions"][self.test_suite]["run_filename"]
            ),
        ]

        raw_log_file = os.path.join(dirs['abs_blob_upload_dir'],
                                    '%s_raw.log' % self.test_suite)

        error_summary_file = os.path.join(dirs['abs_blob_upload_dir'],
                                          '%s_errorsummary.log' % self.test_suite)
        str_format_values = {
            'device_serial': self.device_serial,
            'remote_webserver': c['remote_webserver'],
            'xre_path': self.xre_path,
            'utility_path': self.xre_path,
            'http_port': '8854',  # starting http port  to use for the mochitest server
            'ssl_port': '4454',  # starting ssl port to use for the server
            'certs_path': os.path.join(dirs['abs_work_dir'], 'tests/certs'),
            # TestingMixin._download_and_extract_symbols() will set
            # self.symbols_path when downloading/extracting.
            'symbols_path': self.symbols_path,
            'modules_dir': dirs['abs_modules_dir'],
            'installer_path': self.installer_path,
            'raw_log_file': raw_log_file,
            'log_tbpl_level': self.log_tbpl_level,
            'log_raw_level': self.log_raw_level,
            'error_summary_file': error_summary_file,
            # marionette options
            'address': c.get('marionette_address') % {'device_ip': self.device_ip},
            'gecko_log': os.path.join(dirs["abs_blob_upload_dir"], 'gecko.log'),
            'test_manifest': os.path.join(
                dirs['abs_marionette_tests_dir'],
                self.config.get('marionette_test_manifest', '')
            ),
        }

        user_paths = os.environ.get('MOZHARNESS_TEST_PATHS')
        for option in self.config["suite_definitions"][self.test_suite]["options"]:
            opt = option.split('=')[0]
            # override configured chunk options with script args, if specified
            if opt in ('--this-chunk', '--total-chunks'):
                if user_paths or getattr(self, opt.replace('-', '_').strip('_'), None) is not None:
                    continue

            if '%(app)' in option:
                # only query package name if requested
                cmd.extend([option % {'app': self._query_package_name()}])
            else:
                cmd.extend([option % str_format_values])

        if user_paths:
            cmd.extend(user_paths.split(':'))
        elif not self.verify_enabled:
            if self.this_chunk is not None:
                cmd.extend(['--this-chunk', self.this_chunk])
            if self.total_chunks is not None:
                cmd.extend(['--total-chunks', self.total_chunks])

        try_options, try_tests = self.try_args(self.test_suite)
        cmd.extend(try_options)
        if not self.verify_enabled and not self.per_test_coverage:
            cmd.extend(self.query_tests_args(
                self.config["suite_definitions"][self.test_suite].get("tests"),
                None,
                try_tests))

        return cmd

    def _get_repo_url(self, path):
        """
           Return a url for a file (typically a tooltool manifest) in this hg repo
           and using this revision (or mozilla-central/default if repo/rev cannot
           be determined).

           :param path specifies the directory path to the file of interest.
        """
        if 'GECKO_HEAD_REPOSITORY' in os.environ and 'GECKO_HEAD_REV' in os.environ:
            # probably taskcluster
            repo = os.environ['GECKO_HEAD_REPOSITORY']
            revision = os.environ['GECKO_HEAD_REV']
        else:
            # something unexpected!
            repo = 'https://hg.mozilla.org/mozilla-central'
            revision = 'default'
            self.warning('Unable to find repo/revision for manifest; '
                         'using mozilla-central/default')
        url = '%s/raw-file/%s/%s' % (
            repo,
            revision,
            path)
        return url

    def _tooltool_fetch(self, url, dir):
        c = self.config

        manifest_path = self.download_file(
            url,
            file_name='releng.manifest',
            parent_dir=dir
        )

        if not os.path.exists(manifest_path):
            self.fatal("Could not retrieve manifest needed to retrieve "
                       "artifacts from %s" % manifest_path)

        self.tooltool_fetch(manifest_path,
                            output_dir=dir,
                            cache=c.get("tooltool_cache", None))

    ##########################################
    # Actions for AndroidHardwareTest        #
    ##########################################
    def _dump_perf_info(self):
        '''
        Dump some host and device performance-related information
        to an artifact file, to help understand why jobs run slowly
        sometimes. This is hopefully a temporary diagnostic.
        See bug 1321605.
        '''
        dir = self.query_abs_dirs()['abs_blob_upload_dir']
        perf_path = os.path.join(dir, "android-performance.log")
        with open(perf_path, "w") as f:

            f.write('\n\nHost /proc/cpuinfo:\n')
            out, _ = self._run_proc(['cat', '/proc/cpuinfo'], quiet=True)
            f.write(out)

            f.write('\n\nHost /proc/meminfo:\n')
            out, _ = self._run_proc(['cat', '/proc/meminfo'], quiet=True)
            f.write(out)

            f.write('\n\nHost process list:\n')
            out, _ = self._run_proc(['ps', '-ef'], quiet=True)
            f.write(out)

            f.write('\n\nDevice /proc/cpuinfo:\n')
            cmd = [self.adb_path, '-s', self.device_serial,
                   'shell', 'cat', '/proc/cpuinfo']
            out, _ = self._run_with_timeout(30, cmd, quiet=True)
            f.write(out)
            cpuinfo = out

            f.write('\n\nDevice /proc/meminfo:\n')
            cmd = [self.adb_path, '-s', self.device_serial,
                   'shell', 'cat', '/proc/meminfo']
            out, _ = self._run_with_timeout(30, cmd, quiet=True)
            f.write(out)

            f.write('\n\nDevice process list:\n')
            cmd = [self.adb_path, '-s', self.device_serial,
                   'shell', 'ps']
            out, _ = self._run_with_timeout(30, cmd, quiet=True)
            f.write(out)

        for line in cpuinfo.split('\n'):
            m = re.match("BogoMIPS.*: (\d*)", line)
            if m:
                bogomips = int(m.group(1))
                self.info("Found Android bogomips: %d" % bogomips)
                break

    def verify_device(self):
        '''
        Check to see if the device can be contacted via adb.
        '''
        self.mkdir_p(self.query_abs_dirs()['abs_blob_upload_dir'])
        self._dump_perf_info()
        # Start logcat for the device. The adb process runs until the
        # corresponding device is stopped. Output is written directly to
        # the blobber upload directory so that it is uploaded automatically
        # at the end of the job.
        logcat_filename = 'logcat-%s.log' % self.device_serial
        logcat_path = os.path.join(self.abs_dirs['abs_blob_upload_dir'],
                                   logcat_filename)
        self.logcat_file = open(logcat_path, 'w')
        logcat_cmd = [self.adb_path, '-s', self.device_serial, 'logcat', '-v',
                      'threadtime', 'Trace:S', 'StrictMode:S',
                      'ExchangeService:S']
        self.info(' '.join(logcat_cmd))
        self.logcat_proc = subprocess.Popen(logcat_cmd, stdout=self.logcat_file,
                                            stdin=subprocess.PIPE)
        # Get a post-boot device process list for diagnostics
        ps_cmd = [self.adb_path, '-s', self.device_serial, 'shell', 'ps']
        self._run_with_timeout(30, ps_cmd)

    def download_and_extract(self):
        """
        Download and extract fennec APK, tests.zip, host utils, and robocop (if required).
        """
        super(AndroidHardwareTest, self).download_and_extract(
            suite_categories=self._query_suite_categories())
        dirs = self.query_abs_dirs()
        if self.test_suite and self.test_suite.startswith('robocop'):
            robocop_url = self.installer_url[:self.installer_url.rfind('/')] + '/robocop.apk'
            self.info("Downloading robocop...")
            self.download_file(robocop_url, 'robocop.apk', dirs['abs_work_dir'], error_level=FATAL)
        self.rmtree(dirs['abs_xre_dir'])
        self.mkdir_p(dirs['abs_xre_dir'])
        if self.config["hostutils_manifest_path"]:
            url = self._get_repo_url(self.config["hostutils_manifest_path"])
            self._tooltool_fetch(url, dirs['abs_xre_dir'])
            for p in glob.glob(os.path.join(dirs['abs_xre_dir'], 'host-utils-*')):
                if os.path.isdir(p) and os.path.isfile(os.path.join(p, 'xpcshell')):
                    self.xre_path = p
            if not self.xre_path:
                self.fatal("xre path not found in %s" % dirs['abs_xre_dir'])
        else:
            self.fatal("configure hostutils_manifest_path!")

    def install(self):
        """
        Install APKs on the device.
        """
        install_needed = (not self.test_suite) or \
            self.config["suite_definitions"][self.test_suite].get("install")
        if install_needed is False:
            self.info("Skipping apk installation for %s" % self.test_suite)
            return

        assert self.installer_path is not None, \
            "Either add installer_path to the config or use --installer-path."

        cmd = [self.adb_path, '-s', self.device_serial, 'shell',
               'getprop', 'ro.build.version.sdk']
        self.sdk_level, _ = self._run_with_timeout(30, cmd)

        # Install Fennec
        install_ok = self._retry(3, 30, self._install_fennec_apk, "Install app APK")
        if not install_ok:
            self.fatal('INFRA-ERROR: Failed to install %s on %s' %
                       (self.installer_path, self.device_name),
                       EXIT_STATUS_DICT[TBPL_RETRY])

        # Install Robocop if required
        if self.test_suite and self.test_suite.startswith('robocop'):
            install_ok = self._retry(3, 30, self._install_robocop_apk, "Install Robocop APK")
            if not install_ok:
                self.fatal('INFRA-ERROR: Failed to install %s on %s' %
                           (self.robocop_path, self.device_name),
                           EXIT_STATUS_DICT[TBPL_RETRY])

        self.info("Finished installing apps for %s" % self.device_name)

    def _query_suites(self):
        if self.test_suite:
            return [(self.test_suite, self.test_suite)]
        # per-test mode: determine test suites to run
        all = [('mochitest', {'plain': 'mochitest',
                              'chrome': 'mochitest-chrome',
                              'plain-clipboard': 'mochitest-plain-clipboard',
                              'plain-gpu': 'mochitest-plain-gpu'}),
               ('reftest', {'reftest': 'reftest', 'crashtest': 'crashtest'}),
               ('xpcshell', {'xpcshell': 'xpcshell'})]
        suites = []
        for (category, all_suites) in all:
            cat_suites = self.query_per_test_category_suites(category, all_suites)
            for k in cat_suites.keys():
                suites.append((k, cat_suites[k]))
        return suites

    def _query_suite_categories(self):
        if self.test_suite:
            categories = [self.test_suite]
        else:
            # per-test mode
            categories = ['mochitest', 'reftest', 'xpcshell']
        return categories

    def run_tests(self):
        """
        Run the tests
        """
        self.start_time = datetime.datetime.now()
        max_per_test_time = datetime.timedelta(minutes=60)

        per_test_args = []
        suites = self._query_suites()
        minidump = self.query_minidump_stackwalk()
        for (per_test_suite, suite) in suites:
            self.test_suite = suite

            cmd = self._build_command()

            try:
                cwd = self._query_tests_dir()
            except Exception:
                self.fatal("Don't know how to run --test-suite '%s'!" % self.test_suite)
            env = self.query_env()
            if minidump:
                env['MINIDUMP_STACKWALK'] = minidump
            env['MOZ_UPLOAD_DIR'] = self.query_abs_dirs()['abs_blob_upload_dir']
            env['MINIDUMP_SAVE_PATH'] = self.query_abs_dirs()['abs_blob_upload_dir']
            env['RUST_BACKTRACE'] = 'full'

            summary = None
            for per_test_args in self.query_args(per_test_suite):
                if (datetime.datetime.now() - self.start_time) > max_per_test_time:
                    # Running tests has run out of time. That is okay! Stop running
                    # them so that a task timeout is not triggered, and so that
                    # (partial) results are made available in a timely manner.
                    self.info("TinderboxPrint: Running tests took too long: "
                              "Not all tests were executed.<br/>")
                    # Signal per-test time exceeded, to break out of suites and
                    # suite categories loops also.
                    return False

                final_cmd = copy.copy(cmd)
                if len(per_test_args) > 0:
                    # in per-test mode, remove any chunk arguments from command
                    for arg in final_cmd:
                        if 'total-chunk' in arg or 'this-chunk' in arg:
                            final_cmd.remove(arg)
                final_cmd.extend(per_test_args)

                self.info("Running on %s the command %s" % (self.device_name,
                          subprocess.list2cmdline(final_cmd)))
                self.info("##### %s log begins" % self.test_suite)

                suite_category = self.test_suite
                parser = self.get_test_output_parser(
                    suite_category,
                    config=self.config,
                    log_obj=self.log_obj,
                    error_list=[])
                self.run_command(final_cmd, cwd=cwd, env=env, output_parser=parser)
                tbpl_status, log_level, summary = parser.evaluate_parser(0, summary)
                parser.append_tinderboxprint_line(self.test_suite)

                self.info("##### %s log ends" % self.test_suite)

                if len(per_test_args) > 0:
                    self.record_status(tbpl_status, level=log_level)
                    self.log_per_test_status(per_test_args[-1], tbpl_status, log_level)
                else:
                    self.record_status(tbpl_status, level=log_level)
                    self.log("The %s suite: %s ran with return status: %s" %
                             (suite_category, suite, tbpl_status), level=log_level)

    @PostScriptAction('run-tests')
    def stop_device(self, action, success=None):
        '''
        Report device health.
        '''
        if self.logcat_proc:
            self.info("Killing logcat pid %d." % self.logcat_proc.pid)
            self.logcat_proc.kill()
            self.logcat_file.close()


if __name__ == '__main__':
    hardwareTest = AndroidHardwareTest()
    hardwareTest.run_and_exit()
