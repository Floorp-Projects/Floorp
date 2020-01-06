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

from condprof.util import write_yml_file, LOG, DEFAULT_PREFS, ERROR, BaseEnv


# XXX most of this code should migrate into mozdevice - see Bug 1574849
class AndroidDevice:
    def __init__(self, app_name, marionette_port=2828, verbose=False):
        self.app_name = app_name
        # XXX make that an option
        if "fenix" in app_name:
            self.activity = "org.mozilla.fenix.IntentReceiverActivity"
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
        LOG("Setting ADB log file to %s" % self.log_file)
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
        return self.device.get_logcat()

    def prepare(self, profile, logfile):
        self._set_adb_logger(logfile)
        try:
            self.device = ADBDevice(verbose=self.verbose, logger_name="adb")
        except Exception:
            ERROR("Cannot initialize device")
            raise
        device = self.device
        self.profile = profile

        # checking that the app is installed
        if not device.is_app_installed(self.app_name):
            raise Exception("%s is not installed" % self.app_name)

        # debug flag
        LOG("Setting %s as the debug app on the phone" % self.app_name)
        device.shell(
            "am set-debug-app --persistent %s" % self.app_name, stdout_callback=LOG
        )

        # creating the profile on the device
        LOG("Creating the profile on the device")
        remote_test_root = posixpath.join(device.test_root, "condprof")
        remote_profile = posixpath.join(remote_test_root, "profile")
        LOG("The profile on the phone will be at %s" % remote_profile)
        device.rm(remote_test_root, force=True, recursive=True)
        device.mkdir(remote_test_root)
        device.chmod(remote_test_root, recursive=True, root=True)

        device.rm(remote_profile, force=True, recursive=True)
        LOG("Pushing %s on the phone" % self.profile)
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
            LOG("could not create the yaml file on device. Permission issue?")
            raise

        # command line 'extra' args not used with geckoview apps; instead we use
        # an on-device config.yml file
        intent = "android.intent.action.VIEW"
        device.stop_application(self.app_name)
        device.launch_application(
            self.app_name, self.activity, intent, extras=None, url="about:blank"
        )
        if not device.process_exist(self.app_name):
            raise Exception("Could not start %s" % self.app_name)

        LOG("Creating socket forwarding on port %d" % self.marionette_port)
        device.forward(
            local="tcp:%d" % self.marionette_port,
            remote="tcp:%d" % self.marionette_port,
        )

        # we don't have a clean way for now to check that GV or Fenix
        # is ready to handle our tests. So here we just wait 30s
        LOG("Sleeping for 30s")
        time.sleep(30)

    def stop_browser(self):
        LOG("Stopping %s" % self.app_name)
        self.device.stop_application(self.app_name, root=True)
        time.sleep(5)
        if self.device.process_exist(self.app_name):
            LOG("%s still running, trying SIGKILL" % self.app_name)
            num_tries = 0
            while self.device.process_exist(self.app_name) and num_tries < 5:
                try:
                    self.device.pkill(self.app_name, root=True)
                except ADBError:
                    pass
                num_tries += 1
                time.sleep(1)
        LOG("%s stopped" % self.app_name)

    def collect_profile(self):
        LOG("Collecting profile from %s" % self.remote_profile)
        self.device.pull(self.remote_profile, self.profile)

    def close(self):
        self._unset_adb_logger()
        if self.device is None:
            return
        try:
            self.device.remove_forwards("tcp:%d" % self.marionette_port)
        except ADBError:
            LOG("Could not remove forward port")


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
        LOG("Running Webdriver on port %d" % port)
        LOG("Running Marionette on port 2828")
        pargs = [
            self.binary,
            "-vv",
            "--log",
            "trace",
            "--port",
            str(port),
            "--marionette-port",
            "2828",
        ]
        LOG("Connecting on Android device")
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
        try:
            logcat = self.device.get_logcat()
        except ADBError:
            ERROR("logcat call failure")
            return

        if logcat:
            # local path, not using posixpath
            logfile = os.path.join(self.archive, "logcat.log")
            LOG("Writing logcat at %s" % logfile)
            with open(logfile, "wb") as f:
                for line in logcat:
                    f.write(line.encode("utf8") + b"\n")
        else:
            LOG("logcat came back empty")

    @contextlib.contextmanager
    def get_browser(self):
        yield

    def get_browser_args(self, headless, prefs=None):
        options = []
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
