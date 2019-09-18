# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# class to process, format, and report raptor test results
# received from the raptor control server
from __future__ import absolute_import

import json
import os

from abc import ABCMeta, abstractmethod
from logger.logger import RaptorLogger
from output import RaptorOutput, BrowsertimeOutput

LOG = RaptorLogger(component='perftest-results-handler')


class PerftestResultsHandler(object):
    """Abstract base class to handle perftest results"""

    __metaclass__ = ABCMeta

    def __init__(self, gecko_profile=False, power_test=False,
                 cpu_test=False, memory_test=False, **kwargs):
        self.gecko_profile = gecko_profile
        self.power_test = power_test
        self.cpu_test = cpu_test
        self.memory_test = memory_test
        self.results = []
        self.page_timeout_list = []
        self.images = []
        self.supporting_data = None

    @abstractmethod
    def add(self, new_result_json):
        raise NotImplementedError()

    def add_image(self, screenshot, test_name, page_cycle):
        # add to results
        LOG.info("received screenshot")
        self.images.append({'screenshot': screenshot,
                            'test_name': test_name,
                            'page_cycle': page_cycle})

    def add_page_timeout(self, test_name, page_url, pending_metrics):
        timeout_details = {'test_name': test_name,
                           'url': page_url}
        if pending_metrics:
            pending_metrics = [key for key, value in pending_metrics.items() if value]
            timeout_details['pending_metrics'] = ", ".join(pending_metrics)

        self.page_timeout_list.append(timeout_details)

    def add_supporting_data(self, supporting_data):
        ''' Supporting data is additional data gathered outside of the regular
        Raptor test run (i.e. power data). Will arrive in a dict in the format of:

        supporting_data = {'type': 'data-type',
                           'test': 'raptor-test-ran-when-data-was-gathered',
                           'unit': 'unit that the values are in',
                           'values': {
                               'name': value,
                               'nameN': valueN}}

        More specifically, power data will look like this:

        supporting_data = {'type': 'power',
                           'test': 'raptor-speedometer-geckoview',
                           'unit': 'mAh',
                           'values': {
                               'cpu': cpu,
                               'wifi': wifi,
                               'screen': screen,
                               'proportional': proportional}}
        '''
        LOG.info("RaptorResultsHandler.add_supporting_data received %s data"
                 % supporting_data['type'])
        if self.supporting_data is None:
            self.supporting_data = []
        self.supporting_data.append(supporting_data)

    def _get_expected_perfherder(self, output):
        def is_resource_test():
            if self.power_test or self.cpu_test or self.memory_test:
                return True
            return False

        # if results exists, determine if any test is of type 'scenario'
        is_scenario = False
        if (output.summarized_results or output.summarized_supporting_data):
            data = output.summarized_supporting_data
            if not data:
                data = [output.summarized_results]
            for next_data_set in data:
                data_type = next_data_set['suites'][0]['type']
                if data_type == 'scenario':
                    is_scenario = True
                    break

        if is_scenario and not is_resource_test():
            # skip perfherder check when a scenario test-type is run without
            # a resource flag
            return None

        expected_perfherder = 1
        if is_resource_test():
            # when resource tests are run, no perfherder data is output
            # for the regular raptor tests (i.e. speedometer) so we
            # expect one per resource-type, starting with 0
            expected_perfherder = 0
            if self.power_test:
                expected_perfherder += 1
            if self.memory_test:
                expected_perfherder += 1
            if self.cpu_test:
                expected_perfherder += 1

        return expected_perfherder

    def _validate_treeherder_data(self, output, output_perfdata):
        # late import is required, because install is done in create_virtualenv
        import jsonschema

        expected_perfherder = self._get_expected_perfherder(output)
        if expected_perfherder is None:
            LOG.info(
                "Skipping PERFHERDER_DATA check "
                "because no perfherder data output is expected"
            )
            return True
        elif output_perfdata != expected_perfherder:
            LOG.critical("PERFHERDER_DATA was seen %d times, expected %d."
                         % (output_perfdata, expected_perfherder))
            return False

        external_tools_path = os.environ['EXTERNALTOOLSPATH']
        schema_path = os.path.join(external_tools_path,
                                   'performance-artifact-schema.json')
        LOG.info("Validating PERFHERDER_DATA against %s" % schema_path)
        try:
            with open(schema_path) as f:
                schema = json.load(f)
            if output.summarized_results:
                data = output.summarized_results
            else:
                data = output.summarized_supporting_data[0]
            jsonschema.validate(data, schema)
        except Exception as e:
            LOG.exception("Error while validating PERFHERDER_DATA")
            LOG.info(str(e))
            return False
        return True

    @abstractmethod
    def summarize_and_output(self, test_config, tests, test_names):
        raise NotImplementedError()


class RaptorResultsHandler(PerftestResultsHandler):
    """Process Raptor results"""

    def add(self, new_result_json):
        # add to results
        LOG.info("received results in RaptorResultsHandler.add")
        new_result = RaptorTestResult(new_result_json)
        self.results.append(new_result)

    def summarize_and_output(self, test_config, tests, test_names):
        # summarize the result data, write to file and output PERFHERDER_DATA
        LOG.info("summarizing raptor test results")
        output = RaptorOutput(self.results, self.supporting_data, test_config['subtest_alert_on'])
        output.summarize(test_names)
        # that has each browser cycle separate; need to check if there were multiple browser
        # cycles, and if so need to combine results from all cycles into one overall result
        output.combine_browser_cycles()
        output.summarize_screenshots(self.images)
        # only dump out supporting data (i.e. power) if actual Raptor test completed
        out_sup_perfdata = 0
        if self.supporting_data is not None and len(self.results) != 0:
            output.summarize_supporting_data()
            res, out_sup_perfdata = output.output_supporting_data(test_names)
        res, out_perfdata = output.output(test_names)
        if not self.gecko_profile:
            # res will remain True if no problems are encountered
            # during schema validation and perferder_data counting
            res = self._validate_treeherder_data(output, out_sup_perfdata + out_perfdata)

        return res


class RaptorTestResult():
    """Single Raptor test result class"""

    def __init__(self, test_result_json):
        self.extra_options = []
        # convert test result json/dict (from control server) to test result object instance
        for key, value in test_result_json.iteritems():
            setattr(self, key, value)


class BrowsertimeResultsHandler(PerftestResultsHandler):
    """Process Browsertime results"""
    def __init__(self, config, root_results_dir=None):
        super(BrowsertimeResultsHandler, self).__init__(config)
        self._root_results_dir = root_results_dir

    def result_dir_for_test(self, test):
        return os.path.join(self._root_results_dir, test['name'])

    def add(self, new_result_json):
        # not using control server with bt
        pass

    def parse_browsertime_json(self, raw_btresults):
        """
        Receive a json blob that contains the results direct from the browsertime tool. Parse
        out the values that we wish to use and add those to our result object. That object will
        then be further processed in the BrowsertimeOutput class.

        The values that we care about in the browsertime.json are structured as follows.
        The 'browserScripts' section has one entry for each page-load / browsertime cycle!

        [
          {
            "info": {
              "browsertime": {
                "version": "4.9.2-android"
              },
              "url": "https://www.theguardian.co.uk",
            },
            "browserScripts": [
              {
                "browser": {
                  "userAgent": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.13; rv:70.0)
                                Gecko/20100101 Firefox/70.0",
                  "windowSize": "1366x694"
                },
                "timings": {
                  "firstPaint": 830,
                  "loadEventEnd": 4450,
                  "timeToContentfulPaint": 932,
                  "timeToDomContentFlushed": 864,
                  }
                }
              },
              {
                <repeated for every page-load cycle>
              },
            ],
            "statistics": {
              "timings": {
                "firstPaint": {
                  "median": 668,
                  "mean": 680,
                  "mdev": 9.6851,
                  "stddev": 48,
                  "min": 616,
                  "p10": 642,
                  "p90": 719,
                  "p99": 830,
                  "max": 830
                },
                "loadEventEnd": {
                  "median": 3476,
                  "mean": 3642,
                  "mdev": 111.7028,
                  "stddev": 559,
                  "min": 3220,
                  "p10": 3241,
                  "p90": 4450,
                  "p99": 5818,
                  "max": 5818
                },
                "timeToContentfulPaint": {
                  "median": 758,
                  "mean": 769,
                  "mdev": 10.0941,
                  "stddev": 50,
                  "min": 712,
                  "p10": 728,
                  "p90": 810,
                  "p99": 932,
                  "max": 932
                },
                "timeToDomContentFlushed": {
                  "median": 670,
                  "mean": 684,
                  "mdev": 11.6768,
                  "stddev": 58,
                  "min": 614,
                  "p10": 632,
                  "p90": 738,
                  "p99": 864,
                  "max": 864
                },
              }
            }
          }
        ]
        """
        LOG.info("parsing results from browsertime json")

        # For now, assume that browsertime loads only one site.
        if len(raw_btresults) != 1:
            raise ValueError("Browsertime did not measure exactly one site.")
        (_raw_bt_results,) = raw_btresults

        if not _raw_bt_results['browserScripts']:
            raise ValueError("Browsertime produced no measurements.")
        bt_browser = _raw_bt_results['browserScripts'][0]['browser']

        bt_ver = _raw_bt_results['info']['browsertime']['version']
        bt_url = _raw_bt_results['info']['url'],
        bt_result = {'bt_ver': bt_ver,
                     'browser': bt_browser,
                     'url': bt_url,
                     'measurements': {},
                     'statistics': {}}

        # bt to raptor names
        conversion = (('fnbpaint', 'firstPaint'),
                      ('fcp', 'timeToContentfulPaint'),
                      ('dcf', 'timeToDomContentFlushed'),
                      ('loadtime', 'loadEventEnd'))

        # extracting values from browserScripts and statistics
        for bt, raptor in conversion:
            # XXX looping several times in the list, could do better
            bt_result['measurements'][bt] = [cycle['timings'][raptor] for cycle in
                                             _raw_bt_results['browserScripts']]

            # let's add the browsertime statistics; we'll use those for overall values instead
            # of calculating our own based on the replicates
            bt_result['statistics'][bt] = _raw_bt_results['statistics']['timings'][raptor]

        return bt_result

    def summarize_and_output(self, test_config, tests, test_names):
        """
        Retrieve, process, and output the browsertime test results. Currently supports page-load
        type tests only.

        The Raptor framework either ran a single page-load test (one URL) - or - an entire suite
        of page-load tests (multiple test URLs). Regardless, every test URL measured will
        have its own 'browsertime.json' results file, located in a sub-folder names after the
        Raptor test name, i.e.:

        browsertime-results/
            raptor-tp6-amazon-firefox
                browsertime.json
            raptor-tp6-facebook-firefox
                browsertime.json
            raptor-tp6-google-firefox
                browsertime.json
            raptor-tp6-youtube-firefox
                browsertime.json

        For each test URL that was measured, find the resulting 'browsertime.json' file, and pull
        out the values that we care about.
        """
        # summarize the browsertime result data, write to file and output PERFHERDER_DATA
        LOG.info("retrieving browsertime test results")

        for test in tests:
            bt_res_json = os.path.join(self.result_dir_for_test(test), 'browsertime.json')
            if os.path.exists(bt_res_json):
                LOG.info("found browsertime results at %s" % bt_res_json)
            else:
                LOG.critical("unable to find browsertime results at %s" % bt_res_json)
                return False

            try:
                with open(bt_res_json, 'r') as f:
                    raw_btresults = json.load(f)
            except Exception as e:
                LOG.error("Exception reading %s" % bt_res_json)
                # XXX this should be replaced by a traceback call
                LOG.error("Exception: %s %s" % (type(e).__name__, str(e)))
                raise

            new_result = self.parse_browsertime_json(raw_btresults)

            # add additional info not from the browsertime json
            for field in ('name', 'unit', 'lower_is_better',
                          'alert_threshold', 'cold'):
                new_result[field] = test[field]

            # Differentiate Raptor `pageload` tests from `browsertime-pageload`
            # tests while we compare and contrast.
            new_result['type'] = "browsertime-pageload"

            # All Browsertime measurements are elapsed times in milliseconds.
            new_result['subtest_lower_is_better'] = True
            new_result['subtest_unit'] = 'ms'
            LOG.info("parsed new result: %s" % str(new_result))

            # `extra_options` will be populated with Gecko profiling flags in
            # the future.
            new_result['extra_options'] = []

            self.results.append(new_result)

        # now have all results gathered from all browsertime test URLs; format them for output
        output = BrowsertimeOutput(self.results,
                                   self.supporting_data,
                                   test_config['subtest_alert_on'])

        output.summarize(test_names)
        res, out_perfdata = output.output(test_names)

        if not self.gecko_profile:
            # res will remain True if no problems are encountered
            # during schema validation and perferder_data counting
            res = self._validate_treeherder_data(output, out_perfdata)

        return res
