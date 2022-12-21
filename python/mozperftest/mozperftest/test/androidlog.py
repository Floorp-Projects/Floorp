# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from pathlib import Path

from mozperftest.layers import Layer


class AndroidLog(Layer):
    """Runs an android log test."""

    name = "androidlog"
    activated = False
    arguments = {
        "first-timestamp": {
            "type": str,
            "default": None,
            "help": "First timestamp regexp",
        },
        "second-timestamp": {
            "type": str,
            "default": None,
            "help": "Second timestamp regexp",
        },
        "subtest-name": {
            "type": str,
            "default": "TimeToDisplayed",
            "help": "Name of the metric that is produced",
        },
    }

    def _get_logcat(self):
        logcat = self.get_arg("android-capture-logcat")
        if logcat is None:
            raise NotImplementedError()
        # check if the path is absolute or relative to output
        path = Path(logcat)
        if not path.is_absolute():
            return Path(self.get_arg("output"), path).resolve()
        return path.resolve()

    def __call__(self, metadata):
        app_name = self.get_arg("android-app-name")
        first_ts = r".*Start proc.*" + app_name.replace(".", r"\.") + ".*"
        second_ts = r".*Fully drawn.*" + app_name.replace(".", r"\.") + ".*"
        options = {
            "first-timestamp": self.get_arg("first-timestamp", first_ts),
            "second-timestamp": self.get_arg("second-timestamp", second_ts),
            "processor": self.env.hooks.get("logcat_processor"),
            "transform-subtest-name": self.get_arg("subtest-name"),
        }

        metadata.add_result(
            {
                "results": str(self._get_logcat()),
                "transformer": "LogCatTimeTransformer",
                "transformer-options": options,
                "name": "LogCat",
            }
        )

        return metadata
