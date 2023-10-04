#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import datetime
import functools
import glob
import os
import posixpath
import re
import signal
import subprocess
import tempfile
import time
from threading import Timer

import six

from mozharness.base.script import PostScriptAction, PreScriptAction
from mozharness.mozilla.automation import EXIT_STATUS_DICT, TBPL_RETRY


def ensure_dir(dir):
    """Ensures the given directory exists"""
    if dir and not os.path.exists(dir):
        try:
            os.makedirs(dir)
        except OSError as error:
            if error.errno != errno.EEXIST:
                raise


class AndroidMixin(object):
    """
    Mixin class used by Android test scripts.
    """

    def __init__(self, **kwargs):
        self._adb_path = None
        self._device = None
        self.app_name = None
        self.device_name = os.environ.get("DEVICE_NAME", None)
        self.device_serial = os.environ.get("DEVICE_SERIAL", None)
        self.device_ip = os.environ.get("DEVICE_IP", None)
        self.logcat_proc = None
        self.logcat_file = None
        self.use_gles3 = False
        self.use_root = True
        self.xre_path = None
        super(AndroidMixin, self).__init__(**kwargs)

    @property
    def adb_path(self):
        """Get the path to the adb executable."""
        self.activate_virtualenv()
        if not self._adb_path:
            self._adb_path = self.query_exe("adb")
        return self._adb_path

    @property
    def device(self):
        if not self._device:
            # We must access the adb_path property to activate the
            # virtualenv before importing mozdevice in order to
            # import the mozdevice installed into the virtualenv and
            # not any system-wide installation of mozdevice.
            adb = self.adb_path
            import mozdevice

            self._device = mozdevice.ADBDeviceFactory(
                adb=adb, device=self.device_serial, use_root=self.use_root
            )
        return self._device

    @property
    def is_android(self):
        c = self.config
        installer_url = c.get("installer_url", None)
        return (
            self.device_serial is not None
            or self.is_emulator
            or (
                installer_url is not None
                and (installer_url.endswith(".apk") or installer_url.endswith(".aab"))
            )
        )

    @property
    def is_emulator(self):
        c = self.config
        return True if c.get("emulator_avd_name") else False

    def _get_repo_url(self, path):
        """
        Return a url for a file (typically a tooltool manifest) in this hg repo
        and using this revision (or mozilla-central/default if repo/rev cannot
        be determined).

        :param path specifies the directory path to the file of interest.
        """
        if "GECKO_HEAD_REPOSITORY" in os.environ and "GECKO_HEAD_REV" in os.environ:
            # probably taskcluster
            repo = os.environ["GECKO_HEAD_REPOSITORY"]
            revision = os.environ["GECKO_HEAD_REV"]
        else:
            # something unexpected!
            repo = "https://hg.mozilla.org/mozilla-central"
            revision = "default"
            self.warning(
                "Unable to find repo/revision for manifest; "
                "using mozilla-central/default"
            )
        url = "%s/raw-file/%s/%s" % (repo, revision, path)
        return url

    def _tooltool_fetch(self, url, dir):
        c = self.config
        manifest_path = self.download_file(
            url, file_name="releng.manifest", parent_dir=dir
        )
        if not os.path.exists(manifest_path):
            self.fatal(
                "Could not retrieve manifest needed to retrieve "
                "artifacts from %s" % manifest_path
            )
        # from TooltoolMixin, included in TestingMixin
        self.tooltool_fetch(
            manifest_path, output_dir=dir, cache=c.get("tooltool_cache", None)
        )

    def _launch_emulator(self):
        env = self.query_env()

        # Write a default ddms.cfg to avoid unwanted prompts
        avd_home_dir = self.abs_dirs["abs_avds_dir"]
        DDMS_FILE = os.path.join(avd_home_dir, "ddms.cfg")
        with open(DDMS_FILE, "w") as f:
            f.write("pingOptIn=false\npingId=0\n")
        self.info("wrote dummy %s" % DDMS_FILE)

        # Delete emulator auth file, so it doesn't prompt
        AUTH_FILE = os.path.join(
            os.path.expanduser("~"), ".emulator_console_auth_token"
        )
        if os.path.exists(AUTH_FILE):
            try:
                os.remove(AUTH_FILE)
                self.info("deleted %s" % AUTH_FILE)
            except Exception:
                self.warning("failed to remove %s" % AUTH_FILE)

        env["ANDROID_EMULATOR_HOME"] = avd_home_dir
        avd_path = os.path.join(avd_home_dir, "avd")
        if os.path.exists(avd_path):
            env["ANDROID_AVD_HOME"] = avd_path
            self.info("Found avds at %s" % avd_path)
        else:
            self.warning("AVDs missing? Not found at %s" % avd_path)

        if "deprecated_sdk_path" in self.config:
            sdk_path = os.path.abspath(os.path.join(avd_home_dir, ".."))
        else:
            sdk_path = self.abs_dirs["abs_sdk_dir"]
        if os.path.exists(sdk_path):
            env["ANDROID_SDK_HOME"] = sdk_path
            env["ANDROID_SDK_ROOT"] = sdk_path
            self.info("Found sdk at %s" % sdk_path)
        else:
            self.warning("Android sdk missing? Not found at %s" % sdk_path)

        avd_config_path = os.path.join(
            avd_path, "%s.ini" % self.config["emulator_avd_name"]
        )
        avd_folder = os.path.join(avd_path, "%s.avd" % self.config["emulator_avd_name"])
        if os.path.isfile(avd_config_path):
            # The ini file points to the absolute path to the emulator folder,
            # which might be different, so we need to update it.
            old_config = ""
            with open(avd_config_path, "r") as config_file:
                old_config = config_file.readlines()
                self.info("Old Config: %s" % old_config)
            with open(avd_config_path, "w") as config_file:
                for line in old_config:
                    if line.startswith("path="):
                        config_file.write("path=%s\n" % avd_folder)
                        self.info("Updating path from: %s" % line)
                    else:
                        config_file.write("%s\n" % line)
        else:
            self.warning("Could not find config path at %s" % avd_config_path)

        # enable EGL 3.0 in advancedFeatures.ini
        AF_FILE = os.path.join(avd_home_dir, "advancedFeatures.ini")
        with open(AF_FILE, "w") as f:
            if self.use_gles3:
                f.write("GLESDynamicVersion=on\n")
            else:
                f.write("GLESDynamicVersion=off\n")

        # extra diagnostics for kvm acceleration
        emu = self.config.get("emulator_process_name")
        if os.path.exists("/dev/kvm") and emu and "x86" in emu:
            try:
                self.run_command(["ls", "-l", "/dev/kvm"])
                self.run_command(["kvm-ok"])
                self.run_command(["emulator", "-accel-check"], env=env)
            except Exception as e:
                self.warning("Extra kvm diagnostics failed: %s" % str(e))

        self.info("emulator env: %s" % str(env))
        command = ["emulator", "-avd", self.config["emulator_avd_name"]]
        if "emulator_extra_args" in self.config:
            command += self.config["emulator_extra_args"]

        dir = self.query_abs_dirs()["abs_blob_upload_dir"]
        tmp_file = tempfile.NamedTemporaryFile(
            mode="w", prefix="emulator-", suffix=".log", dir=dir, delete=False
        )
        self.info("Launching the emulator with: %s" % " ".join(command))
        self.info("Writing log to %s" % tmp_file.name)
        proc = subprocess.Popen(
            command, stdout=tmp_file, stderr=tmp_file, env=env, bufsize=0
        )
        return proc

    def _verify_emulator(self):
        boot_ok = self._retry(
            30,
            10,
            self.is_boot_completed,
            "Verify Android boot completed",
            max_time=330,
        )
        if not boot_ok:
            self.warning("Unable to verify Android boot completion")
            return False
        return True

    def _verify_emulator_and_restart_on_fail(self):
        emulator_ok = self._verify_emulator()
        if not emulator_ok:
            self.device_screenshot("screenshot-emulator-start")
            self.kill_processes(self.config["emulator_process_name"])
            subprocess.check_call(["ps", "-ef"])
            # remove emulator tmp files
            for dir in glob.glob("/tmp/android-*"):
                self.rmtree(dir)
            time.sleep(5)
            self.emulator_proc = self._launch_emulator()
        return emulator_ok

    def _retry(self, max_attempts, interval, func, description, max_time=0):
        """
        Execute func until it returns True, up to max_attempts times, waiting for
        interval seconds between each attempt. description is logged on each attempt.
        If max_time is specified, no further attempts will be made once max_time
        seconds have elapsed; this provides some protection for the case where
        the run-time for func is long or highly variable.
        """
        status = False
        attempts = 0
        if max_time > 0:
            end_time = datetime.datetime.now() + datetime.timedelta(seconds=max_time)
        else:
            end_time = None
        while attempts < max_attempts and not status:
            if (end_time is not None) and (datetime.datetime.now() > end_time):
                self.info(
                    "Maximum retry run-time of %d seconds exceeded; "
                    "remaining attempts abandoned" % max_time
                )
                break
            if attempts != 0:
                self.info("Sleeping %d seconds" % interval)
                time.sleep(interval)
            attempts += 1
            self.info(
                ">> %s: Attempt #%d of %d" % (description, attempts, max_attempts)
            )
            status = func()
        return status

    def dump_perf_info(self):
        """
        Dump some host and android device performance-related information
        to an artifact file, to help understand task performance.
        """
        dir = self.query_abs_dirs()["abs_blob_upload_dir"]
        perf_path = os.path.join(dir, "android-performance.log")
        with open(perf_path, "w") as f:
            f.write("\n\nHost cpufreq/scaling_governor:\n")
            cpus = glob.glob("/sys/devices/system/cpu/cpu*/cpufreq/scaling_governor")
            for cpu in cpus:
                out = subprocess.check_output(["cat", cpu], universal_newlines=True)
                f.write("%s: %s" % (cpu, out))

            f.write("\n\nHost /proc/cpuinfo:\n")
            out = subprocess.check_output(
                ["cat", "/proc/cpuinfo"], universal_newlines=True
            )
            f.write(out)

            f.write("\n\nHost /proc/meminfo:\n")
            out = subprocess.check_output(
                ["cat", "/proc/meminfo"], universal_newlines=True
            )
            f.write(out)

            f.write("\n\nHost process list:\n")
            out = subprocess.check_output(["ps", "-ef"], universal_newlines=True)
            f.write(out)

            f.write("\n\nDevice /proc/cpuinfo:\n")
            cmd = "cat /proc/cpuinfo"
            out = self.shell_output(cmd)
            f.write(out)
            cpuinfo = out

            f.write("\n\nDevice /proc/meminfo:\n")
            cmd = "cat /proc/meminfo"
            out = self.shell_output(cmd)
            f.write(out)

            f.write("\n\nDevice process list:\n")
            cmd = "ps"
            out = self.shell_output(cmd)
            f.write(out)

        # Search android cpuinfo for "BogoMIPS"; if found and < (minimum), retry
        # this task, in hopes of getting a higher-powered environment.
        # (Carry on silently if BogoMIPS is not found -- this may vary by
        # Android implementation -- no big deal.)
        # See bug 1321605: Sometimes the emulator is really slow, and
        # low bogomips can be a good predictor of that condition.
        bogomips_minimum = int(self.config.get("bogomips_minimum") or 0)
        for line in cpuinfo.split("\n"):
            m = re.match(r"BogoMIPS.*: (\d*)", line, re.IGNORECASE)
            if m:
                bogomips = int(m.group(1))
                if bogomips_minimum > 0 and bogomips < bogomips_minimum:
                    self.fatal(
                        "INFRA-ERROR: insufficient Android bogomips (%d < %d)"
                        % (bogomips, bogomips_minimum),
                        EXIT_STATUS_DICT[TBPL_RETRY],
                    )
                self.info("Found Android bogomips: %d" % bogomips)
                break

    def logcat_path(self):
        logcat_filename = "logcat-%s.log" % self.device_serial
        return os.path.join(
            self.query_abs_dirs()["abs_blob_upload_dir"], logcat_filename
        )

    def logcat_start(self):
        """
        Start recording logcat. Writes logcat to the upload directory.
        """
        # Start logcat for the device. The adb process runs until the
        # corresponding device is stopped. Output is written directly to
        # the blobber upload directory so that it is uploaded automatically
        # at the end of the job.
        self.logcat_file = open(self.logcat_path(), "w")
        logcat_cmd = [
            self.adb_path,
            "-s",
            self.device_serial,
            "logcat",
            "-v",
            "threadtime",
            "Trace:S",
            "StrictMode:S",
            "ExchangeService:S",
        ]
        self.info(" ".join(logcat_cmd))
        self.logcat_proc = subprocess.Popen(
            logcat_cmd, stdout=self.logcat_file, stdin=subprocess.PIPE
        )

    def logcat_stop(self):
        """
        Stop logcat process started by logcat_start.
        """
        if self.logcat_proc:
            self.info("Killing logcat pid %d." % self.logcat_proc.pid)
            self.logcat_proc.kill()
            self.logcat_file.close()

    def _install_android_app_retry(self, app_path, replace):
        import mozdevice

        try:
            if app_path.endswith(".aab"):
                self.device.install_app_bundle(
                    self.query_abs_dirs()["abs_bundletool_path"], app_path, timeout=120
                )
                self.device.run_as_package = self.query_package_name()
            else:
                self.device.run_as_package = self.device.install_app(
                    app_path, replace=replace, timeout=120
                )
            return True
        except (
            mozdevice.ADBError,
            mozdevice.ADBProcessError,
            mozdevice.ADBTimeoutError,
        ) as e:
            self.info(
                "Failed to install %s on %s: %s %s"
                % (app_path, self.device_name, type(e).__name__, e)
            )
            return False

    def install_android_app(self, app_path, replace=False):
        """
        Install the specified app.
        """
        app_installed = self._retry(
            5,
            10,
            functools.partial(self._install_android_app_retry, app_path, replace),
            "Install app",
        )

        if not app_installed:
            self.fatal(
                "INFRA-ERROR: Failed to install %s" % os.path.basename(app_path),
                EXIT_STATUS_DICT[TBPL_RETRY],
            )

    def uninstall_android_app(self):
        """
        Uninstall the app associated with the configured app, if it is
        installed.
        """
        import mozdevice

        try:
            package_name = self.query_package_name()
            self.device.uninstall_app(package_name)
        except (
            mozdevice.ADBError,
            mozdevice.ADBProcessError,
            mozdevice.ADBTimeoutError,
        ) as e:
            self.info(
                "Failed to uninstall %s from %s: %s %s"
                % (package_name, self.device_name, type(e).__name__, e)
            )
            self.fatal(
                "INFRA-ERROR: %s Failed to uninstall %s"
                % (type(e).__name__, package_name),
                EXIT_STATUS_DICT[TBPL_RETRY],
            )

    def is_boot_completed(self):
        import mozdevice

        try:
            return self.device.is_device_ready(timeout=30)
        except (ValueError, mozdevice.ADBError, mozdevice.ADBTimeoutError):
            pass
        return False

    def shell_output(self, cmd, enable_run_as=False):
        import mozdevice

        try:
            return self.device.shell_output(
                cmd, timeout=30, enable_run_as=enable_run_as
            )
        except mozdevice.ADBTimeoutError as e:
            self.info(
                "Failed to run shell command %s from %s: %s %s"
                % (cmd, self.device_name, type(e).__name__, e)
            )
            self.fatal(
                "INFRA-ERROR: %s Failed to run shell command %s"
                % (type(e).__name__, cmd),
                EXIT_STATUS_DICT[TBPL_RETRY],
            )

    def device_screenshot(self, prefix):
        """
        On emulator, save a screenshot of the entire screen to the upload directory;
        otherwise, save a screenshot of the device to the upload directory.

        :param prefix specifies a filename prefix for the screenshot
        """
        from mozscreenshot import dump_device_screen, dump_screen

        reset_dir = False
        if not os.environ.get("MOZ_UPLOAD_DIR", None):
            dirs = self.query_abs_dirs()
            os.environ["MOZ_UPLOAD_DIR"] = dirs["abs_blob_upload_dir"]
            reset_dir = True
        if self.is_emulator:
            if self.xre_path:
                dump_screen(self.xre_path, self, prefix=prefix)
            else:
                self.info("Not saving screenshot: no XRE configured")
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
            for p in glob.glob(os.path.join(xre_dir, "host-utils-*")):
                if os.path.isdir(p) and os.path.isfile(os.path.join(p, "xpcshell")):
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
            if "androidTest" in self.installer_path:
                self.app_name = "org.mozilla.geckoview.test"
            elif "test_runner" in self.installer_path:
                self.app_name = "org.mozilla.geckoview.test_runner"
            elif "geckoview" in self.installer_path:
                self.app_name = "org.mozilla.geckoview_example"
        if self.app_name is None:
            # Find appname from package-name.txt - assumes download-and-extract
            # has completed successfully.
            # The app/package name will typically be org.mozilla.fennec,
            # but org.mozilla.firefox for release builds, and there may be
            # other variations. 'aapt dump badging <apk>' could be used as an
            # alternative to package-name.txt, but introduces a dependency
            # on aapt, found currently in the Android SDK build-tools component.
            app_dir = self.abs_dirs["abs_work_dir"]
            self.app_path = os.path.join(app_dir, self.installer_path)
            unzip = self.query_exe("unzip")
            package_path = os.path.join(app_dir, "package-name.txt")
            unzip_cmd = [unzip, "-q", "-o", self.app_path]
            self.run_command(unzip_cmd, cwd=app_dir, halt_on_failure=True)
            self.app_name = str(
                self.read_from_file(package_path, verbose=True)
            ).rstrip()
        return self.app_name

    def kill_processes(self, process_name):
        self.info("Killing every process called %s" % process_name)
        process_name = six.ensure_binary(process_name)
        out = subprocess.check_output(["ps", "-A"])
        for line in out.splitlines():
            if process_name in line:
                pid = int(line.split(None, 1)[0])
                self.info("Killing pid %d." % pid)
                os.kill(pid, signal.SIGKILL)

    def delete_ANRs(self):
        remote_dir = self.device.stack_trace_dir
        try:
            if not self.device.is_dir(remote_dir):
                self.device.mkdir(remote_dir)
                self.info("%s created" % remote_dir)
                return
            self.device.chmod(remote_dir, recursive=True)
            for trace_file in self.device.ls(remote_dir, recursive=True):
                trace_path = posixpath.join(remote_dir, trace_file)
                if self.device.is_file(trace_path):
                    self.device.rm(trace_path)
                    self.info("%s deleted" % trace_path)
        except Exception as e:
            self.info(
                "failed to delete %s: %s %s" % (remote_dir, type(e).__name__, str(e))
            )

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
            self.device.chmod(remote_dir, recursive=True)
            self.device.pull(remote_dir, dirs["abs_blob_upload_dir"])
            self.delete_ANRs()
        except Exception as e:
            self.info(
                "failed to pull %s: %s %s" % (remote_dir, type(e).__name__, str(e))
            )

    def delete_tombstones(self):
        remote_dir = "/data/tombstones"
        try:
            if not self.device.is_dir(remote_dir):
                self.device.mkdir(remote_dir)
                self.info("%s created" % remote_dir)
                return
            self.device.chmod(remote_dir, recursive=True)
            for trace_file in self.device.ls(remote_dir, recursive=True):
                trace_path = posixpath.join(remote_dir, trace_file)
                if self.device.is_file(trace_path):
                    self.device.rm(trace_path)
                    self.info("%s deleted" % trace_path)
        except Exception as e:
            self.info(
                "failed to delete %s: %s %s" % (remote_dir, type(e).__name__, str(e))
            )

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
            self.device.chmod(remote_dir, recursive=True)
            self.device.pull(remote_dir, dirs["abs_blob_upload_dir"])
            self.delete_tombstones()
        except Exception as e:
            self.info(
                "failed to pull %s: %s %s" % (remote_dir, type(e).__name__, str(e))
            )

    # Script actions

    def start_emulator(self):
        """
        Starts the emulator
        """
        if not self.is_emulator:
            return

        dirs = self.query_abs_dirs()
        ensure_dir(dirs["abs_work_dir"])
        ensure_dir(dirs["abs_blob_upload_dir"])

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
            emulator_ok = self._retry(
                max_restarts,
                10,
                self._verify_emulator_and_restart_on_fail,
                "Check emulator",
            )
            if not emulator_ok:
                self.fatal(
                    "INFRA-ERROR: Unable to start emulator after %d attempts"
                    % max_restarts,
                    EXIT_STATUS_DICT[TBPL_RETRY],
                )

        self.mkdir_p(self.query_abs_dirs()["abs_blob_upload_dir"])
        self.dump_perf_info()
        self.logcat_start()
        self.delete_ANRs()
        self.delete_tombstones()
        self.info("verify_device complete")

    @PreScriptAction("run-tests")
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

    @PostScriptAction("run-tests")
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
