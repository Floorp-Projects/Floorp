#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import datetime
import glob
import os
import posixpath
import re
import signal
import subprocess
import time
import tempfile
from threading import Timer
from mozharness.mozilla.automation import TBPL_RETRY, EXIT_STATUS_DICT
from mozharness.base.script import PreScriptAction, PostScriptAction


class AndroidMixin(object):
    """
       Mixin class used by Android test scripts.
    """

    def __init__(self, **kwargs):
        self._adb_path = None
        self._device = None
        self.app_name = None
        self.device_name = os.environ.get('DEVICE_NAME', None)
        self.device_serial = os.environ.get('DEVICE_SERIAL', None)
        self.device_ip = os.environ.get('DEVICE_IP', None)
        self.logcat_proc = None
        self.logcat_file = None
        self.use_gles3 = False
        self.xre_path = None
        super(AndroidMixin, self).__init__(**kwargs)

    @property
    def adb_path(self):
        '''Get the path to the adb executable.

        Defer the use of query_exe() since it is defined by the
        BaseScript Mixin which hasn't finished initialing by the
        time the AndroidMixin is first initialized.
        '''
        if not self._adb_path:
            try:
                self._adb_path = self.query_exe('adb')
            except AttributeError:
                # Ignore attribute errors since BaseScript will
                # attempt to access properties before the other Mixins
                # have completed initialization. We recover afterwards
                # when additional attemps occur after initialization
                # is completed.
                pass
        return self._adb_path

    @property
    def device(self):
        if not self._device and self.adb_path:
            try:
                import mozdevice
                self._device = mozdevice.ADBDevice(adb=self.adb_path,
                                                   device=self.device_serial,
                                                   verbose=True)
                self.info("New mozdevice with adb=%s, device=%s" %
                          (self.adb_path, self.device_serial))
            except AttributeError:
                # As in adb_path, above.
                pass
        return self._device

    @property
    def is_android(self):
        try:
            c = self.config
            installer_url = c.get('installer_url', None)
            return self.device_serial is not None or self.is_emulator or \
                (installer_url is not None and installer_url.endswith(".apk"))
        except AttributeError:
            return False

    @property
    def is_emulator(self):
        try:
            c = self.config
            return True if c.get('emulator_avd_name') else False
        except AttributeError:
            return False

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
        # from TooltoolMixin, included in TestingMixin
        self.tooltool_fetch(manifest_path,
                            output_dir=dir,
                            cache=c.get("tooltool_cache", None))

    def _launch_emulator(self):
        env = self.query_env()

        # Write a default ddms.cfg to avoid unwanted prompts
        avd_home_dir = self.abs_dirs['abs_avds_dir']
        DDMS_FILE = os.path.join(avd_home_dir, "ddms.cfg")
        with open(DDMS_FILE, 'w') as f:
            f.write("pingOptIn=false\npingId=0\n")
        self.info("wrote dummy %s" % DDMS_FILE)

        # Delete emulator auth file, so it doesn't prompt
        AUTH_FILE = os.path.join(os.path.expanduser('~'), '.emulator_console_auth_token')
        if os.path.exists(AUTH_FILE):
            try:
                os.remove(AUTH_FILE)
                self.info("deleted %s" % AUTH_FILE)
            except Exception:
                self.warning("failed to remove %s" % AUTH_FILE)

        avd_path = os.path.join(avd_home_dir, 'avd')
        if os.path.exists(avd_path):
            env['ANDROID_AVD_HOME'] = avd_path
            self.info("Found avds at %s" % avd_path)
        else:
            self.warning("AVDs missing? Not found at %s" % avd_path)

        if "deprecated_sdk_path" in self.config:
            sdk_path = os.path.abspath(os.path.join(avd_home_dir, '..'))
        else:
            sdk_path = self.abs_dirs['abs_sdk_dir']
        if os.path.exists(sdk_path):
            env['ANDROID_SDK_HOME'] = sdk_path
            self.info("Found sdk at %s" % sdk_path)
        else:
            self.warning("Android sdk missing? Not found at %s" % sdk_path)

        if self.use_gles3:
            # enable EGL 3.0 in advancedFeatures.ini
            AF_FILE = os.path.join(sdk_path, "advancedFeatures.ini")
            with open(AF_FILE, 'w') as f:
                f.write("GLESDynamicVersion=on\n")
            self.info("set GLESDynamicVersion=on in %s" % AF_FILE)

        # extra diagnostics for kvm acceleration
        emu = self.config.get('emulator_process_name')
        if os.path.exists('/dev/kvm') and emu and 'x86' in emu:
            try:
                self.run_command(['ls', '-l', '/dev/kvm'])
                self.run_command(['kvm-ok'])
                self.run_command(["emulator", "-accel-check"], env=env)
            except Exception as e:
                self.warning("Extra kvm diagnostics failed: %s" % str(e))

        command = ["emulator", "-avd", self.config["emulator_avd_name"]]
        if "emulator_extra_args" in self.config:
            command += self.config["emulator_extra_args"].split()

        dir = self.query_abs_dirs()['abs_blob_upload_dir']
        tmp_file = tempfile.NamedTemporaryFile(mode='w', prefix='emulator-',
                                               suffix='.log', dir=dir, delete=False)
        self.info("Launching the emulator with: %s" % ' '.join(command))
        self.info("Writing log to %s" % tmp_file.name)
        proc = subprocess.Popen(command, stdout=tmp_file, stderr=tmp_file, env=env, bufsize=0)
        return proc

    def _verify_emulator(self):
        boot_ok = self._retry(30, 10, self.is_boot_completed, "Verify Android boot completed",
                              max_time=330)
        if not boot_ok:
            self.warning('Unable to verify Android boot completion')
            return False
        return True

    def _verify_emulator_and_restart_on_fail(self):
        emulator_ok = self._verify_emulator()
        if not emulator_ok:
            self.device_screenshot("screenshot-emulator-start")
            self.kill_processes(self.config["emulator_process_name"])
            subprocess.check_call(['ps', '-ef'])
            # remove emulator tmp files
            for dir in glob.glob("/tmp/android-*"):
                self.rmtree(dir)
            time.sleep(5)
            self.emulator_proc = self._launch_emulator()
        return emulator_ok

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

    def dump_perf_info(self):
        '''
        Dump some host and android device performance-related information
        to an artifact file, to help understand task performance.
        '''
        dir = self.query_abs_dirs()['abs_blob_upload_dir']
        perf_path = os.path.join(dir, "android-performance.log")
        with open(perf_path, "w") as f:

            f.write('\n\nHost cpufreq/scaling_governor:\n')
            cpus = glob.glob('/sys/devices/system/cpu/cpu*/cpufreq/scaling_governor')
            for cpu in cpus:
                out = subprocess.check_output(['cat', cpu])
                f.write("%s: %s" % (cpu, out))

            f.write('\n\nHost /proc/cpuinfo:\n')
            out = subprocess.check_output(['cat', '/proc/cpuinfo'])
            f.write(out)

            f.write('\n\nHost /proc/meminfo:\n')
            out = subprocess.check_output(['cat', '/proc/meminfo'])
            f.write(out)

            f.write('\n\nHost process list:\n')
            out = subprocess.check_output(['ps', '-ef'])
            f.write(out)

            f.write('\n\nDevice /proc/cpuinfo:\n')
            cmd = 'cat /proc/cpuinfo'
            out = self.shell_output(cmd)
            f.write(out)
            cpuinfo = out

            f.write('\n\nDevice /proc/meminfo:\n')
            cmd = 'cat /proc/meminfo'
            out = self.shell_output(cmd)
            f.write(out)

            f.write('\n\nDevice process list:\n')
            cmd = 'ps'
            out = self.shell_output(cmd)
            f.write(out)

        # Search android cpuinfo for "BogoMIPS"; if found and < (minimum), retry
        # this task, in hopes of getting a higher-powered environment.
        # (Carry on silently if BogoMIPS is not found -- this may vary by
        # Android implementation -- no big deal.)
        # See bug 1321605: Sometimes the emulator is really slow, and
        # low bogomips can be a good predictor of that condition.
        bogomips_minimum = int(self.config.get('bogomips_minimum') or 0)
        for line in cpuinfo.split('\n'):
            m = re.match("BogoMIPS.*: (\d*)", line, re.IGNORECASE)
            if m:
                bogomips = int(m.group(1))
                if bogomips_minimum > 0 and bogomips < bogomips_minimum:
                    self.fatal('INFRA-ERROR: insufficient Android bogomips (%d < %d)' %
                               (bogomips, bogomips_minimum),
                               EXIT_STATUS_DICT[TBPL_RETRY])
                self.info("Found Android bogomips: %d" % bogomips)
                break

    def logcat_path(self):
        logcat_filename = 'logcat-%s.log' % self.device_serial
        return os.path.join(self.query_abs_dirs()['abs_blob_upload_dir'],
                            logcat_filename)

    def logcat_start(self):
        """
           Start recording logcat. Writes logcat to the upload directory.
        """
        # Start logcat for the device. The adb process runs until the
        # corresponding device is stopped. Output is written directly to
        # the blobber upload directory so that it is uploaded automatically
        # at the end of the job.
        self.logcat_file = open(self.logcat_path(), 'w')
        logcat_cmd = [self.adb_path, '-s', self.device_serial, 'logcat', '-v',
                      'threadtime', 'Trace:S', 'StrictMode:S',
                      'ExchangeService:S']
        self.info(' '.join(logcat_cmd))
        self.logcat_proc = subprocess.Popen(logcat_cmd, stdout=self.logcat_file,
                                            stdin=subprocess.PIPE)

    def logcat_stop(self):
        """
           Stop logcat process started by logcat_start.
        """
        if self.logcat_proc:
            self.info("Killing logcat pid %d." % self.logcat_proc.pid)
            self.logcat_proc.kill()
            self.logcat_file.close()

    def install_apk(self, apk, replace=False):
        """
           Install the specified apk.
        """
        import mozdevice
        try:
            self.device.install_app(apk, replace=replace)
        except (mozdevice.ADBError, mozdevice.ADBProcessError, mozdevice.ADBTimeoutError) as e:
            self.info('Failed to install %s on %s: %s %s' %
                      (apk, self.device_name,
                       type(e).__name__, e))
            self.fatal('INFRA-ERROR: %s Failed to install %s' %
                       (type(e).__name__, os.path.basename(apk)),
                       EXIT_STATUS_DICT[TBPL_RETRY])

    def uninstall_apk(self):
        """
           Uninstall the app associated with the configured apk, if it is
           installed.
        """
        import mozdevice
        try:
            package_name = self.query_package_name()
            self.device.uninstall_app(package_name)
        except (mozdevice.ADBError, mozdevice.ADBProcessError, mozdevice.ADBTimeoutError) as e:
            self.info('Failed to uninstall %s from %s: %s %s' %
                      (package_name, self.device_name,
                       type(e).__name__, e))
            self.fatal('INFRA-ERROR: %s Failed to uninstall %s' %
                       (type(e).__name__, package_name),
                       EXIT_STATUS_DICT[TBPL_RETRY])

    def is_boot_completed(self):
        import mozdevice
        try:
            out = self.device.get_prop('sys.boot_completed', timeout=30)
            if out.strip() == '1':
                return True
        except (ValueError, mozdevice.ADBError, mozdevice.ADBTimeoutError):
            pass
        return False

    def shell_output(self, cmd):
        import mozdevice
        try:
            return self.device.shell_output(cmd, timeout=30)
        except (mozdevice.ADBTimeoutError) as e:
            self.info('Failed to run shell command %s from %s: %s %s' %
                      (cmd, self.device_name,
                       type(e).__name__, e))
            self.fatal('INFRA-ERROR: %s Failed to run shell command %s' %
                       (type(e).__name__, cmd),
                       EXIT_STATUS_DICT[TBPL_RETRY])

    def device_screenshot(self, prefix):
        """
           On emulator, save a screenshot of the entire screen to the upload directory;
           otherwise, save a screenshot of the device to the upload directory.

           :param prefix specifies a filename prefix for the screenshot
        """
        from mozscreenshot import dump_screen, dump_device_screen
        reset_dir = False
        if not os.environ.get("MOZ_UPLOAD_DIR", None):
            dirs = self.query_abs_dirs()
            os.environ["MOZ_UPLOAD_DIR"] = dirs['abs_blob_upload_dir']
            reset_dir = True
        if self.is_emulator:
            if self.xre_path:
                dump_screen(self.xre_path, self, prefix=prefix)
            else:
                self.info('Not saving screenshot: no XRE configured')
        else:
            dump_device_screen(self.device, self, prefix=prefix)
        if reset_dir:
            del os.environ["MOZ_UPLOAD_DIR"]

    def download_hostutils(self, xre_dir):
        """
           Download and install hostutils from tooltool.
        """
        xre_path = None
        self.rmtree(xre_dir)
        self.mkdir_p(xre_dir)
        if self.config["hostutils_manifest_path"]:
            url = self._get_repo_url(self.config["hostutils_manifest_path"])
            self._tooltool_fetch(url, xre_dir)
            for p in glob.glob(os.path.join(xre_dir, 'host-utils-*')):
                if os.path.isdir(p) and os.path.isfile(os.path.join(p, 'xpcshell')):
                    xre_path = p
            if not xre_path:
                self.fatal("xre path not found in %s" % xre_dir)
        else:
            self.fatal("configure hostutils_manifest_path!")
        return xre_path

    def query_package_name(self):
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
            unzip_cmd = [unzip, '-q', '-o', self.apk_path]
            self.run_command(unzip_cmd, cwd=apk_dir, halt_on_failure=True)
            self.app_name = str(self.read_from_file(package_path, verbose=True)).rstrip()
        return self.app_name

    def kill_processes(self, process_name):
        self.info("Killing every process called %s" % process_name)
        out = subprocess.check_output(['ps', '-A'])
        for line in out.splitlines():
            if process_name in line:
                pid = int(line.split(None, 1)[0])
                self.info("Killing pid %d." % pid)
                os.kill(pid, signal.SIGKILL)

    def delete_ANRs(self):
        remote_dir = self.device.stack_trace_dir
        try:
            if not self.device.is_dir(remote_dir, root=True):
                self.mkdir(remote_dir, root=True)
                self.chmod(remote_dir, root=True)
                self.info("%s created" % remote_dir)
                return
            for trace_file in self.device.ls(remote_dir, root=True):
                trace_path = posixpath.join(remote_dir, trace_file)
                self.device.chmod(trace_path, root=True)
                self.device.rm(trace_path, root=True)
                self.info("%s deleted" % trace_path)
        except Exception as e:
            self.info("failed to delete %s: %s %s" % (remote_dir, type(e).__name__, str(e)))

    def check_for_ANRs(self):
        """
        Copy ANR (stack trace) files from device to upload directory.
        """
        dirs = self.query_abs_dirs()
        remote_dir = self.device.stack_trace_dir
        try:
            if not self.device.is_dir(remote_dir):
                self.info("%s not found; ANR check skipped" % remote_dir)
                return
            self.device.chmod(remote_dir, recursive=True, root=True)
            self.device.pull(remote_dir, dirs['abs_blob_upload_dir'])
            self.delete_ANRs()
        except Exception as e:
            self.info("failed to pull %s: %s %s" % (remote_dir, type(e).__name__, str(e)))

    def delete_tombstones(self):
        remote_dir = "/data/tombstones"
        try:
            if not self.device.is_dir(remote_dir, root=True):
                self.mkdir(remote_dir, root=True)
                self.chmod(remote_dir, root=True)
                self.info("%s created" % remote_dir)
                return
            for trace_file in self.device.ls(remote_dir, root=True):
                trace_path = posixpath.join(remote_dir, trace_file)
                self.device.chmod(trace_path, root=True)
                self.device.rm(trace_path, root=True)
                self.info("%s deleted" % trace_path)
        except Exception as e:
            self.info("failed to delete %s: %s %s" % (remote_dir, type(e).__name__, str(e)))

    def check_for_tombstones(self):
        """
        Copy tombstone files from device to upload directory.
        """
        dirs = self.query_abs_dirs()
        remote_dir = "/data/tombstones"
        try:
            if not self.device.is_dir(remote_dir):
                self.info("%s not found; tombstone check skipped" % remote_dir)
                return
            self.device.chmod(remote_dir, recursive=True, root=True)
            self.device.pull(remote_dir, dirs['abs_blob_upload_dir'])
            self.delete_tombstones()
        except Exception as e:
            self.info("failed to pull %s: %s %s" % (remote_dir, type(e).__name__, str(e)))

    # Script actions

    def setup_avds(self):
        """
        If tooltool cache mechanism is enabled, the cached version is used by
        the fetch command. If the manifest includes an "unpack" field, tooltool
        will unpack all compressed archives mentioned in the manifest.
        """
        if not self.is_emulator:
            return

        c = self.config
        dirs = self.query_abs_dirs()
        self.mkdir_p(dirs['abs_work_dir'])
        self.mkdir_p(dirs['abs_blob_upload_dir'])

        # Always start with a clean AVD: AVD includes Android images
        # which can be stateful.
        self.rmtree(dirs['abs_avds_dir'])
        self.mkdir_p(dirs['abs_avds_dir'])
        if 'avd_url' in c:
            # Intended for experimental setups to evaluate an avd prior to
            # tooltool deployment.
            url = c['avd_url']
            self.download_unpack(url, dirs['abs_avds_dir'])
        else:
            url = self._get_repo_url(c["tooltool_manifest_path"])
            self._tooltool_fetch(url, dirs['abs_avds_dir'])

        avd_home_dir = self.abs_dirs['abs_avds_dir']
        if avd_home_dir != "/home/cltbld/.android":
            # Modify the downloaded avds to point to the right directory.
            cmd = [
                'bash', '-c',
                'sed -i "s|/home/cltbld/.android|%s|" %s/test-*.ini' %
                (avd_home_dir, os.path.join(avd_home_dir, 'avd'))
            ]
            subprocess.check_call(cmd)

    def start_emulator(self):
        """
        Starts the emulator
        """
        if not self.is_emulator:
            return

        if 'emulator_url' in self.config or 'emulator_manifest' in self.config:
            dirs = self.query_abs_dirs()
            if self.config.get('emulator_url'):
                self.download_unpack(self.config['emulator_url'], dirs['abs_work_dir'])
            elif self.config.get('emulator_manifest'):
                manifest_path = self.create_tooltool_manifest(self.config['emulator_manifest'])
                dirs = self.query_abs_dirs()
                cache = self.config.get("tooltool_cache", None)
                if self.tooltool_fetch(manifest_path,
                                       output_dir=dirs['abs_work_dir'],
                                       cache=cache):
                    self.fatal("Unable to download emulator via tooltool!")
        if not os.path.isfile(self.adb_path):
            self.fatal("The adb binary '%s' is not a valid file!" % self.adb_path)
        self.kill_processes("xpcshell")
        self.emulator_proc = self._launch_emulator()

    def verify_device(self):
        """
        Check to see if the emulator can be contacted via adb.
        If any communication attempt fails, kill the emulator, re-launch, and re-check.
        """
        if not self.is_android:
            return

        if self.is_emulator:
            max_restarts = 5
            emulator_ok = self._retry(max_restarts, 10,
                                      self._verify_emulator_and_restart_on_fail,
                                      "Check emulator")
            if not emulator_ok:
                self.fatal('INFRA-ERROR: Unable to start emulator after %d attempts'
                           % max_restarts, EXIT_STATUS_DICT[TBPL_RETRY])

        self.mkdir_p(self.query_abs_dirs()['abs_blob_upload_dir'])
        self.dump_perf_info()
        self.logcat_start()
        self.delete_ANRs()
        self.delete_tombstones()
        self.info("verify_device complete")

    @PreScriptAction('run-tests')
    def timed_screenshots(self, action, success=None):
        """
        If configured, start screenshot timers.
        """
        if not self.is_android:
            return

        def take_screenshot(seconds):
            self.device_screenshot("screenshot-%ss-" % str(seconds))
            self.info("timed (%ss) screenshot complete" % str(seconds))

        self.timers = []
        for seconds in self.config.get("screenshot_times", []):
            self.info("screenshot requested %s seconds from now" % str(seconds))
            t = Timer(int(seconds), take_screenshot, [seconds])
            t.start()
            self.timers.append(t)

    @PostScriptAction('run-tests')
    def stop_device(self, action, success=None):
        """
        Stop logcat and kill the emulator, if necessary.
        """
        if not self.is_android:
            return

        for t in self.timers:
            t.cancel()
        if self.worst_status != TBPL_RETRY:
            self.check_for_ANRs()
            self.check_for_tombstones()
        else:
            self.info("ANR and tombstone checks skipped due to TBPL_RETRY")
        self.logcat_stop()
        if self.is_emulator:
            self.kill_processes(self.config["emulator_process_name"])
