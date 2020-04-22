# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
from mozdevice import ADBDevice
from mozperftest.layers import Layer

HERE = os.path.dirname(__file__)


class DeviceError(Exception):
    pass


class AndroidDevice(Layer):
    """Use an android device via ADB
    """

    name = "android"
    # always activated, since we might
    # decide to use the phone depending on the binary
    activated = True

    arguments = {
        "intent": {"type": str, "default": None, "help": "Intent to use"},
        "app-name": {"type": str, "default": None, "help": "App name"},
        "activity": {"type": str, "default": None, "help": "Activity to use"},
        "install-apk": {
            "nargs": "*",
            "default": [],
            "help": "APK to install to the device",
        },
    }

    def __init__(self, env, mach_cmd):
        super(AndroidDevice, self).__init__(env, mach_cmd)
        self.android_activity = self.app_name = self.device = None

    def setup(self):
        pass

    def _setup_options(self, app_name="org.mozilla.firefox"):
        self.app_name = app_name
        try:
            self.device = ADBDevice(verbose=True)
        except AttributeError as e:
            self.error("Could not connect to the phone. Is it connected?")
            raise DeviceError(str(e))

        # install APKs
        for apk in self.get_arg("android-install-apk"):
            self.info("Installing %s" % apk)
            self.device.install_app(apk, replace=True)
            self.info("Done.")

        if self.android_activity is None:
            # guess the activity, given the app
            if "fenix" in app_name:
                self.android_activity = "org.mozilla.fenix.IntentReceiverActivity"
            elif "geckoview_example" in app_name:
                self.android_activity = (
                    "org.mozilla.geckoview_example.GeckoViewActivity"
                )
            self.set_arg("android_activity", self.android_activity)

        # checking that the app is installed
        if not self.device.is_app_installed(self.app_name):
            raise Exception("%s is not installed" % self.app_name)

        self.info("Android environment:")
        self.info("\t- application name: %s" % self.app_name)
        self.info("\t- activity: %s" % self.android_activity)
        self.info("\t- intent: %s" % self.get_arg("android_intent"))

    def teardown(self):
        pass

    def __call__(self, metadata):
        android = self.get_arg("android")
        app_name = self.get_arg("android-app-name")
        if app_name is None:
            app_name = self.get_arg("browser-binary")
        if app_name is not None and app_name.startswith("org.mozilla.") and not android:
            self.set_arg("android", True)
            android = True
        if not android:
            return metadata
        if app_name is None:
            app_name = "org.mozilla.firefox"
        self.set_arg("android-app-name", app_name)
        self.metadata = metadata
        self._setup_options(app_name)
        return metadata
