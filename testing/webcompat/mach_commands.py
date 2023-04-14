# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import io
import os
import platform
import sys

from mach.decorators import Command
from mozbuild.base import MozbuildObject

here = os.path.abspath(os.path.dirname(__file__))

GVE = "org.mozilla.geckoview_example"


def get(url):
    import requests

    resp = requests.get(url)
    resp.raise_for_status()
    return resp


def untar(fileobj, dest):
    import tarfile

    with tarfile.open(fileobj=fileobj, mode="r") as tar_data:
        tar_data.extractall(path=dest)


def unzip(fileobj, dest):
    import zipfile

    with zipfile.ZipFile(fileobj) as zip_data:
        zip_data.extractall(path=dest)


def create_parser_interventions():
    from mozlog import commandline

    parser = argparse.ArgumentParser()
    parser.add_argument("--webdriver-binary", help="Path to webdriver binary")
    parser.add_argument(
        "--webdriver-port",
        action="store",
        default="4444",
        help="Port on which to run WebDriver",
    )
    parser.add_argument(
        "--webdriver-ws-port",
        action="store",
        default="9222",
        help="Port on which to run WebDriver BiDi websocket",
    )
    parser.add_argument("--bug", help="Bug to run tests for")
    parser.add_argument(
        "--do2fa",
        action="store_true",
        default=False,
        help="Do two-factor auth live in supporting tests",
    )
    parser.add_argument(
        "--config", help="Path to JSON file containing logins and other settings"
    )
    parser.add_argument(
        "--debug", action="store_true", default=False, help="Debug failing tests"
    )
    parser.add_argument(
        "--headless",
        action="store_true",
        default=False,
        help="Run firefox in headless mode",
    )
    parser.add_argument(
        "--interventions",
        action="store",
        default="both",
        choices=["enabled", "disabled", "both", "none"],
        help="Enable webcompat interventions",
    )
    parser.add_argument(
        "--shims",
        action="store",
        default="none",
        choices=["enabled", "disabled", "both", "none"],
        help="Enable SmartBlock shims",
    )
    parser.add_argument(
        "--platform",
        action="store",
        choices=["android", "desktop"],
        help="Platform to target",
    )

    desktop_group = parser.add_argument_group("Desktop-specific arguments")
    desktop_group.add_argument("--binary", help="Path to browser binary")

    android_group = parser.add_argument_group("Android-specific arguments")
    android_group.add_argument(
        "--device-serial",
        action="store",
        help="Running Android instances to connect to, if not emulator-5554",
    )
    android_group.add_argument(
        "--package-name",
        action="store",
        default=GVE,
        help="Android package name to use",
    )

    commandline.add_logging_group(parser)
    return parser


class InterventionTest(MozbuildObject):
    def set_default_kwargs(self, logger, command_context, kwargs):
        platform = kwargs["platform"]
        binary = kwargs["binary"]
        device_serial = kwargs["device_serial"]
        is_gve_build = command_context.substs.get("MOZ_APP_NAME") == "fennec"

        if platform == "android" or (
            platform is None and binary is None and (device_serial or is_gve_build)
        ):
            kwargs["platform"] = "android"
        else:
            kwargs["platform"] = "desktop"

        if kwargs["platform"] == "desktop" and kwargs["binary"] is None:
            kwargs["binary"] = self.get_binary_path()

        if kwargs["webdriver_binary"] is None:
            webdriver_binary = self.get_binary_path(
                "geckodriver", validate_exists=False
            )

            if not os.path.exists(webdriver_binary):
                webdriver_binary = self.install_geckodriver(
                    logger, dest=os.path.dirname(webdriver_binary)
                )

            if not os.path.exists(webdriver_binary):
                logger.error("Can't find geckodriver")
                sys.exit(1)
            kwargs["webdriver_binary"] = webdriver_binary

    def platform_string_geckodriver(self):
        uname = platform.uname()
        platform_name = {"Linux": "linux", "Windows": "win", "Darwin": "macos"}.get(
            uname[0]
        )

        if platform_name in ("linux", "win"):
            bits = "64" if uname[4] == "x86_64" else "32"
        elif platform_name == "macos":
            bits = ""
        else:
            raise ValueError(f"No precompiled geckodriver for platform {uname}")

        return f"{platform_name}{bits}"

    def install_geckodriver(self, logger, dest):
        """Install latest Geckodriver."""
        if dest is None:
            dest = os.path.join(self.distdir, "dist", "bin")

        is_windows = platform.uname()[0] == "Windows"

        release = get(
            "https://api.github.com/repos/mozilla/geckodriver/releases/latest"
        ).json()
        ext = "zip" if is_windows else "tar.gz"
        platform_name = self.platform_string_geckodriver()
        name_suffix = f"-{platform_name}.{ext}"
        for item in release["assets"]:
            if item["name"].endswith(name_suffix):
                url = item["browser_download_url"]
                break
        else:
            raise ValueError(f"Failed to find geckodriver for platform {platform_name}")

        logger.info(f"Installing geckodriver from {url}")

        data = io.BytesIO(get(url).content)
        data.seek(0)
        decompress = unzip if ext == "zip" else untar
        decompress(data, dest=dest)

        exe_ext = ".exe" if is_windows else ""
        path = os.path.join(dest, f"geckodriver{exe_ext}")

        return path

    def setup_device(self, command_context, kwargs):
        if kwargs["platform"] != "android":
            return

        app = kwargs["package_name"]
        device_serial = kwargs["device_serial"]

        if not device_serial:
            from mozrunner.devices.android_device import (
                InstallIntent,
                verify_android_device,
            )

            verify_android_device(
                command_context, app=app, network=True, install=InstallIntent.YES
            )

            kwargs["device_serial"] = os.environ.get("DEVICE_SERIAL")

        # GVE does not have the webcompat addon by default. Add it.
        if app == GVE:
            kwargs["addon"] = "/data/local/tmp/webcompat.xpi"
            push_to_device(
                command_context.substs["ADB"],
                device_serial,
                webcompat_addon(command_context),
                kwargs["addon"],
            )

    def run(self, command_context, **kwargs):
        import mozlog
        import runner

        mozlog.commandline.setup_logging(
            "test-interventions", kwargs, {"mach": sys.stdout}
        )
        logger = mozlog.get_default_logger("test-interventions")
        status_handler = mozlog.handlers.StatusHandler()
        logger.add_handler(status_handler)

        self.set_default_kwargs(logger, command_context, kwargs)

        self.setup_device(command_context, kwargs)

        if kwargs["interventions"] != "none":
            interventions = (
                ["enabled", "disabled"]
                if kwargs["interventions"] == "both"
                else [kwargs["interventions"]]
            )

            for interventions_setting in interventions:
                runner.run(
                    logger,
                    os.path.join(here, "interventions"),
                    kwargs["webdriver_binary"],
                    kwargs["webdriver_port"],
                    kwargs["webdriver_ws_port"],
                    browser_binary=kwargs.get("binary"),
                    device_serial=kwargs.get("device_serial"),
                    package_name=kwargs.get("package_name"),
                    addon=kwargs.get("addon"),
                    bug=kwargs["bug"],
                    debug=kwargs["debug"],
                    interventions=interventions_setting,
                    config=kwargs["config"],
                    headless=kwargs["headless"],
                    do2fa=kwargs["do2fa"],
                )

        if kwargs["shims"] != "none":
            shims = (
                ["enabled", "disabled"]
                if kwargs["shims"] == "both"
                else [kwargs["shims"]]
            )

            for shims_setting in shims:
                runner.run(
                    logger,
                    os.path.join(here, "shims"),
                    kwargs["webdriver_binary"],
                    kwargs["webdriver_port"],
                    kwargs["webdriver_ws_port"],
                    browser_binary=kwargs.get("binary"),
                    device_serial=kwargs.get("device_serial"),
                    package_name=kwargs.get("package_name"),
                    addon=kwargs.get("addon"),
                    bug=kwargs["bug"],
                    debug=kwargs["debug"],
                    shims=shims_setting,
                    config=kwargs["config"],
                    headless=kwargs["headless"],
                    do2fa=kwargs["do2fa"],
                )

        summary = status_handler.summarize()
        passed = (
            summary.unexpected_statuses == 0
            and summary.log_level_counts.get("ERROR", 0) == 0
            and summary.log_level_counts.get("CRITICAL", 0) == 0
        )
        return passed


def webcompat_addon(command_context):
    import shutil

    src = os.path.join(command_context.topsrcdir, "browser", "extensions", "webcompat")
    dst = os.path.join(
        command_context.virtualenv_manager.virtualenv_root, "webcompat.xpi"
    )
    shutil.make_archive(dst, "zip", src)
    shutil.move(f"{dst}.zip", dst)
    return dst


def push_to_device(adb_path, device_serial, local_path, remote_path):
    from mozdevice import ADBDeviceFactory

    device = ADBDeviceFactory(adb=adb_path, device=device_serial)
    device.push(local_path, remote_path)
    device.chmod(remote_path)


@Command(
    "test-interventions",
    category="testing",
    description="Test the webcompat interventions",
    parser=create_parser_interventions,
    virtualenv_name="webcompat",
)
def test_interventions(command_context, **params):
    here = os.path.abspath(os.path.dirname(__file__))
    command_context.virtualenv_manager.activate()
    command_context.virtualenv_manager.install_pip_requirements(
        os.path.join(here, "requirements.txt"),
        require_hashes=False,
    )

    intervention_test = command_context._spawn(InterventionTest)
    return 0 if intervention_test.run(command_context, **params) else 1
