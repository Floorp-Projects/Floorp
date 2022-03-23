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
    parser.add_argument("--binary", help="Path to browser binary")
    parser.add_argument("--webdriver-binary", help="Path to webdriver binary")
    parser.add_argument("--bug", help="Bug to run tests for")
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
        choices=["enabled", "disabled", "both"],
        help="Enable webcompat interventions",
    )
    commandline.add_logging_group(parser)
    return parser


class InterventionTest(MozbuildObject):
    def set_default_kwargs(self, logger, kwargs):
        if kwargs["binary"] is None:
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

    def get_capabilities(self, kwargs):
        return {"moz:firefoxOptions": {"binary": kwargs["binary"]}}

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

    def run(self, **kwargs):
        import runner
        import mozlog

        mozlog.commandline.setup_logging(
            "test-interventions", kwargs, {"mach": sys.stdout}
        )
        logger = mozlog.get_default_logger("test-interventions")
        status_handler = mozlog.handlers.StatusHandler()
        logger.add_handler(status_handler)

        self.set_default_kwargs(logger, kwargs)

        interventions = (
            ["enabled", "disabled"]
            if kwargs["interventions"] == "both"
            else [kwargs["interventions"]]
        )

        for interventions_setting in interventions:
            runner.run(
                logger,
                os.path.join(here, "interventions"),
                kwargs["binary"],
                kwargs["webdriver_binary"],
                bug=kwargs["bug"],
                debug=kwargs["debug"],
                interventions=interventions_setting,
                config=kwargs["config"],
                headless=kwargs["headless"],
            )

        summary = status_handler.summarize()
        passed = (
            summary.unexpected_statuses == 0
            and summary.log_level_counts.get("ERROR", 0) == 0
            and summary.log_level_counts.get("CRITICAL", 0) == 0
        )
        return passed


@Command(
    "test-interventions",
    category="testing",
    description="Test the webcompat interventions",
    parser=create_parser_interventions,
    virtualenv_name="webcompat",
)
def test_interventions(command_context, **params):
    intervention_test = command_context._spawn(InterventionTest)
    return 0 if intervention_test.run(**params) else 1
