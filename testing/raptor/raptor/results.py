# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# class to process, format, and report raptor test results
# received from the raptor control server
from __future__ import absolute_import

from output import Output

from mozlog import get_proxy_logger

LOG = get_proxy_logger(component='results-handler')


class RaptorResultsHandler():
    """Handle Raptor test results"""

    def __init__(self):
        self.results = []

    def add(self, new_result_json):
        # add to results
        LOG.info("received results in RaptorResultsHandler.add")
        new_result = RaptorTestResult(new_result_json)
        self.results.append(new_result)

    def summarize_and_output(self, test_config):
        # summarize the result data, write to file and output PERFHERDER_DATA
        LOG.info("summarizing raptor test results")
        output = Output(self.results)
        output.summarize()
        return output.output()


class RaptorTestResult():
    """Single Raptor test result class"""

    def __init__(self, test_result_json):
        self.extra_options = []
        # convert test result json/dict (from control server) to test result object instance
        for key, value in test_result_json.iteritems():
            setattr(self, key, value)
