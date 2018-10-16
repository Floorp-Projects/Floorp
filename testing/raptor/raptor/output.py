
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
            vals = []
            subtests = []
            suite = {
                'name': test.name,
                'type': test.type,
                'extraOptions': test.extra_options,
                'subtests': subtests,
                'lowerIsBetter': test.lower_is_better,
                'unit': test.unit,
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

                for measurement_name, replicates in test.measurements.iteritems():
                    new_subtest = {}
                    new_subtest['name'] = test.name + "-" + measurement_name
                    new_subtest['replicates'] = replicates
                    new_subtest['lowerIsBetter'] = test.subtest_lower_is_better
                    new_subtest['alertThreshold'] = float(test.alert_threshold)
                    new_subtest['value'] = 0
                    new_subtest['unit'] = test.subtest_unit

                    # ignore first value due to 1st pageload noise
                    LOG.info("ignoring the first %s value due to initial pageload noise"
                             % measurement_name)
                    filtered_values = filter.ignore_first(new_subtest['replicates'], 1)

                    # for pageload tests that measure TTFI: TTFI is not guaranteed to be available
                    # everytime; the raptor measure.js webext will substitute a '-1' value in the
                    # cases where TTFI is not available, which is acceptable; however we don't want
                    # to include those '-1' TTFI values in our final results calculations
                    if measurement_name == "ttfi":
                        filtered_values = filter.ignore_negative(filtered_values)
                        # we've already removed the first pageload value; if there aren't any more
                        # valid TTFI values available for this pageload just remove it from results
                        if len(filtered_values) < 1:
                            continue

                    new_subtest['value'] = filter.median(filtered_values)

                    vals.append([new_subtest['value'], new_subtest['name']])
                    subtests.append(new_subtest)

            elif test.type == "benchmark":
                if 'speedometer' in test.measurements:
                    subtests, vals = self.parseSpeedometerOutput(test)
                elif 'motionmark' in test.measurements:
                    subtests, vals = self.parseMotionmarkOutput(test)
                elif 'sunspider' in test.measurements:
                    subtests, vals = self.parseSunspiderOutput(test)
                elif 'webaudio' in test.measurements:
                    subtests, vals = self.parseWebaudioOutput(test)
                elif 'unity-webgl' in test.measurements:
                    subtests, vals = self.parseUnityWebGLOutput(test)
                elif 'assorted-dom' in test.measurements:
                    subtests, vals = self.parseAssortedDomOutput(test)
                elif 'wasm-misc' in test.measurements:
                    subtests, vals = self.parseWASMMiscOutput(test)
                suite['subtests'] = subtests

            else:
                LOG.error("output.summarize received unsupported test results type")
                return

            # for pageload tests, if there are > 1 subtests here, that means there
            # were multiple measurements captured in each single pageload; we want
            # to get the mean of those values and report 1 overall 'suite' value
            # for the page; so that each test page/URL only has 1 line output
            # on treeherder/perfherder (all replicates available in the JSON)

            # for benchmarks there is generally  more than one subtest in each cycle
            # and a benchmark-specific formula is needed to calculate the final score

            if len(subtests) > 1:
                suite['value'] = self.construct_summary(vals, testname=test.name)

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

        _subtests = {}
        data = test.measurements['speedometer']
        for page_cycle in data:
            for sub, replicates in page_cycle[0].iteritems():
                # for each pagecycle, build a list of subtests and append all related replicates
                if sub not in _subtests.keys():
                    # subtest not added yet, first pagecycle, so add new one
                    _subtests[sub] = {'unit': test.subtest_unit,
                                      'alertThreshold': float(test.alert_threshold),
                                      'lowerIsBetter': test.subtest_lower_is_better,
                                      'name': sub,
                                      'replicates': []}
                _subtests[sub]['replicates'].extend([round(x, 3) for x in replicates])

        vals = []
        subtests = []
        names = _subtests.keys()
        names.sort(reverse=True)
        for name in names:
            _subtests[name]['value'] = filter.median(_subtests[name]['replicates'])
            subtests.append(_subtests[name])
            vals.append([_subtests[name]['value'], name])

        return subtests, vals

    def parseWASMMiscOutput(self, test):
        '''
          {u'wasm-misc': [
            [[{u'name': u'validate', u'time': 163.44000000000005},
              ...
              {u'name': u'__total__', u'time': 63308.434904788155}]],
            ...
            [[{u'name': u'validate', u'time': 129.42000000000002},
              {u'name': u'__total__', u'time': 63181.24089257814}]]
           ]}
        '''
        _subtests = {}
        data = test.measurements['wasm-misc']
        for page_cycle in data:
            for item in page_cycle[0]:
                # for each pagecycle, build a list of subtests and append all related replicates
                sub = item['name']
                if sub not in _subtests.keys():
                    # subtest not added yet, first pagecycle, so add new one
                    _subtests[sub] = {'unit': test.subtest_unit,
                                      'alertThreshold': float(test.alert_threshold),
                                      'lowerIsBetter': test.subtest_lower_is_better,
                                      'name': sub,
                                      'replicates': []}
                _subtests[sub]['replicates'].append(item['time'])

        vals = []
        subtests = []
        names = _subtests.keys()
        names.sort(reverse=True)
        for name in names:
            _subtests[name]['value'] = filter.median(_subtests[name]['replicates'])
            subtests.append(_subtests[name])
            vals.append([_subtests[name]['value'], name])

        return subtests, vals

    def parseWebaudioOutput(self, test):
        # each benchmark 'index' becomes a subtest; each pagecycle / iteration
        # of the test has multiple values per index/subtest

        # this is the format we receive the results in from the benchmark
        # i.e. this is ONE pagecycle of speedometer:

        # {u'name': u'raptor-webaudio-firefox', u'type': u'benchmark', u'measurements':
        # {u'webaudio': [[u'[{"name":"Empty testcase","duration":26,"buffer":{}},{"name"
        # :"Simple gain test without resampling","duration":66,"buffer":{}},{"name":"Simple
        # gain test without resampling (Stereo)","duration":71,"buffer":{}},{"name":"Simple
        # gain test without resampling (Stereo and positional)","duration":67,"buffer":{}},
        # {"name":"Simple gain test","duration":41,"buffer":{}},{"name":"Simple gain test
        # (Stereo)","duration":59,"buffer":{}},{"name":"Simple gain test (Stereo and positional)",
        # "duration":68,"buffer":{}},{"name":"Upmix without resampling (Mono -> Stereo)",
        # "duration":53,"buffer":{}},{"name":"Downmix without resampling (Mono -> Stereo)",
        # "duration":44,"buffer":{}},{"name":"Simple mixing (same buffer)",
        # "duration":288,"buffer":{}}

        _subtests = {}
        data = test.measurements['webaudio']
        for page_cycle in data:
            data = json.loads(page_cycle[0])
            for item in data:
                # for each pagecycle, build a list of subtests and append all related replicates
                sub = item['name']
                replicates = [item['duration']]
                if sub not in _subtests.keys():
                    # subtest not added yet, first pagecycle, so add new one
                    _subtests[sub] = {'unit': test.subtest_unit,
                                      'alertThreshold': float(test.alert_threshold),
                                      'lowerIsBetter': test.subtest_lower_is_better,
                                      'name': sub,
                                      'replicates': []}
                _subtests[sub]['replicates'].extend([round(x, 3) for x in replicates])

        vals = []
        subtests = []
        names = _subtests.keys()
        names.sort(reverse=True)
        for name in names:
            _subtests[name]['value'] = filter.median(_subtests[name]['replicates'])
            subtests.append(_subtests[name])
            vals.append([_subtests[name]['value'], name])

        print subtests
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

        _subtests = {}
        data = test.measurements['motionmark']
        for page_cycle in data:
            page_cycle_results = page_cycle[0]

            # TODO: this assumes a single suite is run
            suite = page_cycle_results.keys()[0]
            for sub in page_cycle_results[suite].keys():
                replicate = round(page_cycle_results[suite][sub]['frameLength']['average'], 3)

                if sub not in _subtests.keys():
                    # subtest not added yet, first pagecycle, so add new one
                    _subtests[sub] = {'unit': test.subtest_unit,
                                      'alertThreshold': float(test.alert_threshold),
                                      'lowerIsBetter': test.subtest_lower_is_better,
                                      'name': sub,
                                      'replicates': []}
                _subtests[sub]['replicates'].extend([replicate])

        vals = []
        subtests = []
        names = _subtests.keys()
        names.sort(reverse=True)
        for name in names:
            _subtests[name]['value'] = filter.median(_subtests[name]['replicates'])
            subtests.append(_subtests[name])
            vals.append([_subtests[name]['value'], name])

        return subtests, vals

    def parseSunspiderOutput(self, test):
        _subtests = {}
        data = test.measurements['sunspider']
        for page_cycle in data:
            for sub, replicates in page_cycle[0].iteritems():
                # for each pagecycle, build a list of subtests and append all related replicates
                if sub not in _subtests.keys():
                    # subtest not added yet, first pagecycle, so add new one
                    _subtests[sub] = {'unit': test.subtest_unit,
                                      'alertThreshold': float(test.alert_threshold),
                                      'lowerIsBetter': test.subtest_lower_is_better,
                                      'name': sub,
                                      'replicates': []}
                _subtests[sub]['replicates'].extend([round(x, 3) for x in replicates])

        subtests = []
        vals = []

        names = _subtests.keys()
        names.sort(reverse=True)
        for name in names:
            _subtests[name]['value'] = filter.mean(_subtests[name]['replicates'])
            subtests.append(_subtests[name])

            vals.append([_subtests[name]['value'], name])

        return subtests, vals

    def parseUnityWebGLOutput(self, test):
        """
        Example output (this is one page cycle):

        {'name': 'raptor-unity-webgl-firefox',
         'type': 'benchmark',
         'measurements': {
            'unity-webgl': [
                [
                    '[{"benchmark":"Mandelbrot GPU","result":1035361},...}]'
                ]
            ]
         },
         'lower_is_better': False,
         'unit': 'score'
        }
        """
        _subtests = {}
        data = test.measurements['unity-webgl']
        for page_cycle in data:
            data = json.loads(page_cycle[0])
            for item in data:
                # for each pagecycle, build a list of subtests and append all related replicates
                sub = item['benchmark']
                if sub not in _subtests.keys():
                    # subtest not added yet, first pagecycle, so add new one
                    _subtests[sub] = {'unit': test.subtest_unit,
                                      'alertThreshold': float(test.alert_threshold),
                                      'lowerIsBetter': test.subtest_lower_is_better,
                                      'name': sub,
                                      'replicates': []}
                _subtests[sub]['replicates'].append(item['result'])

        vals = []
        subtests = []
        names = _subtests.keys()
        names.sort(reverse=True)
        for name in names:
            _subtests[name]['value'] = filter.median(_subtests[name]['replicates'])
            subtests.append(_subtests[name])
            vals.append([_subtests[name]['value'], name])

        return subtests, vals

    def parseAssortedDomOutput(self, test):
        # each benchmark 'index' becomes a subtest; each pagecycle / iteration
        # of the test has multiple values

        # this is the format we receive the results in from the benchmark
        # i.e. this is ONE pagecycle of assorted-dom ('test' is a valid subtest name btw):

        # {u'worker-getname-performance-getter': 5.9, u'window-getname-performance-getter': 6.1,
        # u'window-getprop-performance-getter': 6.1, u'worker-getprop-performance-getter': 6.1,
        # u'test': 5.8, u'total': 30}

        # the 'total' is provided for us from the benchmark; the overall score will be the mean of
        # the totals from all pagecycles; but keep all the subtest values for the logs/json

        _subtests = {}
        data = test.measurements['assorted-dom']
        for pagecycle in data:
            for _sub, _value in pagecycle[0].iteritems():
                # build a list of subtests and append all related replicates
                if _sub not in _subtests.keys():
                    # subtest not added yet, first pagecycle, so add new one
                    _subtests[_sub] = {'unit': test.subtest_unit,
                                       'alertThreshold': float(test.alert_threshold),
                                       'lowerIsBetter': test.subtest_lower_is_better,
                                       'name': _sub,
                                       'replicates': []}
                _subtests[_sub]['replicates'].extend([_value])

        vals = []
        subtests = []
        names = _subtests.keys()
        names.sort(reverse=True)
        for name in names:
            _subtests[name]['value'] = round(filter.median(_subtests[name]['replicates']), 2)
            subtests.append(_subtests[name])
            # only use the 'total's to compute the overall result
            if name == 'total':
                vals.append([_subtests[name]['value'], name])

        return subtests, vals

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
    def webaudio_score(cls, val_list):
        """
        webaudio_score: self reported as 'Geometric Mean'
        """
        results = [i for i, j in val_list if j == 'Geometric Mean']
        return filter.mean(results)

    @classmethod
    def unity_webgl_score(cls, val_list):
        """
        unity_webgl_score: self reported as 'Geometric Mean'
        """
        results = [i for i, j in val_list if j == 'Geometric Mean']
        return filter.mean(results)

    @classmethod
    def wasm_misc_score(cls, val_list):
        """
        wasm_misc_score: self reported as '__total__'
        """
        results = [i for i, j in val_list if j == '__total__']
        return filter.mean(results)

    @classmethod
    def stylebench_score(cls, val_list):
        """
        stylebench_score: https://bug-172968-attachments.webkit.org/attachment.cgi?id=319888
        """
        correctionFactor = 3
        results = [i for i, j in val_list]

        # stylebench has 5 tests, each of these are made of up 5 subtests
        #
        #   * Adding classes.
        #   * Removing classes.
        #   * Mutating attributes.
        #   * Adding leaf elements.
        #   * Removing leaf elements.
        #
        # which are made of two subtests each (sync/async) and repeated 5 times
        # each, thus, the list here looks like:
        #
        #   [Test name/Adding classes - 0/ Sync; <x>]
        #   [Test name/Adding classes - 0/ Async; <y>]
        #   [Test name/Adding classes - 0; <x> + <y>]
        #   [Test name/Removing classes - 0/ Sync; <x>]
        #   [Test name/Removing classes - 0/ Async; <y>]
        #   [Test name/Removing classes - 0; <x> + <y>]
        #   ...
        #   [Test name/Adding classes - 1 / Sync; <x>]
        #   [Test name/Adding classes - 1 / Async; <y>]
        #   [Test name/Adding classes - 1 ; <x> + <y>]
        #   ...
        #   [Test name/Removing leaf elements - 4; <x> + <y>]
        #   [Test name; <sum>] <- This is what we want.
        #
        # So, 5 (subtests) *
        #     5 (repetitions) *
        #     3 (entries per repetition (sync/async/sum)) =
        #     75 entries for test before the sum.
        #
        # We receive 76 entries per test, which ads up to 380. We want to use
        # the 5 test entries, not the rest.
        if len(results) != 380:
            raise Exception("StyleBench has 380 entries, found: %s instead" % len(results))

        results = results[75::76]
        score = 60 * 1000 / filter.geometric_mean(results) / correctionFactor
        return score

    @classmethod
    def sunspider_score(cls, val_list):
        results = [i for i, j in val_list]
        return sum(results)

    @classmethod
    def assorted_dom_score(cls, val_list):
        results = [i for i, j in val_list]
        return round(filter.geometric_mean(results), 2)

    def construct_summary(self, vals, testname):
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
        elif testname.startswith('raptor-sunspider'):
            return self.sunspider_score(vals)
        elif testname.startswith('raptor-unity-webgl'):
            return self.unity_webgl_score(vals)
        elif testname.startswith('raptor-webaudio'):
            return self.webaudio_score(vals)
        elif testname.startswith('raptor-assorted-dom'):
            return self.assorted_dom_score(vals)
        elif testname.startswith('raptor-wasm-misc'):
            return self.wasm_misc_score(vals)
        elif len(vals) > 1:
            return round(filter.geometric_mean([i for i, j in vals]), 2)
        else:
            return round(filter.mean([i for i, j in vals]), 2)
