# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, division, print_function

import six
import json
import os
import re
import shutil
import sys
from abc import ABCMeta, abstractmethod, abstractproperty
from argparse import ArgumentParser
from collections import defaultdict

from mozbuild.base import MozbuildObject, BuildEnvironmentNotFoundException
from mozprocess import ProcessHandler

here = os.path.abspath(os.path.dirname(__file__))
build = MozbuildObject.from_environment(cwd=here)

JSSHELL_NOT_FOUND = """
Could not detect a JS shell. Either make sure you have a non-artifact build
with `ac_add_options --enable-js-shell` or specify it with `--binary`.
""".strip()


@six.add_metaclass(ABCMeta)
class Benchmark(object):
    lower_is_better = True
    should_alert = True

    def __init__(self, shell, args=None, shell_name=None):
        self.shell = shell
        self.args = args
        self.shell_name = shell_name

    @abstractproperty
    def unit(self):
        """Returns the unit of measurement of the benchmark."""

    @abstractproperty
    def name(self):
        """Returns the string name of the benchmark."""

    @abstractproperty
    def path(self):
        """Return the path to the benchmark relative to topsrcdir."""

    @abstractmethod
    def process_line(self, line):
        """Process a line of stdout from the benchmark."""

    @abstractmethod
    def collect_results(self):
        """Build the result after the process has finished."""

    @property
    def command(self):
        """Returns the command to run as a list."""
        cmd = [self.shell]
        if self.args:
            cmd += self.args
        return cmd

    @property
    def version(self):
        if self._version:
            return self._version

        with open(os.path.join(self.path, "VERSION"), "r") as fh:
            self._version = fh.read().strip("\r\n\r\n \t")
        return self._version

    def reset(self):
        """Resets state between runs."""
        name = self.name
        if self.shell_name:
            name = "{}-{}".format(name, self.shell_name)

        self.perfherder_data = {
            "framework": {
                "name": "js-bench",
            },
            "suites": [
                {
                    "lowerIsBetter": self.lower_is_better,
                    "name": name,
                    "shouldAlert": self.should_alert,
                    "subtests": [],
                    "unit": self.unit,
                    "value": None,
                },
            ],
        }
        self.suite = self.perfherder_data["suites"][0]

    def _provision_benchmark_script(self):
        if os.path.isdir(self.path):
            return

        # Some benchmarks may have been downloaded from a fetch task, make
        # sure they get copied over.
        fetches_dir = os.environ.get("MOZ_FETCHES_DIR")
        if fetches_dir and os.path.isdir(fetches_dir):
            fetchdir = os.path.join(fetches_dir, self.name)
            if os.path.isdir(fetchdir):
                shutil.copytree(fetchdir, self.path)

    def run(self):
        self.reset()

        # Update the environment variables
        env = os.environ.copy()

        process_args = {
            "cmd": self.command,
            "cwd": self.path,
            "onFinish": self.collect_results,
            "processOutputLine": self.process_line,
            "stream": sys.stdout,
            "env": env,
            "universal_newlines": True,
        }
        proc = ProcessHandler(**process_args)
        proc.run()
        return proc.wait()


class RunOnceBenchmark(Benchmark):
    def collect_results(self):
        bench_total = 0
        # NOTE: for this benchmark we run the test once, so we have a single value array
        for bench, scores in self.scores.items():
            for score, values in scores.items():
                test_name = "{}-{}".format(self.name, score)
                # pylint --py3k W1619
                mean = sum(values) / len(values)
                self.suite["subtests"].append({"name": test_name, "value": mean})
                bench_total += int(sum(values))
        self.suite["value"] = bench_total


class Ares6(Benchmark):
    name = "ares6"
    path = os.path.join("third_party", "webkit", "PerformanceTests", "ARES-6")
    unit = "ms"

    @property
    def command(self):
        cmd = super(Ares6, self).command
        return cmd + ["cli.js"]

    def reset(self):
        super(Ares6, self).reset()

        self.bench_name = None
        self.last_summary = None
        # Scores are of the form:
        # {<bench_name>: {<score_name>: [<values>]}}
        self.scores = defaultdict(lambda: defaultdict(list))

    def _try_find_score(self, score_name, line):
        m = re.search(score_name + ":\s*(\d+\.?\d*?) (\+-)?.+", line)
        if not m:
            return False

        score = m.group(1)
        self.scores[self.bench_name][score_name].append(float(score))
        return True

    def process_line(self, line):
        m = re.search("Running... (.+) \(.+\)", line)
        if m:
            self.bench_name = m.group(1)
            return

        if self._try_find_score("firstIteration", line):
            return

        if self._try_find_score("averageWorstCase", line):
            return

        if self._try_find_score("steadyState", line):
            return

        m = re.search("summary:\s*(\d+\.?\d*?) (\+-)?.+", line)
        if m:
            self.last_summary = float(m.group(1))

    def collect_results(self):
        for bench, scores in self.scores.items():
            for score, values in scores.items():
                # pylint --py3k W1619
                mean = sum(values) / len(values)
                test_name = "{}-{}".format(bench, score)
                self.suite["subtests"].append({"name": test_name, "value": mean})

        if self.last_summary:
            self.suite["value"] = self.last_summary


class SixSpeed(RunOnceBenchmark):
    name = "six-speed"
    path = os.path.join("third_party", "webkit", "PerformanceTests", "six-speed")
    unit = "ms"

    @property
    def command(self):
        cmd = super(SixSpeed, self).command
        return cmd + ["test.js"]

    def reset(self):
        super(SixSpeed, self).reset()

        # Scores are of the form:
        # {<bench_name>: {<score_name>: [<values>]}}
        self.scores = defaultdict(lambda: defaultdict(list))

    def process_line(self, output):
        m = re.search("(.+): (\d+)", output)
        if not m:
            return
        subtest = m.group(1)
        score = m.group(2)
        if subtest not in self.scores[self.name]:
            self.scores[self.name][subtest] = []
        self.scores[self.name][subtest].append(int(score))


class SunSpider(RunOnceBenchmark):
    name = "sunspider"
    path = os.path.join(
        "third_party", "webkit", "PerformanceTests", "SunSpider", "sunspider-0.9.1"
    )
    unit = "ms"

    @property
    def command(self):
        cmd = super(SunSpider, self).command
        return cmd + ["sunspider-standalone-driver.js"]

    def reset(self):
        super(SunSpider, self).reset()

        # Scores are of the form:
        # {<bench_name>: {<score_name>: [<values>]}}
        self.scores = defaultdict(lambda: defaultdict(list))

    def process_line(self, output):
        m = re.search("(.+): (\d+)", output)
        if not m:
            return
        subtest = m.group(1)
        score = m.group(2)
        if subtest not in self.scores[self.name]:
            self.scores[self.name][subtest] = []
        self.scores[self.name][subtest].append(int(score))


class WebToolingBenchmark(Benchmark):
    name = "web-tooling-benchmark"
    path = os.path.join(
        "third_party", "webkit", "PerformanceTests", "web-tooling-benchmark"
    )
    main_js = "cli.js"
    unit = "score"
    lower_is_better = False
    subtests_lower_is_better = False

    @property
    def command(self):
        cmd = super(WebToolingBenchmark, self).command
        return cmd + [self.main_js]

    def reset(self):
        super(WebToolingBenchmark, self).reset()

        # Scores are of the form:
        # {<bench_name>: {<score_name>: [<values>]}}
        self.scores = defaultdict(lambda: defaultdict(list))

    def process_line(self, output):
        m = re.search(" +([a-zA-Z].+): +([.0-9]+) +runs/sec", output)
        if not m:
            return
        subtest = m.group(1)
        score = m.group(2)
        if subtest not in self.scores[self.name]:
            self.scores[self.name][subtest] = []
        self.scores[self.name][subtest].append(float(score))

    def collect_results(self):
        # NOTE: for this benchmark we run the test once, so we have a single value array
        bench_mean = None
        for bench, scores in self.scores.items():
            for score_name, values in scores.items():
                test_name = "{}-{}".format(self.name, score_name)
                # pylint --py3k W1619
                mean = sum(values) / len(values)
                self.suite["subtests"].append(
                    {
                        "lowerIsBetter": self.subtests_lower_is_better,
                        "name": test_name,
                        "value": mean,
                    }
                )
                if score_name == "mean":
                    bench_mean = mean
        self.suite["value"] = bench_mean

    def run(self):
        self._provision_benchmark_script()
        return super(WebToolingBenchmark, self).run()


class Octane(RunOnceBenchmark):
    name = "octane"
    path = os.path.join("third_party", "webkit", "PerformanceTests", "octane")
    unit = "score"
    lower_is_better = False

    @property
    def command(self):
        cmd = super(Octane, self).command
        return cmd + ["run.js"]

    def reset(self):
        super(Octane, self).reset()

        # Scores are of the form:
        # {<bench_name>: {<score_name>: [<values>]}}
        self.scores = defaultdict(lambda: defaultdict(list))

    def process_line(self, output):
        m = re.search("(.+): (\d+)", output)
        if not m:
            return
        subtest = m.group(1)
        score = m.group(2)
        if subtest.startswith("Score"):
            subtest = "score"
        if subtest not in self.scores[self.name]:
            self.scores[self.name][subtest] = []
        self.scores[self.name][subtest].append(int(score))

    def collect_results(self):
        bench_score = None
        # NOTE: for this benchmark we run the test once, so we have a single value array
        for bench, scores in self.scores.items():
            for score_name, values in scores.items():
                test_name = "{}-{}".format(self.name, score_name)
                # pylint --py3k W1619
                mean = sum(values) / len(values)
                self.suite["subtests"].append({"name": test_name, "value": mean})
                if score_name == "score":
                    bench_score = mean
        self.suite["value"] = bench_score

    def run(self):
        self._provision_benchmark_script()
        return super(Octane, self).run()


all_benchmarks = {
    "ares6": Ares6,
    "six-speed": SixSpeed,
    "sunspider": SunSpider,
    "web-tooling-benchmark": WebToolingBenchmark,
    "octane": Octane,
}


def run(benchmark, binary=None, extra_args=None, perfherder=None):
    if not binary:
        try:
            binary = os.path.join(build.bindir, "js" + build.substs["BIN_SUFFIX"])
        except BuildEnvironmentNotFoundException:
            binary = None

        if not binary or not os.path.isfile(binary):
            print(JSSHELL_NOT_FOUND)
            return 1

    bench = all_benchmarks.get(benchmark)(
        binary, args=extra_args, shell_name=perfherder
    )
    res = bench.run()

    if perfherder:
        print("PERFHERDER_DATA: {}".format(json.dumps(bench.perfherder_data)))
    return res


def get_parser():
    parser = ArgumentParser()
    parser.add_argument(
        "benchmark",
        choices=list(all_benchmarks),
        help="The name of the benchmark to run.",
    )
    parser.add_argument(
        "-b", "--binary", default=None, help="Path to the JS shell binary to use."
    )
    parser.add_argument(
        "--arg",
        dest="extra_args",
        action="append",
        default=None,
        help="Extra arguments to pass to the JS shell.",
    )
    parser.add_argument(
        "--perfherder",
        default=None,
        help="Log PERFHERDER_DATA to stdout using the given suite name.",
    )
    return parser


def cli(args=sys.argv[1:]):
    parser = get_parser()
    args = parser.parser_args(args)
    return run(**vars(args))


if __name__ == "__main__":
    sys.exit(cli())
