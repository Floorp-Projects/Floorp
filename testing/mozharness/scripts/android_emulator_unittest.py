#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import copy
import datetime
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
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options
from mozharness.mozilla.testing.unittest import EmulatorMixin


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
        {"action": "store",
         "dest": "test_suite",
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
                         'start-emulator',
                         'download-and-extract',
                         'create-virtualenv',
                         'verify-emulator',
                         'install',
                         'run-tests',
                         'stop-emulator'],
            default_actions=['clobber',
                             'start-emulator',
                             'download-and-extract',
                             'create-virtualenv',
                             'verify-emulator',
                             'install',
                             'run-tests',
                             'stop-emulator'],
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
        self.emulator = c.get('emulator')
        self.test_suite_definitions = c['test_suite_definitions']
        self.test_suite = c.get('test_suite')
        assert self.test_suite in self.test_suite_definitions

    def _query_tests_dir(self):
        dirs = self.query_abs_dirs()
        suite_category = self.test_suite_definitions[self.test_suite]["category"]
        try:
            test_dir = self.tree_config["suite_definitions"][suite_category]["testsdir"]
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

    def _launch_emulator(self):
        env = self.query_env()

        # Set $LD_LIBRARY_PATH to self.dirs['abs_work_dir'] so that
        # the emulator picks up the symlink to libGL.so.1 that we
        # constructed in start_emulator.
        env['LD_LIBRARY_PATH'] = self.abs_dirs['abs_work_dir']

        # Specifically for developer mode we have to set this variable
        avd_home_dir = self.abs_dirs['abs_avds_dir']
        env['ANDROID_AVD_HOME'] = os.path.join(avd_home_dir, 'avd')

        command = [
            "emulator", "-avd", self.emulator["name"],
            "-port", str(self.emulator["emulator_port"]),
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
        }

    def _retry(self, max_attempts, interval, func, description, max_time = 0):
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
            end_time = datetime.datetime.now() + datetime.timedelta(seconds = max_time)
        else:
            end_time = None
        while attempts < max_attempts and not status:
            if (end_time is not None) and (datetime.datetime.now() > end_time):
                self.info("Maximum retry run-time of %d seconds exceeded; remaining attempts abandoned" % max_time)
                break
            if attempts != 0:
                self.info("Sleeping %d seconds" % interval)
                time.sleep(interval)
            attempts += 1
            self.info(">> %s: Attempt #%d of %d" % (description, attempts, max_attempts))
            status = func()
        return status

    def _run_with_timeout(self, timeout, cmd):
        timeout_cmd = ['timeout', '%s' % timeout] + cmd
        return self._run_proc(timeout_cmd)

    def _run_proc(self, cmd):
        self.info('Running %s' % subprocess.list2cmdline(cmd))
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
        out, err = p.communicate()
        if out:
            self.info('%s' % str(out.strip()))
        if err:
            self.info('stderr: %s' % str(err.strip()))
        return out

    def _telnet_cmd(self, telnet, command):
        telnet.write('%s\n' % command)
        result = telnet.read_until('OK', 10)
        self.info('%s: %s' % (command, result))
        return result

    def _verify_adb(self):
        self.info('Verifying adb connectivity')
        self._run_with_timeout(300, [self.adb_path, 'wait-for-device'])
        return True

    def _verify_adb_device(self):
        out = self._run_with_timeout(30, [self.adb_path, 'devices'])
        if (self.emulator['device_id'] in out) and ("device" in out):
            return True
        return False

    def _is_boot_completed(self):
        boot_cmd = [self.adb_path, '-s', self.emulator['device_id'],
                    'shell', 'getprop', 'sys.boot_completed']
        out = self._run_with_timeout(30, boot_cmd)
        if out.strip() == '1':
            return True
        return False

    def _telnet_to_emulator(self):
        port = self.emulator["emulator_port"]
        telnet_ok = False
        try:
            tn = telnetlib.Telnet('localhost', port, 10)
            if tn is not None:
                self.info('Connected to port %d' % port)
                res = tn.read_until('OK', 10)
                self.info(res)
                self._telnet_cmd(tn, 'avd status')
                if self.config["device_manager"] == "sut":
                    sutport1 = self.emulator["sut_port1"]
                    sutport2 = self.emulator["sut_port2"]
                    cmd = 'redir add tcp:' + str(sutport1) + ':' + str(self.config["default_sut_port1"])
                    self._telnet_cmd(tn, cmd)
                    cmd = 'redir add tcp:' + str(sutport2) + ':' + str(self.config["default_sut_port2"])
                    self._telnet_cmd(tn, cmd)
                self._telnet_cmd(tn, 'redir list')
                self._telnet_cmd(tn, 'network status')
                tn.write('quit\n')
                tn.read_all()
                telnet_ok = True
            else:
                self.warning('Unable to connect to port %d' % port)
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
        return telnet_ok

    def _telnet_to_sut(self):
        port = self.emulator["sut_port1"]
        contacted_sut = False
        try:
            tn = telnetlib.Telnet('localhost', port, 10)
            if tn is not None:
                self.info('Connected to port %d' % port)
                res = tn.read_until('$>', 10)
                if res.find('$>') == -1:
                    self.warning('Unexpected SUT response: %s' % res)
                else:
                    self.info('SUT response: %s' % res)
                    contacted_sut = True
                tn.write('quit\n')
                tn.read_all()
            else:
                self.warning('Unable to connect to port %d' % port)
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
        return contacted_sut

    def _verify_emulator(self):
        adb_ok = self._verify_adb()
        if not adb_ok:
            self.warning('Unable to communicate with adb')
            return False
        adb_device_ok = self._retry(4, 30, self._verify_adb_device, "Verify emulator visible to adb")
        if not adb_device_ok:
            self.warning('Unable to communicate with emulator via adb')
            return False
        boot_ok = self._retry(30, 10, self._is_boot_completed, "Verify Android boot completed", max_time = 330)
        if not boot_ok:
            self.warning('Unable to verify Android boot completion')
            return False
        telnet_ok = self._retry(4, 30, self._telnet_to_emulator, "Verify telnet to emulator")
        if not telnet_ok:
            self.warning('Unable to telnet to emulator on port %d' % self.emulator["emulator_port"])
            return False
        if self.config["device_manager"] == "sut":
            sut_ok = self._retry(8, 30, self._telnet_to_sut, "Verify telnet to SUT agent")
            if not sut_ok:
                self.warning('Unable to communicate with SUT agent on port %d' % self.emulator["sut_port1"])
                return False
        return True

    def _verify_emulator_and_restart_on_fail(self):
        emulator_ok = self._verify_emulator()
        self._dump_host_state()
        self._screenshot("emulator-startup-screenshot-")
        if not emulator_ok:
            self._kill_processes(self.config["emulator_process_name"])
            self._dump_emulator_log()
            self._restart_adbd()
            self.emulator_proc = self._launch_emulator()
        return emulator_ok

    def _install_fennec_apk(self):
        install_ok = False
        cmd = [self.adb_path, '-s', self.emulator['device_id'], 'install', '-r', self.installer_path]
        out = self._run_with_timeout(300, cmd)
        if 'Success' in out:
            install_ok = True
        return install_ok

    def _install_robocop_apk(self):
        install_ok = False
        cmd = [self.adb_path, '-s', self.emulator['device_id'], 'install', '-r', self.robocop_path]
        out = self._run_with_timeout(300, cmd)
        if 'Success' in out:
            install_ok = True
        return install_ok

    def _dump_host_state(self):
        self._run_proc(['ps', '-ef'])
        self._run_proc(['netstat', '-a', '-p', '-n', '-t', '-u'])

    def _dump_emulator_log(self):
        self.info("##### %s emulator log begins" % self.emulator["name"])
        output = self.read_from_file(self.emulator_proc["tmp_file"].name, verbose=False)
        if output:
            self.info(output)
        self.info("##### %s emulator log ends" % self.emulator["name"])

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
        self._run_with_timeout(30, [self.adb_path, 'start-server'])

    def _screenshot(self, prefix):
        """
           Save a screenshot of the entire screen to the blob upload directory.
        """
        dirs = self.query_abs_dirs()
        utility = os.path.join(dirs['abs_xre_dir'], "bin", "screentopng")
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

    @PostScriptRun
    def _post_script(self):
        self._kill_processes(self.config["emulator_process_name"])

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

    def _build_command(self):
        c = self.config
        dirs = self.query_abs_dirs()
        suite_category = self.test_suite_definitions[self.test_suite]["category"]

        if suite_category not in self.tree_config["suite_definitions"]:
            self.fatal("Key '%s' not defined in the in-tree config! Please add it to '%s'. "
                       "See bug 981030 for more details." % (suite_category,
                       os.path.join('gecko', 'testing', self.config['in_tree_config'])))
        cmd = [
            self.query_python_path('python'),
            '-u',
            os.path.join(
                self._query_tests_dir(),
                self.tree_config["suite_definitions"][suite_category]["run_filename"]
            ),
        ]

        raw_log_file = os.path.join(dirs['abs_blob_upload_dir'],
                                    '%s_raw.log' % self.test_suite)
        str_format_values = {
            'app': self._query_package_name(),
            'remote_webserver': c['remote_webserver'],
            'xre_path': os.path.join(dirs['abs_xre_dir'], 'xre'),
            'utility_path':  os.path.join(dirs['abs_xre_dir'], 'bin'),
            'http_port': self.emulator['http_port'],
            'ssl_port': self.emulator['ssl_port'],
            'certs_path': os.path.join(dirs['abs_work_dir'], 'tests/certs'),
            # TestingMixin._download_and_extract_symbols() will set
            # self.symbols_path when downloading/extracting.
            'symbols_path': self.symbols_path,
            'modules_dir': dirs['abs_modules_dir'],
            'installer_path': self.installer_path,
            'raw_log_file': raw_log_file,
            'dm_trans': c['device_manager'],
        }
        if self.config["device_manager"] == "sut":
            str_format_values.update({
                'device_ip': c['device_ip'],
                'device_port': str(self.emulator['sut_port1']),
            })
        for option in self.tree_config["suite_definitions"][suite_category]["options"]:
            cmd.extend([option % str_format_values])

        for arg in self.test_suite_definitions[self.test_suite]["extra_args"]:
            argname = arg.split('=')[0]
            # only add the extra arg if it wasn't already defined by in-tree configs
            if any(a.split('=')[0] == argname for a in cmd):
                continue
            cmd.append(arg)

        return cmd

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

    def start_emulator(self):
        '''
        Starts the emulator
        '''
        if 'emulator_url' in self.config or 'emulator_manifest' in self.config or 'tools_manifest' in self.config:
            self.install_emulator()

        if not os.path.isfile(self.adb_path):
            self.fatal("The adb binary '%s' is not a valid file!" % self.adb_path)
        self._restart_adbd()

        if not self.config.get("developer_mode"):
            # We kill compiz because it sometimes prevents us from starting the emulator
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
        self.emulator_proc = self._launch_emulator()

    def verify_emulator(self):
        '''
        Check to see if the emulator can be contacted via adb, telnet, and sut, if configured. 
        If any communication attempt fails, kill the emulator, re-launch, and re-check.
        '''
        self.mkdir_p(self.query_abs_dirs()['abs_blob_upload_dir'])
        max_restarts = 3
        emulator_ok = self._retry(max_restarts, 30, self._verify_emulator_and_restart_on_fail, "Check emulator")
        if not emulator_ok:
            self.fatal('Unable to start emulator after %d attempts' % max_restarts)
        # Start logcat for the emulator. The adb process runs until the
        # corresponding emulator is killed. Output is written directly to
        # the blobber upload directory so that it is uploaded automatically
        # at the end of the job.
        logcat_filename = 'logcat-%s.log' % self.emulator["device_id"]
        logcat_path = os.path.join(self.abs_dirs['abs_blob_upload_dir'], logcat_filename)
        logcat_cmd = '%s -s %s logcat -v threadtime Trace:S StrictMode:S ExchangeService:S > %s &' % \
            (self.adb_path, self.emulator["device_id"], logcat_path)
        self.info(logcat_cmd)
        os.system(logcat_cmd)
        # Get a post-boot emulator process list for diagnostics
        ps_cmd = [self.adb_path, '-s', self.emulator["device_id"], 'shell', 'ps']
        self._run_with_timeout(30, ps_cmd)

    def download_and_extract(self):
        """
        Download and extract fennec APK, tests.zip, host utils, and robocop (if required).
        """
        suite_category = self.test_suite_definitions[self.test_suite]['category']
        super(AndroidEmulatorTest, self).download_and_extract(suite_categories=[suite_category])
        dirs = self.query_abs_dirs()
        if self.test_suite.startswith('robocop'):
            robocop_url = self.installer_url[:self.installer_url.rfind('/')] + '/robocop.apk'
            self.info("Downloading robocop...")
            self.download_file(robocop_url, 'robocop.apk', dirs['abs_work_dir'], error_level=FATAL)
        self.mkdir_p(dirs['abs_xre_dir'])
        self._download_unzip(self.host_utils_url, dirs['abs_xre_dir'])

    def install(self):
        """
        Install APKs on the emulator
        """
        assert self.installer_path is not None, \
            "Either add installer_path to the config or use --installer-path."

        config = {
            'device-id': self.emulator["device_id"],
            'enable_automation': True,
            'device_package_name': self._query_package_name()
        }
        config = dict(config.items() + self.config.items())

        # Install Fennec
        install_ok = self._retry(3, 30, self._install_fennec_apk, "Install Fennec APK")
        if not install_ok:
            self.fatal('Failed to install %s on %s' % (self.installer_path, self.emulator["name"]))

        # Install Robocop if required
        if self.test_suite.startswith('robocop'):
            install_ok = self._retry(3, 30, self._install_robocop_apk, "Install Robocop APK")
            if not install_ok:
                self.fatal('Failed to install %s on %s' % (self.robocop_path, self.emulator["name"]))

        self.info("Finished installing apps for %s" % self.emulator["name"])

    def run_tests(self):
        """
        Run the tests
        """
        cmd = self._build_command()
        cmd = self.append_harness_extra_args(cmd)
        try:
            cwd = self._query_tests_dir()
        except:
            self.fatal("Don't know how to run --test-suite '%s'!" % self.test_suite)
        env = self.query_env()
        if self.query_minidump_stackwalk():
            env['MINIDUMP_STACKWALK'] = self.minidump_stackwalk_path
        env['MOZ_UPLOAD_DIR'] = self.query_abs_dirs()['abs_blob_upload_dir']
        env['MINIDUMP_SAVE_PATH'] = self.query_abs_dirs()['abs_blob_upload_dir']

        self.info("Running on %s the command %s" % (self.emulator["name"], subprocess.list2cmdline(cmd)))
        self.info("##### %s log begins" % self.test_suite)

        parser = self.get_test_output_parser(
            self.test_suite_definitions[self.test_suite]["category"],
            config=self.config,
            log_obj=self.log_obj,
            error_list=self.error_list)
        return_code = self.run_command(cmd,
                                       cwd=cwd,
                                       env=env,
                                       output_parser=parser)
        tbpl_status, log_level = parser.evaluate_parser(0)
        parser.append_tinderboxprint_line(self.test_suite)

        self.info("##### %s log ends" % self.test_suite)
        self._dump_emulator_log()
        self.buildbot_status(tbpl_status, level=log_level)

    def stop_emulator(self):
        '''
        Report emulator health, then make sure that the emulator has been stopped
        '''
        self._verify_emulator()
        self._kill_processes(self.config["emulator_process_name"])

if __name__ == '__main__':
    emulatorTest = AndroidEmulatorTest()
    emulatorTest.run_and_exit()
