# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import pathlib
import platform
import re
import subprocess

import mozversion

from mozperftest.utils import ON_TRY


class MultipleApplicationSetups(Exception):
    """Raised when there are multiple setup classes for an application."""

    pass


app_setups = {}


def binary_setup(klass):
    for app in klass.apps:
        if app in app_setups:
            raise MultipleApplicationSetups(f"{app} already has a setup class")
        app_setups[app] = klass
    return klass


def get_app_setup(app):
    app_setup = app_setups.get(app, None)
    return app_setup


class BaseVersionProducer:
    def __init__(self, application, logger):
        self.application = application
        self.logger = logger

    def get_binary_version(self, binary, **kwargs):
        # Try to get the version info through mozversion, and if that
        # fails then let the Desktop, or Mobile handle it
        application_info = mozversion.get_version(binary=binary)
        return application_info.get("application_version")


class DesktopVersionProducer(BaseVersionProducer):
    def get_binary_version(self, binary, **kwargs):
        try:
            return super(DesktopVersionProducer, self).get_binary_version(binary)
        except Exception:
            pass

        version = None
        try:
            if "mac" in platform.system().lower():
                import plistlib

                for plist_file in ("version.plist", "Info.plist"):
                    try:
                        binary_path = pathlib.Path(binary)
                        plist_path = binary_path.parent.parent.joinpath(plist_file)
                        with plist_path.open("rb") as plist_file_content:
                            plist = plistlib.load(plist_file_content)
                    except FileNotFoundError:
                        pass
                version = plist.get("CFBundleShortVersionString")
            elif "linux" in platform.system().lower():
                command = [binary, "--version"]
                proc = subprocess.run(
                    command, timeout=10, capture_output=True, text=True
                )

                bmeta = proc.stdout.split("\n")
                meta_re = re.compile(r"([A-z\s]+)\s+([\w.]*)")
                if len(bmeta) != 0:
                    match = meta_re.match(bmeta[0])
                    if match:
                        version = match.group(2)
            else:
                # On windows we need to use wimc to get the version
                command = r'wmic datafile where name="{0}"'.format(
                    binary.replace("\\", r"\\")
                )
                bmeta = subprocess.check_output(command)

                meta_re = re.compile(r"\s+([\d.a-z]+)\s+")
                match = meta_re.findall(bmeta.decode("utf-8"))
                if len(match) > 0:
                    version = match[-1]
        except Exception as e:
            self.logger.warning(
                "Failed to get browser meta data through fallback method: %s-%s"
                % (e.__class__.__name__, e)
            )
            raise e

        return version


class MobileVersionProducer(BaseVersionProducer):
    def get_binary_version(self, binary, apk_path=None, **kwargs):
        try:
            return super(MobileVersionProducer, self).get_binary_version(
                apk_path or binary
            )
        except Exception:
            pass

        from mozdevice import ADBDeviceFactory

        device = ADBDeviceFactory(verbose=True)
        pkg_info = device.shell_output("dumpsys package %s" % binary)
        version_matcher = re.compile(r".*versionName=([\d.]+)")
        for line in pkg_info.split("\n"):
            match = version_matcher.match(line)
            if match:
                return match.group(1)

        return None


class BaseSetup:
    version_producer = DesktopVersionProducer

    def __init__(self, application, layer_obj):
        self.application = application
        self.logger = layer_obj
        self.get_binary_path = layer_obj.mach_cmd.get_binary_path

    def get_binary_version(self, binary, **kwargs):
        return self.__class__.version_producer(
            self.application, self.logger
        ).get_binary_version(binary, **kwargs)


@binary_setup
class FirefoxSetup(BaseSetup):
    apps = ["firefox"]

    def setup_binary(self):
        if ON_TRY:
            self.logger.warning(
                "Cannot setup Firefox binary automatically in CI. Provide the path "
                "with --binary."
            )
            return None
        return self.get_binary_path()


@binary_setup
class ChromeSetup(BaseSetup):
    apps = ["chrome"]

    def setup_binary(self):
        if ON_TRY:
            self.logger.warning(
                "Cannot setup Chrome binary automatically in CI. Provide the path "
                "with --binary."
            )
            return None
        if "win" in platform.system().lower():
            self.logger.warning(
                "Getting the chrome binary on windows may not work. "
                "Use --binary to provide a path to it."
            )
            cmd = "where"
            executable = "chrome.exe"
        else:
            cmd = "which"
            executable = "google-chrome"
        return subprocess.check_output([cmd, executable]).decode("utf-8").strip()


@binary_setup
class ChromeMobileSetup(BaseSetup):
    apps = ["chrome-m"]
    version_producer = MobileVersionProducer

    def setup_binary(self):
        return "com.android.chrome"


@binary_setup
class FirefoxMobileSetup(BaseSetup):
    apps = ["fenix", "geckoview", "focus"]
    version_producer = MobileVersionProducer

    def setup_binary(self):
        return {
            "focus": "org.mozilla.focus",
            "geckoview": "org.mozilla.geckoview_example",
            "fenix": "org.mozilla.fenix",
        }[self.application]
