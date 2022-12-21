# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import errno
import json
import os
import sys
from pathlib import Path

from mozfile import which

from mozperftest.layers import Layer
from mozperftest.utils import run_script, silence

METRICS_FIELDS = (
    "SpeedIndex",
    "FirstVisualChange",
    "LastVisualChange",
    "VisualProgress",
    "videoRecordingStart",
)


class VisualData:
    def open_data(self, data):
        res = {
            "name": "visualmetrics",
            "subtest": data["name"],
            "data": [
                {"file": "visualmetrics", "value": value, "xaxis": xaxis}
                for xaxis, value in enumerate(data["values"])
            ],
        }
        return res

    def transform(self, data):
        return data

    def merge(self, data):
        return data


class VisualMetrics(Layer):
    """Wrapper around Browsertime's visualmetrics.py script"""

    name = "visualmetrics"
    activated = False
    arguments = {}

    def setup(self):
        self.metrics = {}
        self.metrics_fields = []

        # making sure we have ffmpeg and imagemagick available
        for tool in ("ffmpeg", "convert"):
            if sys.platform in ("win32", "msys"):
                tool += ".exe"
            path = which(tool)
            if not path:
                raise OSError(errno.ENOENT, f"Could not find {tool}")

    def run(self, metadata):
        if "VISUALMETRICS_PY" not in os.environ:
            raise OSError(
                "The VISUALMETRICS_PY environment variable is not set."
                "Make sure you run the browsertime layer"
            )
        path = Path(os.environ["VISUALMETRICS_PY"])
        if not path.exists():
            raise FileNotFoundError(str(path))

        self.visualmetrics = path
        treated = 0

        for result in metadata.get_results():
            result_dir = result.get("results")
            if result_dir is None:
                continue
            result_dir = Path(result_dir)
            if not result_dir.is_dir():
                continue
            browsertime_json = Path(result_dir, "browsertime.json")
            if not browsertime_json.exists():
                continue
            treated += self.run_visual_metrics(browsertime_json)

        self.info(f"Treated {treated} videos.")

        if len(self.metrics) > 0:
            metadata.add_result(
                {
                    "name": metadata.script["name"] + "-vm",
                    "framework": {"name": "mozperftest"},
                    "transformer": "mozperftest.metrics.visualmetrics:VisualData",
                    "results": list(self.metrics.values()),
                }
            )

            # we also extend --perfherder-metrics and --console-metrics if they
            # are activated
            def add_to_option(name):
                existing = self.get_arg(name, [])
                for field in self.metrics_fields:
                    existing.append({"name": field, "unit": "ms"})
                self.env.set_arg(name, existing)

            if self.get_arg("perfherder"):
                add_to_option("perfherder-metrics")

            if self.get_arg("console"):
                add_to_option("console-metrics")

        else:
            self.warning("No video was treated.")
        return metadata

    def run_visual_metrics(self, browsertime_json):
        verbose = self.get_arg("verbose")
        self.info(f"Looking at {browsertime_json}")
        venv = self.mach_cmd.virtualenv_manager

        class _display:
            def __enter__(self, *args, **kw):
                return self

            __exit__ = __enter__

        may_silence = not verbose and silence or _display

        with browsertime_json.open() as f:
            browsertime_json_data = json.loads(f.read())

        videos = 0
        global_options = [
            str(self.visualmetrics),
            "--orange",
            "--perceptual",
            "--contentful",
            "--force",
            "--renderignore",
            "5",
            "--viewport",
        ]
        if verbose:
            global_options += ["-vvv"]

        for site in browsertime_json_data:
            # collecting metrics from browserScripts
            # because it can be used in splitting
            for index, bs in enumerate(site["browserScripts"]):
                for name, val in bs.items():
                    if not isinstance(val, (str, int)):
                        continue
                    self.append_metrics(index, name, val)

            extra = {"lowerIsBetter": True, "unit": "ms"}

            for index, video in enumerate(site["files"]["video"]):
                videos += 1
                video_path = browsertime_json.parent / video
                output = "[]"
                with may_silence():
                    res, output = run_script(
                        venv.python_path,
                        global_options + ["--video", str(video_path), "--json"],
                        verbose=verbose,
                        label="visual metrics",
                        display=False,
                    )
                    if not res:
                        self.error(f"Failed {res}")
                        continue

                output = output.strip()
                if verbose:
                    self.info(str(output))
                try:
                    output = json.loads(output)
                except json.JSONDecodeError:
                    self.error("Could not read the json output from visualmetrics.py")
                    continue

                for name, value in output.items():
                    if name.endswith(
                        "Progress",
                    ):
                        self._expand_visual_progress(index, name, value, **extra)
                    else:
                        self.append_metrics(index, name, value, **extra)

        return videos

    def _expand_visual_progress(self, index, name, value, **fields):
        def _split_percent(val):
            # value is of the form "567=94%"
            val = val.split("=")
            value, percent = val[0].strip(), val[1].strip()
            if percent.endswith("%"):
                percent = percent[:-1]
            return int(percent), int(value)

        percents = [_split_percent(elmt) for elmt in value.split(",")]

        # we want to keep the first added value for each percent
        # so the trick here is to create a dict() with the reversed list
        percents = dict(reversed(percents))

        # we are keeping the last 5 percents
        percents = list(percents.items())
        percents.sort()
        for percent, value in percents[:5]:
            self.append_metrics(index, f"{name}{percent}", value, **fields)

    def append_metrics(self, index, name, value, **fields):
        if name not in self.metrics_fields:
            self.metrics_fields.append(name)
        if name not in self.metrics:
            self.metrics[name] = {"name": name, "values": []}

        self.metrics[name]["values"].append(value)
        self.metrics[name].update(**fields)
