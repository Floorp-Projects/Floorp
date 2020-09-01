# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import subprocess
import json
import os
import sys
import errno
from pathlib import Path

from mozfile import which
from mozperftest.layers import Layer
from mozperftest.utils import silence

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

    merge = transform


class VisualMetrics(Layer):
    """Wrapper around Browsertime's visualmetrics.py script
    """

    name = "visualmetrics"
    activated = False
    arguments = {}

    def setup(self):
        self.metrics = []
        self.metrics_fields = []

        # making sure we have ffmpeg and imagemagick available
        for tool in ("ffmpeg", "magick"):
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
                    "name": "visualmetrics",
                    "framework": {"name": "mozperftest"},
                    "transformer": "mozperftest.metrics.visualmetrics:VisualData",
                    "results": self.metrics,
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

    def run_command(self, *cmd):
        cmd = list(cmd)
        self.debug(f"Running command {' '.join(cmd)}")
        env = dict(os.environ)
        path = [
            p
            for p in env["PATH"].split(os.pathsep)
            if p != self.mach_cmd.virtualenv_manager.bin_path
        ]
        env["PATH"] = os.pathsep.join(path)
        try:
            res = subprocess.check_output(cmd, universal_newlines=True, env=env)
            self.debug("Command succeeded", result=res)
            return 0, res
        except subprocess.CalledProcessError as e:
            self.debug("Command failed", cmd=cmd, status=e.returncode, output=e.output)
            return e.returncode, e.output

    def run_visual_metrics(self, browsertime_json):
        verbose = self.get_arg("verbose")
        self.info(f"Looking at {browsertime_json}")

        class _display:
            def __enter__(self, *args, **kw):
                return self

            __exit__ = __enter__

        may_silence = not verbose and silence or _display

        with browsertime_json.open() as f:
            browsertime_json_data = json.loads(f.read())

        videos = 0
        for site in browsertime_json_data:
            for video in site["files"]["video"]:
                videos += 1
                video_path = browsertime_json.parent / video
                res = "[]"
                with may_silence():
                    code, res = self.run_command(
                        str(self.visualmetrics), "--video", str(video_path), "--json"
                    )
                    if code != 0:
                        raise IOError(str(res))
                try:
                    res = json.loads(res)
                except json.JSONDecodeError:
                    self.error(
                        "Could not read the json output from" " visualmetrics.py"
                    )
                    raise

                for name, value in res.items():
                    if name in ("VisualProgress",):
                        self._expand_visual_progress(name, value)
                    else:
                        self.append_metrics(name, value)

        return videos

    def _expand_visual_progress(self, name, value):
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
            self.append_metrics(f"{name}{percent}", value)

    def append_metrics(self, name, value):
        if name not in self.metrics_fields:
            self.metrics_fields.append(name)
        self.metrics.append(
            {"name": name, "values": [value], "lowerIsBetter": True, "unit": "ms"}
        )
