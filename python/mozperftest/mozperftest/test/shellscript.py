# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import json
import os
import platform
import signal

import mozprocess

from mozperftest.layers import Layer


class UnknownScriptError(Exception):
    """Triggered when an unknown script type is encountered."""

    pass


class ShellScriptData:
    def open_data(self, data):
        return {
            "name": "shellscript",
            "subtest": data["name"],
            "data": [
                {"file": "custom", "value": value, "xaxis": xaxis}
                for xaxis, value in enumerate(data["values"])
            ],
            "shouldAlert": data.get("shouldAlert", True),
            "unit": data.get("unit", "ms"),
            "lowerIsBetter": data.get("lowerIsBetter", True),
        }

    def transform(self, data):
        return data

    merge = transform


class ShellScriptRunner(Layer):
    """
    This is a layer that can be used to run custom shell scripts. They
    are expected to produce a log message prefixed with `perfMetrics` and
    contain the data from the test.
    """

    name = "shell-script"
    activated = True
    arguments = {
        "output-timeout": {
            "default": 120,
            "help": "Output timeout for the script, or how long to wait for a log message.",
        },
        "process-timeout": {
            "default": 600,
            "help": "Process timeout for the script, or the max run time.",
        },
    }

    def __init__(self, env, mach_cmd):
        super(ShellScriptRunner, self).__init__(env, mach_cmd)
        self.metrics = []
        self.timed_out = False
        self.output_timed_out = False

    def kill(self, proc):
        if "win" in platform.system().lower():
            proc.send_signal(signal.CTRL_BREAK_EVENT)
        else:
            os.killpg(proc.pid, signal.SIGKILL)
        proc.wait()

    def parse_metrics(self):
        parsed_metrics = []
        for metrics in self.metrics:
            prepared_metrics = (
                metrics.replace("perfMetrics:", "")
                .replace("{{", "{")
                .replace("}}", "}")
                .strip()
            )

            metrics_json = json.loads(prepared_metrics)
            if isinstance(metrics_json, list):
                parsed_metrics.extend(metrics_json)
            else:
                parsed_metrics.append(metrics_json)

        return parsed_metrics

    def line_handler_wrapper(self):
        """This function is used to gather the perfMetrics logs."""

        def _line_handler(proc, line):
            # NOTE: this hack is to workaround encoding issues on windows
            # a newer version of browsertime adds a `σ` character to output
            line = line.replace(b"\xcf\x83", b"")

            line = line.decode("utf-8")
            if "perfMetrics" in line:
                self.metrics.append(line)
            self.info(line.strip())

        return _line_handler

    def run(self, metadata):
        test = metadata.script

        cmd = []
        if test["filename"].endswith(".sh"):
            cmd = ["bash", test["filename"]]
        else:
            raise UnknownScriptError(
                "Only `.sh` (bash) scripts are currently implemented."
            )

        def timeout_handler(proc):
            self.timed_out = True
            self.error("Process timed out")
            self.kill(proc)

        def output_timeout_handler(proc):
            self.output_timed_out = True
            self.error("Process output timed out")
            self.kill(proc)

        os.environ["BROWSER_BINARY"] = metadata.binary
        mozprocess.run_and_wait(
            cmd,
            output_line_handler=self.line_handler_wrapper(),
            env=os.environ,
            timeout=self.get_arg("process-timeout"),
            timeout_handler=timeout_handler,
            output_timeout=self.get_arg("output-timeout"),
            output_timeout_handler=output_timeout_handler,
            text=False,
        )

        metadata.add_result(
            {
                "name": test["name"],
                "framework": {"name": "mozperftest"},
                "transformer": "mozperftest.test.shellscript:ShellScriptData",
                "shouldAlert": True,
                "results": self.parse_metrics(),
            }
        )

        return metadata
