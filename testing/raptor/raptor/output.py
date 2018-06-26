
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# some parts of this originally taken from /testing/talos/talos/output.py

"""output raptor test results"""
from __future__ import absolute_import

import filter

import json
import os

from mozlog import get_proxy_logger

LOG = get_proxy_logger(component="raptor-output")


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
                'type': test.type,
                'extraOptions': test.extra_options,
                'subtests': subtests,
                'lowerIsBetter': test.lower_is_better,
                'alertThreshold': float(test.alert_threshold)
            }

            suites.append(suite)

            # process results for pageloader type of tests
            if test.type == "pageload":
                # each test can report multiple measurements per pageload
                # each measurement becomes a subtest inside the 'suite'

                # this is the format we receive the results in from the pageload test
                # i.e. one test (subtest) in raptor-firefox-tp6:

                # {u'name': u'raptor-firefox-tp6-amazon', u'type': u'pageload', u'measurements':
                # {u'fnbpaint': [788, 315, 334, 286, 318, 276, 296, 296, 292, 285, 268, 277, 274,
                # 328, 295, 290, 286, 270, 279, 280, 346, 303, 308, 398, 281]}, u'browser':
                # u'Firefox 62.0a1 20180528123052', u'lower_is_better': True, u'page':
                # u'https://www.amazon.com/s/url=search-alias%3Daps&field-keywords=laptop',
                # u'unit': u'ms', u'alert_threshold': 2}

                for key, values in test.measurements.iteritems():
                    new_subtest = {}
                    new_subtest['name'] = test.name + "-" + key
                    new_subtest['replicates'] = values
                    new_subtest['lowerIsBetter'] = test.lower_is_better
                    new_subtest['alertThreshold'] = float(test.alert_threshold)
                    new_subtest['value'] = 0
                    new_subtest['unit'] = test.unit

                    filtered_values = filter.ignore_first(new_subtest['replicates'], 1)
                    new_subtest['value'] = filter.median(filtered_values)
                    vals.append(new_subtest['value'])

                    subtests.append(new_subtest)

            elif test.type == "benchmark":
                if 'speedometer' in test.measurements:
                    subtests, vals = self.parseSpeedometerOutput(test)
                elif 'motionmark' in test.measurements:
                    subtests, vals = self.parseMotionmarkOutput(test)
                suite['subtests'] = subtests
            else:
                LOG.error("output.summarize received unsupported test results type")
                return

        # if there is more than one subtest, calculate a summary result
        if len(subtests) > 1:
            suite['value'] = self.construct_results(vals, testname=test.name)

        self.summarized_results = test_results

    def parseSpeedometerOutput(self, test):
        # each benchmark 'index' becomes a subtest; each pagecycle / iteration
        # of the test has multiple values per index/subtest

        # this is the format we receive the results in from the benchmark
        # i.e. this is ONE pagecycle of speedometer:

        # {u'name': u'raptor-speedometer', u'type': u'benchmark', u'measurements':
        # {u'speedometer': [[{u'AngularJS-TodoMVC/DeletingAllItems': [147.3000000000011,
        # 149.95999999999913, 143.29999999999927, 150.34000000000378, 257.6999999999971],
        # u'Inferno-TodoMVC/CompletingAllItems/Sync': [88.03999999999996,#
        # 85.60000000000036, 94.18000000000029, 95.19999999999709, 86.47999999999593],
        # u'AngularJS-TodoMVC': [518.2400000000016, 525.8199999999997, 610.5199999999968,
        # 532.8200000000215, 640.1800000000003], ...(repeated for each index/subtest)}]]},
        # u'browser': u'Firefox 62.0a1 20180528123052', u'lower_is_better': False, u'page':
        # u'http://localhost:55019/Speedometer/index.html?raptor', u'unit': u'score',
        # u'alert_threshold': 2}

        subtests = []
        vals = []
        data = test.measurements['speedometer']
        for page_cycle in data:
            page_cycle_results = page_cycle[0]

            for sub, replicates in page_cycle_results.iteritems():
                # for each pagecycle, replicates are appended to each subtest
                # so if it doesn't exist the first time create the subtest entry
                existing = False
                for existing_sub in subtests:
                    if existing_sub['name'] == sub:
                        # pagecycle, subtest already there, so append the replicates
                        existing_sub['replicates'].extend(replicates)
                        # update the value now that we have more replicates
                        existing_sub['value'] = filter.median(existing_sub['replicates'])
                        # now need to update our vals list too since have new subtest value
                        for existing_val in vals:
                            if existing_val[1] == sub:
                                existing_val[0] = existing_sub['value']
                                break
                        existing = True
                        break

                if not existing:
                    # subtest not added yet, first pagecycle, so add new one
                    new_subtest = {}
                    new_subtest['name'] = sub
                    new_subtest['replicates'] = replicates
                    new_subtest['lowerIsBetter'] = test.lower_is_better
                    new_subtest['alertThreshold'] = float(test.alert_threshold)
                    new_subtest['value'] = filter.median(replicates)
                    new_subtest['unit'] = test.unit
                    subtests.append(new_subtest)
                    vals.append([new_subtest['value'], sub])
        return subtests, vals

    def parseMotionmarkOutput(self, test):
        # for motionmark we want the frameLength:average value for each test

        # this is the format we receive the results in from the benchmark
        # i.e. this is ONE pagecycle of motionmark htmlsuite test:composited Transforms:

        # {u'name': u'raptor-motionmark-firefox',
        #  u'type': u'benchmark',
        #  u'measurements': {
        #    u'motionmark':
        #      [[{u'HTMLsuite':
        #        {u'Composited Transforms':
        #          {u'scoreLowerBound': 272.9947975553528,
        #           u'frameLength': {u'average': 25.2, u'stdev': 27.0,
        #                            u'percent': 68.2, u'concern': 39.5},
        #           u'controller': {u'average': 300, u'stdev': 0, u'percent': 0, u'concern': 3},
        #           u'scoreUpperBound': 327.0052024446473,
        #           u'complexity': {u'segment1': [[300, 16.6], [300, 16.6]], u'complexity': 300,
        #                           u'segment2': [[300, None], [300, None]], u'stdev': 6.8},
        #           u'score': 300.00000000000006,
        #           u'complexityAverage': {u'segment1': [[30, 30], [30, 30]], u'complexity': 30,
        #                                  u'segment2': [[300, 300], [300, 300]], u'stdev': None}
        #  }}}]]}}

        subtests = {}
        vals = []
        data = test.measurements['motionmark']
        for page_cycle in data:
            page_cycle_results = page_cycle[0]

            # TODO: this assumes a single suite is run
            suite = page_cycle_results.keys()[0]
            for sub in page_cycle_results[suite].keys():
                replicate = round(page_cycle_results[suite][sub]['frameLength']['average'], 3)

                # for each pagecycle, replicates are appended to each subtest
                if sub in subtests.keys():
                    subtests[sub]['replicates'].append(replicate)
                    subtests[sub]['value'] = filter.median(subtests[sub]['replicates'])
                    continue

                # subtest not added yet, first pagecycle, so add new one
                new_subtest = {}
                new_subtest['name'] = sub
                new_subtest['replicates'] = [replicate]
                new_subtest['lowerIsBetter'] = test.lower_is_better
                new_subtest['alertThreshold'] = float(test.alert_threshold)
                new_subtest['unit'] = test.unit
                subtests[sub] = new_subtest

        retVal = []
        subtest_names = subtests.keys()
        subtest_names.sort(reverse=True)
        for name in subtest_names:
            subtests[name]['value'] = filter.median(subtests[name]['replicates'])
            vals.append([subtests[name]['value'], name])
            retVal.append(subtests[name])

        return retVal, vals

    def output(self):
        """output to file and perfherder data json """
        if self.summarized_results == {}:
            LOG.error("error: no summarized raptor results found!")
            return False

        if os.environ['MOZ_UPLOAD_DIR']:
            # i.e. testing/mozharness/build/raptor.json locally; in production it will
            # be at /tasks/task_*/build/ (where it will be picked up by mozharness later
            # and made into a tc artifact accessible in treeherder as perfherder-data.json)
            results_path = os.path.join(os.path.dirname(os.environ['MOZ_UPLOAD_DIR']),
                                        'raptor.json')
        else:
            results_path = os.path.join(os.getcwd(), 'raptor.json')

        with open(results_path, 'w') as f:
            for result in self.summarized_results:
                f.write("%s\n" % result)

        # the output that treeherder expects to find
        extra_opts = self.summarized_results['suites'][0].get('extraOptions', [])
        if 'geckoProfile' not in extra_opts:
            LOG.info("PERFHERDER_DATA: %s" % json.dumps(self.summarized_results))

        json.dump(self.summarized_results, open(results_path, 'w'), indent=2,
                  sort_keys=True)

        LOG.info("results can also be found locally at: %s" % results_path)
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
        if testname.startswith('raptor-v8_7'):
            return self.v8_Metric(vals)
        elif testname.startswith('raptor-kraken'):
            return self.JS_Metric(vals)
        elif testname.startswith('raptor-jetstream'):
            return self.benchmark_score(vals)
        elif testname.startswith('raptor-speedometer'):
            return self.speedometer_score(vals)
        elif testname.startswith('raptor-stylebench'):
            return self.stylebench_score(vals)
        elif len(vals) > 1:
            return filter.geometric_mean([i for i, j in vals])
        else:
            return filter.mean([i for i, j in vals])
