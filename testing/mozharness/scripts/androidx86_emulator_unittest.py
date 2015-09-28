#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import copy
import os
import sys
import signal
import socket
import subprocess
import telnetlib
import time
import tempfile

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozprocess import ProcessHandler

from mozharness.base.log import FATAL
from mozharness.base.script import BaseScript, PostScriptRun
from mozharness.base.vcs.vcsbase import VCSMixin
from mozharness.mozilla.blob_upload import BlobUploadMixin, blobupload_config_options
from mozharness.mozilla.mozbase import MozbaseMixin
from mozharness.mozilla.buildbot import TBPL_WORST_LEVEL_TUPLE
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options
from mozharness.mozilla.testing.unittest import EmulatorMixin

from mozharness.mozilla.testing.device import ADBDeviceHandler


class AndroidEmulatorTest(BlobUploadMixin, TestingMixin, EmulatorMixin, VCSMixin, BaseScript, MozbaseMixin):
    config_options = [[
        ["--robocop-url"],
        {"action": "store",
         "dest": "robocop_url",
         "default": None,
         "help": "URL to the robocop apk",
         }
    ], [
        ["--host-utils-url"],
        {"action": "store",
         "dest": "xre_url",
         "default": None,
         "help": "URL to the host utils zip",
         }
    ], [
        ["--test-suite"],
        {"action": "append",
         "dest": "test_suites",
         }
    ], [
        ["--adb-path"],
        {"action": "store",
         "dest": "adb_path",
         "default": None,
         "help": "Path to adb",
         }
    ]] + copy.deepcopy(testing_config_options) + \
        copy.deepcopy(blobupload_config_options)

    error_list = [
    ]

    virtualenv_requirements = [
    ]

    virtualenv_modules = [
    ]

    app_name = None

    def __init__(self, require_config_file=False):
        super(AndroidEmulatorTest, self).__init__(
            config_options=self.config_options,
            all_actions=['clobber',
                         'read-buildbot-config',
                         'setup-avds',
                         'start-emulators',
                         'download-and-extract',
                         'create-virtualenv',
                         'install',
                         'run-tests',
                         'stop-emulators'],
            default_actions=['clobber',
                             'start-emulators',
                             'download-and-extract',
                             'create-virtualenv',
                             'install',
                             'run-tests',
                             'stop-emulators'],
            require_config_file=require_config_file,
            config={
                'virtualenv_modules': self.virtualenv_modules,
                'virtualenv_requirements': self.virtualenv_requirements,
                'require_test_zip': True,
                # IP address of the host as seen from the emulator
                'remote_webserver': '10.0.2.2',
            }
        )

        # these are necessary since self.config is read only
        c = self.config
        abs_dirs = self.query_abs_dirs()
        self.adb_path = self.query_exe('adb')
        self.installer_url = c.get('installer_url')
        self.installer_path = c.get('installer_path')
        self.test_url = c.get('test_url')
        self.test_manifest = c.get('test_manifest')
        self.robocop_url = c.get('robocop_url')
        self.robocop_path = os.path.join(abs_dirs['abs_work_dir'], "robocop.apk")
        self.host_utils_url = c.get('host_utils_url')
        self.minidump_stackwalk_path = c.get("minidump_stackwalk_path")
        self.emulators = c.get('emulators')
        self.test_suite_definitions = c['test_suite_definitions']
        self.test_suites = c.get('test_suites')
        for suite in self.test_suites:
            assert suite in self.test_suite_definitions

    def _query_tests_dir(self, suite_name):
        dirs = self.query_abs_dirs()
        suite_category = self.test_suite_definitions[suite_name]["category"]
        try:
            test_dir = self.config["suite_definitions"][suite_category]["testsdir"]
        except:
            test_dir = suite_category

        return os.path.join(dirs['abs_test_install_dir'], test_dir)

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(AndroidEmulatorTest, self).query_abs_dirs()
        dirs = {}
        dirs['abs_test_install_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'tests')
        dirs['abs_xre_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'hostutils')
        dirs['abs_modules_dir'] = os.path.join(
            dirs['abs_test_install_dir'], 'modules')
        dirs['abs_blob_upload_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'blobber_upload_dir')
        dirs['abs_emulator_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'emulator')

        if self.config.get("developer_mode"):
            dirs['abs_avds_dir'] = os.path.join(
                abs_dirs['abs_work_dir'], "avds_dir")
        else:
            dirs['abs_avds_dir'] = "/home/cltbld/.android"

        for key in dirs.keys():
            if key not in abs_dirs:
                abs_dirs[key] = dirs[key]
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def _build_arg(self, option, value):
        """
        Build a command line argument
        """
        if not value:
            return []
        return [str(option), str(value)]

    def _redirectSUT(self, emulator_index):
        '''
        This redirects the default SUT ports for a given emulator.
        This is needed if more than one emulator is started.
        '''
        emulator = self.emulators[emulator_index]
        emuport = emulator["emulator_port"]
        sutport1 = emulator["sut_port1"]
        sutport2 = emulator["sut_port2"]
        attempts = 0
        tn = None
        redirect_completed = False
        while attempts < 5:
            if attempts == 0:
                self.info("Sleeping 10 seconds")
                time.sleep(10)
            else:
                self.info("Sleeping 30 seconds")
                time.sleep(30)
            attempts += 1
            self.info("  Attempt #%d to redirect ports: (%d, %d, %d)" %
                      (attempts, emuport, sutport1, sutport2))
            try:
                tn = telnetlib.Telnet('localhost', emuport, 300)
                break
            except socket.error, e:
                self.info("Trying again after exception: %s" % str(e))
                pass

        if tn is not None:
            res = tn.read_until('OK')
            if res.find('OK') == -1:
                self.warning('initial OK prompt not received from emulator: ' + str(res))
            tn.write('redir add tcp:' + str(sutport1) + ':' + str(self.config["default_sut_port1"]) + '\n')
            tn.write('redir add tcp:' + str(sutport2) + ':' + str(self.config["default_sut_port2"]) + '\n')
            tn.write('quit\n')
            res = tn.read_all()
            if res.find('OK') == -1:
                self.warning('error adding redirect: ' + str(res))
            else:
                redirect_completed = True
        else:
            self.warning('failed to establish a telnet connection with the emulator')
        return redirect_completed

    def _launch_emulator(self, emulator_index):
        emulator = self.emulators[emulator_index]
        env = self.query_env()

        # Set $LD_LIBRARY_PATH to self.dirs['abs_work_dir'] so that
        # the emulator picks up the symlink to libGL.so.1 that we
        # constructed in start_emulators.
        env['LD_LIBRARY_PATH'] = self.abs_dirs['abs_work_dir']

        # Specifically for developer mode we have to set this variable
        avd_home_dir = self.abs_dirs['abs_avds_dir']
        env['ANDROID_AVD_HOME'] = os.path.join(avd_home_dir, 'avd')

        command = [
            "emulator", "-avd", emulator["name"],
            "-port", str(emulator["emulator_port"]),
        ]
        if "emulator_extra_args" in self.config:
            command += self.config["emulator_extra_args"].split()

        tmp_file = tempfile.NamedTemporaryFile(mode='w')
        tmp_stdout = open(tmp_file.name, 'w')
        self.info("Created temp file %s." % tmp_file.name)
        self.info("Trying to start the emulator with this command: %s" % ' '.join(command))
        proc = subprocess.Popen(command, stdout=tmp_stdout, stderr=tmp_stdout, env=env)
        return {
            "process": proc,
            "tmp_file": tmp_file,
            "tmp_stdout": tmp_stdout
        }

    def _check_emulator(self, emulator):
        self.info('Checking emulator %s' % emulator["name"])

        if self.config["device_manager"] == "sut":
            attempts = 0
            tn = None
            contacted_sut = False
            while attempts < 8 and not contacted_sut:
                if attempts != 0:
                    self.info("Sleeping 30 seconds")
                    time.sleep(30)
                attempts += 1
                self.info("  Attempt #%d to connect to SUT on port %d" %
                          (attempts, emulator["sut_port1"]))
                try:
                    tn = telnetlib.Telnet('localhost', emulator["sut_port1"], 10)
                    if tn is not None:
                        self.info('Connected to port %d' % emulator["sut_port1"])
                        res = tn.read_until('$>', 10)
                        tn.write('quit\n')
                        if res.find('$>') == -1:
                            self.warning('Unexpected SUT response: %s' % res)
                        else:
                            self.info('SUT response: %s' % res)
                            contacted_sut = True
                        tn.read_all()
                    else:
                        self.warning('Unable to connect to the SUT agent on port %d' % emulator["sut_port1"])
                except socket.error, e:
                    self.info('Trying again after socket error: %s' % str(e))
                    pass
                except EOFError:
                    self.info('Trying again after EOF')
                    pass
                except:
                    self.info('Trying again after unexpected exception')
                    pass
                finally:
                    if tn is not None:
                        tn.close()
            if not contacted_sut:
                self.warning('Unable to communicate with SUT agent on port %d' % emulator["sut_port1"])

        attempts = 0
        tn = None
        contacted_emu = False
        while attempts < 4:
            if attempts != 0:
                self.info("Sleeping 30 seconds")
                time.sleep(30)
            attempts += 1
            self.info("  Attempt #%d to connect to emulator on port %d" %
                      (attempts, emulator["emulator_port"]))
            try:
                tn = telnetlib.Telnet('localhost', emulator["emulator_port"], 10)
                if tn is not None:
                    self.info('Connected to port %d' % emulator["emulator_port"])
                    res = tn.read_until('OK', 10)
                    self.info(res)
                    tn.write('avd status\n')
                    res = tn.read_until('OK', 10)
                    self.info('avd status: %s' % res)
                    tn.write('redir list\n')
                    res = tn.read_until('OK', 10)
                    self.info('redir list: %s' % res)
                    tn.write('network status\n')
                    res = tn.read_until('OK', 10)
                    self.info('network status: %s' % res)
                    tn.write('quit\n')
                    tn.read_all()
                    tn.close()
                    contacted_emu = True
                    break
                else:
                    self.warning('Unable to connect to the emulator on port %d' % emulator["emulator_port"])
            except socket.error, e:
                self.info('Trying again after socket error: %s' % str(e))
                pass
            except EOFError:
                self.info('Trying again after EOF')
                pass
            except:
                self.info('Trying again after unexpected exception')
                pass
            finally:
                if tn is not None:
                    tn.close()
        if not contacted_emu:
            self.warning('Unable to communicate with emulator on port %d' % emulator["emulator_port"])

        ps_cmd = [self.adb_path, '-s', emulator["device_id"], 'shell', 'ps']
        p = subprocess.Popen(ps_cmd, stdout=subprocess.PIPE)
        out, err = p.communicate()
        self.info('%s:\n%s\n%s' % (ps_cmd, out, err))

    def _dump_host_state(self):
        p = subprocess.Popen(['ps', '-ef'], stdout=subprocess.PIPE)
        out, err = p.communicate()
        self.info("output from ps -ef follows...")
        if out:
            self.info(out)
        if err:
            self.info(err)
        p = subprocess.Popen(['netstat', '-a', '-p', '-n', '-t', '-u'], stdout=subprocess.PIPE)
        out, err = p.communicate()
        self.info("output from netstat -a -p -n -t -u follows...")
        if out:
            self.info(out)
        if err:
            self.info(err)

    def _dump_emulator_log(self, emulator_index):
        emulator = self.emulators[emulator_index]
        self.info("##### %s emulator log begins" % emulator["name"])
        output = self.read_from_file(self.emulator_procs[emulator_index]["tmp_file"].name, verbose=False)
        if output:
            self.info(output)
        self.info("##### %s emulator log ends" % emulator["name"])

    def _kill_processes(self, process_name):
        p = subprocess.Popen(['ps', '-A'], stdout=subprocess.PIPE)
        out, err = p.communicate()
        self.info("Let's kill every process called %s" % process_name)
        for line in out.splitlines():
            if process_name in line:
                pid = int(line.split(None, 1)[0])
                self.info("Killing pid %d." % pid)
                os.kill(pid, signal.SIGKILL)

    @PostScriptRun
    def _post_script(self):
        self._kill_processes(self.config["emulator_process_name"])

    # XXX: This and android_panda.py's function might make sense to take higher up
    def _download_robocop_apk(self):
        dirs = self.query_abs_dirs()
        self.apk_url = self.installer_url[:self.installer_url.rfind('/')]
        robocop_url = self.apk_url + '/robocop.apk'
        self.info("Downloading robocop...")
        self.download_file(robocop_url, 'robocop.apk', dirs['abs_work_dir'], error_level=FATAL)

    def _query_package_name(self):
        if self.app_name is None:
            #find appname from package-name.txt - assumes download-and-extract has completed successfully
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

    def _build_command(self, emulator, suite_name):
        c = self.config
        dirs = self.query_abs_dirs()
        suite_category = self.test_suite_definitions[suite_name]["category"]

        if suite_category not in c["suite_definitions"]:
            self.fatal("Key '%s' not defined in the config!" % suite_category)

        cmd = [
            self.query_python_path('python'),
            '-u',
            os.path.join(
                self._query_tests_dir(suite_name),
                c["suite_definitions"][suite_category]["run_filename"]
            ),
        ]

        raw_log_file = os.path.join(dirs['abs_blob_upload_dir'],
                                    '%s_raw.log' % suite_name)
        error_summary_file = os.path.join(dirs['abs_blob_upload_dir'],
                                          '%s_errorsummary.log' % suite_name)
        str_format_values = {
            'app': self._query_package_name(),
            'remote_webserver': c['remote_webserver'],
            'xre_path': os.path.join(dirs['abs_xre_dir'], 'xre'),
            'utility_path':  os.path.join(dirs['abs_xre_dir'], 'bin'),
            'http_port': emulator['http_port'],
            'ssl_port': emulator['ssl_port'],
            'certs_path': os.path.join(dirs['abs_work_dir'], 'tests/certs'),
            # TestingMixin._download_and_extract_symbols() will set
            # self.symbols_path when downloading/extracting.
            'symbols_path': self.symbols_path,
            'modules_dir': dirs['abs_modules_dir'],
            'installer_path': self.installer_path,
            'raw_log_file': raw_log_file,
            'error_summary_file': error_summary_file,
            'dm_trans': c['device_manager'],
        }
        if self.config["device_manager"] == "sut":
            str_format_values.update({
                'device_ip': c['device_ip'],
                'device_port': str(emulator['sut_port1']),
            })
        for option in c["suite_definitions"][suite_category]["options"]:
            cmd.extend([option % str_format_values])

        for arg in self.test_suite_definitions[suite_name]["extra_args"]:
            argname = arg.split('=')[0]
            # only add the extra arg if it wasn't already defined by in-tree configs
            if any(a.split('=')[0] == argname for a in cmd):
                continue
            cmd.append(arg)

        try_options, try_tests = self.try_args(suite_category)
        cmd.extend(try_options)
        cmd.extend(self.query_tests_args(
            self.config["suite_definitions"][suite_category].get("tests"),
            self.test_suite_definitions[suite_name].get("tests"),
            try_tests))

        return cmd

    def preflight_run_tests(self):
        super(AndroidEmulatorTest, self).preflight_run_tests()

        if not os.path.isfile(self.adb_path):
            self.fatal("The adb binary '%s' is not a valid file!" % self.adb_path)

    def _trigger_test(self, suite_name, emulator_index):
        """
        Run a test suite on an emulator

        We return a dictionary with the following information:
         - subprocess object that is running the test on the emulator
         - the filename where the stdout is going to
         - the stdout where the output is going to
         - the suite name that is associated
        """
        dirs = self.query_abs_dirs()
        cmd = self._build_command(self.emulators[emulator_index], suite_name)

        try:
            cwd = self._query_tests_dir(suite_name)
        except:
            self.fatal("Don't know how to run --test-suite '%s'!" % suite_name)

        env = self.query_env()
        if self.query_minidump_stackwalk():
            env['MINIDUMP_STACKWALK'] = self.minidump_stackwalk_path
        env['MOZ_UPLOAD_DIR'] = self.query_abs_dirs()['abs_blob_upload_dir']
        env['MINIDUMP_SAVE_PATH'] = self.query_abs_dirs()['abs_blob_upload_dir']

        self.info("Running on %s the command %s" % (self.emulators[emulator_index]["name"], subprocess.list2cmdline(cmd)))
        tmp_file = tempfile.NamedTemporaryFile(mode='w')
        tmp_stdout = open(tmp_file.name, 'w')
        self.info("Created temp file %s." % tmp_file.name)
        return {
            "process": subprocess.Popen(cmd, cwd=cwd, stdout=tmp_stdout, stderr=tmp_stdout, env=env),
            "tmp_file": tmp_file,
            "tmp_stdout": tmp_stdout,
            "suite_name": suite_name,
            "emulator_index": emulator_index
        }

    def _tooltool_fetch(self, url):
        c = self.config
        dirs = self.query_abs_dirs()

        manifest_path = self.download_file(
            url,
            file_name='releng.manifest',
            parent_dir=dirs['abs_avds_dir']
        )

        if not os.path.exists(manifest_path):
            self.fatal("Could not retrieve manifest needed to retrieve avds "
                       "artifacts from %s" % manifest_path)

        self.tooltool_fetch(manifest_path,
                            output_dir=dirs['abs_avds_dir'],
                            cache=c.get("tooltool_cache", None))

    ##########################################
    ### Actions for AndroidEmulatorTest ###
    ##########################################
    def setup_avds(self):
        '''
        If tooltool cache mechanism is enabled, the cached version is used by
        the fetch command. If the manifest includes an "unpack" field, tooltool
        will unpack all compressed archives mentioned in the manifest.
        '''
        c = self.config
        dirs = self.query_abs_dirs()

        # FIXME
        # Clobbering and re-unpacking would not be needed if we had a way to
        # check whether the unpacked content already present match the
        # contents of the tar ball
        self.rmtree(dirs['abs_avds_dir'])
        self.mkdir_p(dirs['abs_avds_dir'])
        if not self.buildbot_config:
            # XXX until we figure out how to determine the repo_path, revision
            url = 'https://hg.mozilla.org/%s/raw-file/%s/%s' % (
                "try", "default", c["tooltool_manifest_path"])
            self._tooltool_fetch(url)
        elif self.buildbot_config and 'properties' in self.buildbot_config:
            url = 'https://hg.mozilla.org/%s/raw-file/%s/%s' % (
                self.buildbot_config['properties']['repo_path'],
                self.buildbot_config['properties']['revision'],
                c["tooltool_manifest_path"])
            self._tooltool_fetch(url)
        else:
            self.fatal("properties in self.buildbot_config are required to "
                       "retrieve tooltool manifest to be used for avds setup")

        if self.config.get("developer_mode"):
            # For developer mode we have to modify the downloaded avds to
            # point to the right directory.
            avd_home_dir = self.abs_dirs['abs_avds_dir']
            cmd = [
                'bash', '-c',
                'sed -i "s|/home/cltbld/.android|%s|" %s/test-*.ini' %
                (avd_home_dir, os.path.join(avd_home_dir, 'avd'))
            ]
            proc = ProcessHandler(cmd)
            proc.run()
            proc.wait()


    def start_emulators(self):
        '''
        This action starts the emulators and redirects the two SUT ports for each one of them
        '''
        assert len(self.test_suites) <= len(self.emulators), \
            "We can't run more tests that the number of emulators we start"

        if 'emulator_url' in self.config or 'emulator_manifest' in self.config or 'tools_manifest' in self.config:
            self.install_emulator()

        if not self.config.get("developer_mode"):
            # We kill compiz because it sometimes prevents us from starting the emulators
            self._kill_processes("compiz")
            self._kill_processes("xpcshell")

        # We add a symlink for libGL.so because the emulator dlopen()s it by that name
        # even though the installed library on most systems without dev packages is
        # libGL.so.1
        linkfile = os.path.join(self.abs_dirs['abs_work_dir'], "libGL.so")
        self.info("Attempting to establish symlink for %s" % linkfile)
        try:
            os.unlink(linkfile)
        except OSError:
            pass
        for libdir in ["/usr/lib/x86_64-linux-gnu/mesa",
                       "/usr/lib/i386-linux-gnu/mesa",
                       "/usr/lib/mesa"]:
            libfile = os.path.join(libdir, "libGL.so.1")
            if os.path.exists(libfile):
                self.info("Symlinking %s -> %s" % (linkfile, libfile))
                self.mkdir_p(self.abs_dirs['abs_work_dir'])
                os.symlink(libfile, linkfile)
                break

        attempts = 0
        redirect_failed = True
        # Launch the required emulators and redirect the SUT ports for each. If unable
        # to redirect the SUT ports, kill the emulators and try starting them again.
        # The wait-and-retry logic is necessary because the emulators intermittently fail
        # to respond to telnet connections immediately after startup: bug 949740. In this
        # case, the emulator log shows "ioctl(KVM_CREATE_VM) failed: Interrupted system call".
        # We do not know how to avoid this error and the only way we have found to
        # recover is to kill the emulator and start again.
        while attempts < 3 and redirect_failed:
            if attempts > 0:
                self.info("Sleeping 30 seconds before retry")
                time.sleep(30)
            attempts += 1
            self.info('Attempt #%d to launch emulators...' % attempts)
            self._dump_host_state()
            self.emulator_procs = []
            emulator_index = 0
            redirect_failed = False
            for test in self.test_suites:
                emulator_proc = self._launch_emulator(emulator_index)
                self.emulator_procs.append(emulator_proc)
                if self.config["device_manager"] == "sut":
                    if self._redirectSUT(emulator_index):
                        emulator = self.emulators[emulator_index]
                        self.info("%s: %s; sut port: %s/%s" %
                                  (emulator["name"], emulator["emulator_port"], emulator["sut_port1"], emulator["sut_port2"]))
                        emulator_index += 1
                    else:
                        self._dump_emulator_log(emulator_index)
                        self._kill_processes(self.config["emulator_process_name"])
                        redirect_failed = True
                        break
        if redirect_failed:
            self.fatal('We have not been able to establish a telnet connection with the emulator')

        # Verify that we can communicate with each emulator
        emulator_index = 0
        for test in self.test_suites:
            emulator = self.emulators[emulator_index]
            emulator_index += 1
            self._check_emulator(emulator)
        # Start logcat for each emulator. Each adb process runs until the
        # corresponding emulator is killed. Output is written directly to
        # the blobber upload directory so that it is uploaded automatically
        # at the end of the job.
        self.mkdir_p(self.abs_dirs['abs_blob_upload_dir'])
        emulator_index = 0
        for test in self.test_suites:
            emulator = self.emulators[emulator_index]
            emulator_index += 1
            logcat_filename = 'logcat-%s.log' % emulator["device_id"]
            logcat_path = os.path.join(self.abs_dirs['abs_blob_upload_dir'], logcat_filename)
            logcat_cmd = '%s -s %s logcat -v time Trace:S StrictMode:S ExchangeService:S > %s &' % \
                (self.adb_path, emulator["device_id"], logcat_path)
            self.info(logcat_cmd)
            os.system(logcat_cmd)
        # Create the /data/anr directory on each emulator image.
        emulator_index = 0
        for test in self.test_suites:
            emulator = self.emulators[emulator_index]
            emulator_index += 1
            mkdir_cmd = [self.adb_path, '-s', emulator["device_id"], 'shell', 'mkdir', '/data/anr']
            p = subprocess.Popen(mkdir_cmd, stdout=subprocess.PIPE)
            out, err = p.communicate()
            self.info('%s:\n%s\n%s' % (mkdir_cmd, out, err))

    def download_and_extract(self):
        # This will download and extract the fennec.apk and tests.zip
        suite_categories = set([self.test_suite_definitions[c]['category']
                                for c in self.test_suites])
        super(AndroidEmulatorTest, self).download_and_extract(suite_categories=suite_categories)
        dirs = self.query_abs_dirs()

        for suite_name in self.test_suites:
            if suite_name.startswith('robocop'):
                self._download_robocop_apk()
                break

        self.mkdir_p(dirs['abs_xre_dir'])
        self._download_unzip(self.host_utils_url, dirs['abs_xre_dir'])

    def install(self):
        assert self.installer_path is not None, \
            "Either add installer_path to the config or use --installer-path."

        emulator_index = 0
        for suite_name in self.test_suites:
            emulator = self.emulators[emulator_index]
            emulator_index += 1

            config = {
                'device-id': emulator["device_id"],
                'enable_automation': True,
                'device_package_name': self._query_package_name()
            }
            config = dict(config.items() + self.config.items())

            self.info("Creating ADBDevicHandler for %s with config %s" % (emulator["name"], config))
            dh = ADBDeviceHandler(config=config, log_obj=self.log_obj, script_obj=self)
            dh.device_id = emulator['device_id']

            # Wait for Android to finish booting
            completed = None
            retries = 0
            while retries < 30:
                completed = self.get_output_from_command([self.adb_path,
                    "-s", emulator['device_id'], "shell",
                    "getprop", "sys.boot_completed"])
                if completed == '1':
                    break
                time.sleep(10)
                retries = retries + 1
            if completed != '1':
                self.warning('Retries exhausted waiting for Android boot.')

            # Install Fennec
            self.info("Installing Fennec for %s" % emulator["name"])
            dh.install_app(self.installer_path)

            # Install the robocop apk if required
            if suite_name.startswith('robocop'):
                self.info("Installing Robocop for %s" % emulator["name"])
                config['device_package_name'] = self.config["robocop_package_name"]
                dh.install_app(self.robocop_path)

            self.info("Finished installing apps for %s" % emulator["name"])

    def run_tests(self):
        """
        Run the tests
        """
        procs = []

        emulator_index = 0
        for suite_name in self.test_suites:
            procs.append(self._trigger_test(suite_name, emulator_index))
            emulator_index += 1

        joint_tbpl_status = None
        joint_log_level = None
        start_time = int(time.time())
        while True:
            for p in procs:
                emulator_index = p["emulator_index"]
                return_code = p["process"].poll()
                if return_code is not None:
                    suite_name = p["suite_name"]
                    # To make reading the log of the suite not mix with the previous line
                    sys.stdout.write('\n')
                    self.info("##### %s log begins" % p["suite_name"])
                    # Let's close the stdout
                    p["tmp_stdout"].close()
                    # Let's read the file that now has the output
                    output = self.read_from_file(p["tmp_file"].name, verbose=False)
                    # Let's parse the output (which also prints it)
                    # and determine what the results should be
                    parser = self.get_test_output_parser(
                        self.test_suite_definitions[p["suite_name"]]["category"],
                        config=self.config,
                        log_obj=self.log_obj,
                        error_list=self.error_list)
                    for line in output.splitlines():
                        parser.parse_single_line(line)

                    # After parsing each line we should know what the summary for this suite should be
                    tbpl_status, log_level = parser.evaluate_parser(0)
                    parser.append_tinderboxprint_line(p["suite_name"])
                    # After running all jobs we will report the worst status of all emulator runs
                    joint_tbpl_status = self.worst_level(tbpl_status, joint_tbpl_status, TBPL_WORST_LEVEL_TUPLE)
                    joint_log_level = self.worst_level(log_level, joint_log_level)

                    self.info("##### %s log ends" % p["suite_name"])
                    self._dump_emulator_log(emulator_index)
                    procs.remove(p)
            if procs == []:
                break
            else:
                # Every 5 minutes let's print something to stdout
                # so buildbot won't kill the process due to lack of output
                if int(time.time()) - start_time > 5 * 60:
                    self.info('#')
                    start_time = int(time.time())
                time.sleep(30)

        self.buildbot_status(joint_tbpl_status, level=joint_log_level)

    def stop_emulators(self):
        '''
        Report emulator health, then make sure that every emulator has been stopped
        '''
        emulator_index = 0
        for test in self.test_suites:
            emulator = self.emulators[emulator_index]
            emulator_index += 1
            self._check_emulator(emulator)
        self._kill_processes(self.config["emulator_process_name"])

if __name__ == '__main__':
    emulatorTest = AndroidEmulatorTest()
    emulatorTest.run_and_exit()
