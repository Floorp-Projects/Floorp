# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import tempfile
from pathlib import Path

from mozdevice import ADBDevice, ADBError
from mozperftest.layers import Layer
from mozperftest.utils import download_file


_ROOT_URL = "https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/"
_FENNEC_BUILDS = "project.mobile.fenix.v2.fennec-nightly.latest/artifacts/public/build/"
_PERMALINKS = {
    "fennec_nightly_armeabi_v7a": _ROOT_URL
    + _FENNEC_BUILDS
    + "armeabi-v7a/geckoNightly/target.apk"
}


class DeviceError(Exception):
    pass


class AndroidDevice(Layer):
    """Use an android device via ADB
    """

    name = "android"
    activated = False

    arguments = {
        "app-name": {
            "type": str,
            "default": "org.mozilla.firefox",
            "help": "Android app name",
        },
        "intent": {"type": str, "default": None, "help": "Intent to use"},
        "activity": {"type": str, "default": None, "help": "Activity to use"},
        "install-apk": {
            "nargs": "*",
            "default": [],
            "help": (
                "APK to install to the device "
                "Can be a file, an url or an alias url from "
                " %s" % ", ".join(_PERMALINKS.keys())
            ),
        },
    }

    def __init__(self, env, mach_cmd):
        super(AndroidDevice, self).__init__(env, mach_cmd)
        self.android_activity = self.app_name = self.device = None

    def setup(self):
        pass

    def teardown(self):
        pass

    def __call__(self, metadata):
        self.app_name = self.get_arg("android-app-name")
        self.android_activity = self.get_arg("android-activity")
        self.metadata = metadata
        try:
            self.device = ADBDevice(verbose=True, timeout=30)
        except (ADBError, AttributeError) as e:
            self.error("Could not connect to the phone. Is it connected?")
            raise DeviceError(str(e))

        # install APKs
        for apk in self.get_arg("android-install-apk"):
            self.info("Installing %s" % apk)
            if apk in _PERMALINKS:
                apk = _PERMALINKS[apk]
            if apk.startswith("http"):
                with tempfile.TemporaryDirectory() as tmpdirname:
                    target = Path(tmpdirname, "target.apk")
                    self.info("Downloading %s" % apk)
                    download_file(apk, target)
                    self.info("Installing downloaded APK")
                    self.device.install_app(str(target), replace=True)
            else:
                self.device.install_app(apk, replace=True)
            self.info("Done.")

        # checking that the app is installed
        if not self.device.is_app_installed(self.app_name):
            raise Exception("%s is not installed" % self.app_name)

        # set up default activity with the app name if none given
        if self.android_activity is None:
            # guess the activity, given the app
            if "fenix" in self.app_name:
                self.android_activity = "org.mozilla.fenix.IntentReceiverActivity"
            elif "geckoview_example" in self.app_name:
                self.android_activity = (
                    "org.mozilla.geckoview_example.GeckoViewActivity"
                )
            self.set_arg("android_activity", self.android_activity)

        self.info("Android environment:")
        self.info("- Application name: %s" % self.app_name)
        self.info("- Activity: %s" % self.android_activity)
        self.info("- Intent: %s" % self.get_arg("android_intent"))
        return metadata
