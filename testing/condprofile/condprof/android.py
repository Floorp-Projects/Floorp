# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
""" Drives an android device.
"""
import os
import posixpath
import tempfile
import contextlib
import time
import logging

import attr
from arsenic.services import Geckodriver, free_port, subprocess_based_service
from mozdevice import ADBDevice, ADBError

from condprof.util import write_yml_file, logger, DEFAULT_PREFS, BaseEnv


# XXX most of this code should migrate into mozdevice - see Bug 1574849
class AndroidDevice:
    def __init__(self, app_name, marionette_port=2828, verbose=False):
        self.app_name = app_name
        self.fennec = "firefox" in app_name

        # XXX make that an option
        if "fenix" in app_name:
            self.activity = "org.mozilla.fenix.IntentReceiverActivity"
        elif self.fennec:
            self.activity = None
        else:
            self.activity = "org.mozilla.geckoview_example.GeckoViewActivity"
        self.verbose = verbose
        self.device = None
        self.marionette_port = marionette_port
        self.profile = None
        self.remote_profile = None
        self.log_file = None
        self._adb_fh = None

    def _set_adb_logger(self, log_file):
        self.log_file = log_file
        if self.log_file is None:
            return
        logger.info("Setting ADB log file to %s" % self.log_file)
        adb_logger = logging.getLogger("adb")
        adb_logger.setLevel(logging.DEBUG)
        self._adb_fh = logging.FileHandler(self.log_file)
        self._adb_fh.setLevel(logging.DEBUG)
        adb_logger.addHandler(self._adb_fh)

    def _unset_adb_logger(self):
        if self._adb_fh is None:
            return
        logging.getLogger("adb").removeHandler(self._adb_fh)
        self._adb_fh = None

    def clear_logcat(self, timeout=None, buffers=[]):
        if not self.device:
            return
        self.device.clear_logcat(timeout, buffers)

    def get_logcat(self):
        if not self.device:
            return None
        # we don't want to have ADBCommand dump the command
        # in the debug stream so we reduce its verbosity here
        # temporarely
        old_verbose = self.device._verbose
        self.device._verbose = False
        try:
            return self.device.get_logcat()
        finally:
            self.device._verbose = old_verbose

    def prepare(self, profile, logfile):
        self._set_adb_logger(logfile)
        try:
            # See android_emulator_pgo.py run_tests for more
            # details on why test_root must be /sdcard/tests
            # for android pgo due to Android 4.3.
            self.device = ADBDevice(verbose=self.verbose,
                                    logger_name="adb",
                                    test_root='/sdcard/tests')
        except Exception:
            logger.error("Cannot initialize device")
            raise
        device = self.device
        self.profile = profile

        # checking that the app is installed
        if not device.is_app_installed(self.app_name):
            raise Exception("%s is not installed" % self.app_name)

        # debug flag
        logger.info("Setting %s as the debug app on the phone" % self.app_name)
        device.shell(
            "am set-debug-app --persistent %s" % self.app_name,
            stdout_callback=logger.info,
        )

        # creating the profile on the device
        logger.info("Creating the profile on the device")
        remote_test_root = posixpath.join(device.test_root, "condprof")
        remote_profile = posixpath.join(remote_test_root, "profile")
        logger.info("The profile on the phone will be at %s" % remote_profile)
        device.rm(remote_test_root, force=True, recursive=True)
        device.mkdir(remote_test_root)
        device.chmod(remote_test_root, recursive=True, root=True)

        device.rm(remote_profile, force=True, recursive=True)
        logger.info("Pushing %s on the phone" % self.profile)
        device.push(profile, remote_profile)
        device.chmod(remote_profile, recursive=True, root=True)
        self.profile = profile
        self.remote_profile = remote_profile

        # creating the yml file
        yml_data = {
            "args": ["-marionette", "-profile", self.remote_profile],
            "prefs": DEFAULT_PREFS,
            "env": {"LOG_VERBOSE": 1, "R_LOG_LEVEL": 6, "MOZ_LOG": ""},
        }

        yml_name = "%s-geckoview-config.yaml" % self.app_name
        yml_on_host = posixpath.join(tempfile.mkdtemp(), yml_name)
        write_yml_file(yml_on_host, yml_data)
        tmp_on_device = posixpath.join("/data", "local", "tmp")
        if not device.exists(tmp_on_device):
            raise IOError("%s does not exists on the device" % tmp_on_device)
        yml_on_device = posixpath.join(tmp_on_device, yml_name)
        try:
            device.rm(yml_on_device, force=True, recursive=True)
            device.push(yml_on_host, yml_on_device)
            device.chmod(yml_on_device, recursive=True, root=True)
        except Exception:
            logger.info("could not create the yaml file on device. Permission issue?")
            raise

        # command line 'extra' args not used with geckoview apps; instead we use
        # an on-device config.yml file
        intent = "android.intent.action.VIEW"
        device.stop_application(self.app_name)
        if self.fennec:
            # XXX does the Fennec app picks up the YML file ?
            extra_args = [
                "-profile",
                self.remote_profile,
                "--es",
                "env0",
                "LOG_VERBOSE=1",
                "--es",
                "env1",
                "R_LOG_LEVEL=6",
                "--es",
                "env2",
                "MOZ_WEBRENDER=0",
            ]

            device.launch_fennec(
                self.app_name,
                extra_args=extra_args,
                url="about:blank",
                fail_if_running=False,
            )
        else:
            device.launch_application(
                self.app_name, self.activity, intent, extras=None, url="about:blank"
            )
        if not device.process_exist(self.app_name):
            raise Exception("Could not start %s" % self.app_name)

        logger.info("Creating socket forwarding on port %d" % self.marionette_port)
        device.forward(
            local="tcp:%d" % self.marionette_port,
            remote="tcp:%d" % self.marionette_port,
        )

        # we don't have a clean way for now to check that GV or Fenix
        # is ready to handle our tests. So here we just wait 30s
        logger.info("Sleeping for 30s")
        time.sleep(30)

    def stop_browser(self):
        logger.info("Stopping %s" % self.app_name)
        try:
            self.device.stop_application(self.app_name, root=True)
        except ADBError:
            logger.info("Could not stop the application using force-stop")

        time.sleep(5)
        if self.device.process_exist(self.app_name):
            logger.info("%s still running, trying SIGKILL" % self.app_name)
            num_tries = 0
            while self.device.process_exist(self.app_name) and num_tries < 5:
                try:
                    self.device.pkill(self.app_name, root=True)
                except ADBError:
                    pass
                num_tries += 1
                time.sleep(1)
        logger.info("%s stopped" % self.app_name)

    def collect_profile(self):
        logger.info("Collecting profile from %s" % self.remote_profile)
        self.device.pull(self.remote_profile, self.profile)

    def close(self):
        self._unset_adb_logger()
        if self.device is None:
            return
        try:
            self.device.remove_forwards("tcp:%d" % self.marionette_port)
        except ADBError:
            logger.warning("Could not remove forward port")


# XXX redundant, remove
@contextlib.contextmanager
def device(app_name, marionette_port=2828, verbose=True):
    device_ = AndroidDevice(app_name, marionette_port, verbose)
    try:
        yield device_
    finally:
        device_.close()


@attr.s
class AndroidGeckodriver(Geckodriver):
    async def start(self):
        port = free_port()
        await self._check_version()
        logger.info("Running Webdriver on port %d" % port)
        logger.info("Running Marionette on port 2828")
        pargs = [
            self.binary,
            "--log",
            "trace",
            "--port",
            str(port),
            "--marionette-port",
            "2828",
        ]
        logger.info("Connecting on Android device")
        pargs.append("--connect-existing")
        return await subprocess_based_service(
            pargs, f"http://localhost:{port}", self.log_file
        )


class AndroidEnv(BaseEnv):
    @contextlib.contextmanager
    def get_device(self, *args, **kw):
        with device(self.firefox, *args, **kw) as d:
            self.device = d
            yield self.device

    def get_target_platform(self):
        app = self.firefox.split("org.mozilla.")[-1]
        if self.device_name is None:
            return app
        return "%s-%s" % (self.device_name, app)

    def dump_logs(self):
        logger.info("Dumping Android logs")
        try:
            logcat = self.device.get_logcat()
            if logcat:
                # local path, not using posixpath
                logfile = os.path.join(self.archive, "logcat.log")
                logger.info("Writing logcat at %s" % logfile)
                with open(logfile, "wb") as f:
                    for line in logcat:
                        f.write(line.encode("utf8", errors="replace") + b"\n")
            else:
                logger.info("logcat came back empty")
        except Exception:
            logger.error("Could not extract the logcat", exc_info=True)

    @contextlib.contextmanager
    def get_browser(self):
        yield

    def get_browser_args(self, headless, prefs=None):
        # XXX merge with DesktopEnv.get_browser_args
        options = ["--allow-downgrade"]
        if headless:
            options.append("-headless")
        if prefs is None:
            prefs = {}
        return {"moz:firefoxOptions": {"args": options, "prefs": prefs}}

    def prepare(self, logfile):
        self.device.prepare(self.profile, logfile)

    def get_browser_version(self):
        return self.target_platform + "-XXXneedtograbversion"

    def get_geckodriver(self, log_file):
        return AndroidGeckodriver(binary=self.geckodriver, log_file=log_file)

    def check_session(self, session):
        async def fake_close(*args):
            pass

        session.close = fake_close

    def collect_profile(self):
        self.device.collect_profile()

    def stop_browser(self):
        self.device.stop_browser()
