# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# class to process, format, and report raptor test results
# received from the raptor control server
from __future__ import absolute_import

from mozlog import get_proxy_logger
from output import Output

LOG = get_proxy_logger(component='results-handler')


class RaptorResultsHandler():
    """Handle Raptor test results"""

    def __init__(self):
        self.results = []
        self.page_timeout_list = []
        self.images = []
        self.supporting_data = None

    def add(self, new_result_json):
        # add to results
        LOG.info("received results in RaptorResultsHandler.add")
        new_result = RaptorTestResult(new_result_json)
        self.results.append(new_result)

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

    def summarize_and_output(self, test_config, test_names):
        # summarize the result data, write to file and output PERFHERDER_DATA
        LOG.info("summarizing raptor test results")
        output = Output(self.results, self.supporting_data, test_config['subtest_alert_on'])
        output.summarize(test_names)
        # that has each browser cycle separate; need to check if there were multiple browser
        # cycles, and if so need to combine results from all cycles into one overall result
        output.combine_browser_cycles()
        output.summarize_screenshots(self.images)
        # only dump out supporting data (i.e. power) if actual Raptor test completed
        if self.supporting_data is not None and len(self.results) != 0:
            output.summarize_supporting_data()
            output.output_supporting_data(test_names)
        return output.output(test_names)


class RaptorTestResult():
    """Single Raptor test result class"""

    def __init__(self, test_result_json):
        self.extra_options = []
        # convert test result json/dict (from control server) to test result object instance
        for key, value in test_result_json.iteritems():
            setattr(self, key, value)
