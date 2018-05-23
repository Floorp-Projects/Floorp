
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# some parts of this originally taken from /testing/talos/talos/output.py

"""output raptor test results"""
from __future__ import absolute_import

import filter

import json
from mozlog import get_proxy_logger

LOG = get_proxy_logger(component="output")


class Output(object):
    """class for raptor output"""

    def __init__(self, results):
        """
        - results : list of RaptorTestResult instances
        """
        self.results = results
        self.summarized_results = {}

    def summarize(self):
        suites = []
        vals = []
        test_results = {
            'framework': {
                'name': 'raptor',
            },
            'suites': suites,
        }

        # check if we actually have any results
        if len(self.results) == 0:
            LOG.error("error: no raptor test results found!")
            return

        for test in self.results:
            subtests = []
            suite = {
                'name': test.name,
                'extraOptions': test.extra_options,
                'subtests': subtests
            }

            suites.append(suite)

            # each test can report multiple measurements per pageload
            # each measurement becomes a subtest inside the 'suite'
            for key, values in test.measurements.iteritems():
                new_subtest = {}
                new_subtest['name'] = test.name + "-" + key
                new_subtest['replicates'] = values
                new_subtest['lower_is_better'] = test.lower_is_better
                new_subtest['alert_threshold'] = float(test.alert_threshold)
                new_subtest['value'] = 0
                new_subtest['unit'] = test.unit

                filtered_values = filter.ignore_first(new_subtest['replicates'], 1)
                new_subtest['value'] = filter.median(filtered_values)
                vals.append(new_subtest['value'])

                subtests.append(new_subtest)

        # if there is more than one subtest, calculate a summary result
        if len(subtests) > 1:
            suite['value'] = self.construct_results(vals, testname=test.name)

        LOG.info("returning summarized test results:")
        LOG.info(test_results)

        self.summarized_results = test_results

    def output(self):
        """output to file and perfherder data json """
        if self.summarized_results == {}:
            LOG.error("error: no summarized raptor results found!")
            return False

        results_path = "raptor.json"

        with open(results_path, 'w') as f:
            for result in self.summarized_results:
                f.write("%s\n" % result)

        # the output that treeherder expects to find
        extra_opts = self.summarized_results['suites'][0].get('extraOptions', [])
        if 'geckoProfile' not in extra_opts:
            LOG.info("PERFHERDER_DATA: %s" % json.dumps(self.summarized_results))

        json.dump(self.summarized_results, open(results_path, 'w'), indent=2,
                  sort_keys=True)
        return True

    @classmethod
    def v8_Metric(cls, val_list):
        results = [i for i, j in val_list]
        score = 100 * filter.geometric_mean(results)
        return score

    @classmethod
    def JS_Metric(cls, val_list):
        """v8 benchmark score"""
        results = [i for i, j in val_list]
        return sum(results)

    @classmethod
    def speedometer_score(cls, val_list):
        """
        speedometer_score: https://bug-172968-attachments.webkit.org/attachment.cgi?id=319888
        """
        correctionFactor = 3
        results = [i for i, j in val_list]
        # speedometer has 16 tests, each of these are made of up 9 subtests
        # and a sum of the 9 values.  We receive 160 values, and want to use
        # the 16 test values, not the sub test values.
        if len(results) != 160:
            raise Exception("Speedometer has 160 subtests, found: %s instead" % len(results))

        results = results[9::10]
        score = 60 * 1000 / filter.geometric_mean(results) / correctionFactor
        return score

    @classmethod
    def benchmark_score(cls, val_list):
        """
        benchmark_score: ares6/jetstream self reported as 'geomean'
        """
        results = [i for i, j in val_list if j == 'geomean']
        return filter.mean(results)

    @classmethod
    def stylebench_score(cls, val_list):
        """
        stylebench_score: https://bug-172968-attachments.webkit.org/attachment.cgi?id=319888
        """
        correctionFactor = 3
        results = [i for i, j in val_list]
        # stylebench has 4 tests, each of these are made of up 12 subtests
        # and a sum of the 12 values.  We receive 52 values, and want to use
        # the 4 test values, not the sub test values.
        if len(results) != 52:
            raise Exception("StyleBench has 52 subtests, found: %s instead" % len(results))

        results = results[12::13]
        score = 60 * 1000 / filter.geometric_mean(results) / correctionFactor
        return score

    def construct_results(self, vals, testname):
        if testname.startswith('v8_7'):
            return self.v8_Metric(vals)
        elif testname.startswith('kraken'):
            return self.JS_Metric(vals)
        elif testname.startswith('ares6'):
            return self.benchmark_score(vals)
        elif testname.startswith('jetstream'):
            return self.benchmark_score(vals)
        elif testname.startswith('speedometer'):
            return self.speedometer_score(vals)
        elif testname.startswith('stylebench'):
            return self.stylebench_score(vals)
        elif len(vals) > 1:
            return filter.geometric_mean([i for i, j in vals])
        else:
            return filter.mean([i for i, j in vals])
