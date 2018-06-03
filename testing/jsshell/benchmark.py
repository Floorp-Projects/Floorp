# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import json
import os
import re
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


class Benchmark(object):
    __metaclass__ = ABCMeta
    lower_is_better = True
    should_alert = False
    units = 'score'

    def __init__(self, shell, args=None):
        self.shell = shell
        self.args = args

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

        with open(os.path.join(self.path, 'VERSION'), 'r') as fh:
            self._version = fh.read().strip("\r\n\r\n \t")
        return self._version

    def reset(self):
        """Resets state between runs."""
        self.perfherder_data = {
            'framework': {
                'name': 'js-bench',
            },
            'suites': [
                {
                    'lowerIsBetter': self.lower_is_better,
                    'name': self.name,
                    'shouldAlert': self.should_alert,
                    'subtests': [],
                    'units': self.units,
                    'value': None
                },
            ],
        }
        self.suite = self.perfherder_data['suites'][0]

    def run(self):
        self.reset()

        process_args = {
            'cmd': self.command,
            'cwd': self.path,
            'onFinish': self.collect_results,
            'processOutputLine': self.process_line,
            'stream': sys.stdout,
        }
        proc = ProcessHandler(**process_args)
        proc.run()
        return proc.wait()


class Ares6(Benchmark):
    name = 'ares6'
    path = os.path.join('third_party', 'webkit', 'PerformanceTests', 'ARES-6')
    units = 'ms'

    @property
    def command(self):
        cmd = super(Ares6, self).command
        return cmd + ['cli.js']

    def reset(self):
        super(Ares6, self).reset()

        self.bench_name = None
        self.last_summary = None
        # Scores are of the form:
        # {<bench_name>: {<score_name>: [<values>]}}
        self.scores = defaultdict(lambda: defaultdict(list))

    def _try_find_score(self, score_name, line):
        m = re.search(score_name + ':\s*(\d+\.?\d*?) (\+-)?.+', line)
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

        if self._try_find_score('firstIteration', line):
            return

        if self._try_find_score('averageWorstCase', line):
            return

        if self._try_find_score('steadyState', line):
            return

        m = re.search('summary:\s*(\d+\.?\d*?) (\+-)?.+', line)
        if m:
            self.last_summary = float(m.group(1))

    def collect_results(self):
        for bench, scores in self.scores.items():
            for score, values in scores.items():
                total = sum(values) / len(values)
                test_name = "{}-{}".format(bench, score)
                self.suite['subtests'].append({'name': test_name, 'value': total})

        if self.last_summary:
            self.suite['value'] = self.last_summary


class SixSpeed(Benchmark):
    name = 'six-speed'
    path = os.path.join('third_party', 'webkit', 'PerformanceTests', 'six-speed')
    units = 'ms'

    @property
    def command(self):
        cmd = super(SixSpeed, self).command
        return cmd + ['test.js']

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


    def collect_results(self):
        bench_total = 0
        # NOTE: for this benchmark we run the test once, so we have a single value array
        for bench, scores in self.scores.items():
            for score, values in scores.items():
                test_name = "{}-{}".format(self.name, score)
                total = sum(values) / len(values)
                self.suite['subtests'].append({'name': test_name, 'value': total})
                bench_total += int(sum(values))
        self.suite['value'] = bench_total


all_benchmarks = {
    'ares6': Ares6,
    'six-speed': SixSpeed,
}


def run(benchmark, binary=None, extra_args=None, perfherder=False):
    if not binary:
        try:
            binary = os.path.join(build.bindir, 'js' + build.substs['BIN_SUFFIX'])
        except BuildEnvironmentNotFoundException:
            binary = None

        if not binary or not os.path.isfile(binary):
            print(JSSHELL_NOT_FOUND)
            return 1

    bench = all_benchmarks.get(benchmark)(binary, args=extra_args)
    res = bench.run()

    if perfherder:
        print("PERFHERDER_DATA: {}".format(json.dumps(bench.perfherder_data)))
    return res


def get_parser():
    parser = ArgumentParser()
    parser.add_argument('benchmark', choices=all_benchmarks.keys(),
                        help="The name of the benchmark to run.")
    parser.add_argument('-b', '--binary', default=None,
                        help="Path to the JS shell binary to use.")
    parser.add_argument('--arg', dest='extra_args', action='append', default=None,
                        help="Extra arguments to pass to the JS shell.")
    parser.add_argument('--perfherder', action='store_true', default=False,
                        help="Log PERFHERDER_DATA to stdout.")
    return parser


def cli(args=sys.argv[1:]):
    parser = get_parser()
    args = parser.parser_args(args)
    return run(**vars(args))


if __name__ == '__main__':
    sys.exit(cli())
