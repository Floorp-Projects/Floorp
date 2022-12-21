#!/usr/bin/env python
import pathlib
from unittest import mock

import mozunit

from mozperftest.environment import METRICS, SYSTEM, TEST
from mozperftest.tests.support import EXAMPLE_TEST, get_running_env, temp_file
from mozperftest.utils import temp_dir

HERE = pathlib.Path(__file__).parent
LOGCAT = HERE / "data" / "logcat"


def fetch(self, url):
    return str(HERE / "fetched_artifact.zip")


class FakeDevice:
    def __init__(self, **args):
        self.apps = []

    def uninstall_app(self, apk_name):
        return True

    def install_app(self, apk, replace=True):
        if apk not in self.apps:
            self.apps.append(apk)

    def is_app_installed(self, app_name):
        return True

    def get_logcat(self):
        with LOGCAT.open() as f:
            for line in f:
                yield line


@mock.patch("mozperftest.test.browsertime.runner.install_package")
@mock.patch(
    "mozperftest.test.noderunner.NodeRunner.verify_node_install", new=lambda x: True
)
@mock.patch("mozbuild.artifact_cache.ArtifactCache.fetch", new=fetch)
@mock.patch(
    "mozperftest.test.browsertime.runner.BrowsertimeRunner._setup_node_packages",
    new=lambda x, y: None,
)
@mock.patch("mozperftest.system.android.ADBLoggedDevice", new=FakeDevice)
def test_android_log(*mocked):
    with temp_file() as logcat, temp_dir() as output:
        args = {
            "flavor": "mobile-browser",
            "android-install-apk": ["this.apk"],
            "android": True,
            "console": True,
            "android-timeout": 30,
            "android-capture-adb": "stdout",
            "android-capture-logcat": logcat,
            "android-app-name": "org.mozilla.fenix",
            "androidlog": True,
            "output": output,
            "browsertime-no-window-recorder": False,
            "browsertime-viewport-size": "1234x567",
            "tests": [EXAMPLE_TEST],
        }

        mach_cmd, metadata, env = get_running_env(**args)

        with env.layers[SYSTEM] as sys, env.layers[TEST] as andro:
            metadata = andro(sys(metadata))

        # we want to drop the first result
        metadata._results = metadata._results[1:]
        with env.layers[METRICS] as metrics:
            metadata = metrics(metadata)

        assert pathlib.Path(output, "LogCatstd-output.json").exists()


if __name__ == "__main__":
    mozunit.main()
