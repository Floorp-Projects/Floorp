# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys

AWSY_PATH = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
if AWSY_PATH not in sys.path:
    sys.path.append(AWSY_PATH)

from awsy.awsy_test_case import AwsyTestCase

# A description of each checkpoint and the root path to it.
CHECKPOINTS = [
    {
        'name': "After tabs open [+30s, forced GC]",
        'path': "memory-report-TabsOpenForceGC-4.json.gz",
        'name_filter': 'Web Content', # We only want the content process
        'count': 1 # We only care about the first one
    },
]

# A description of each perfherder suite and the path to its values.
PERF_SUITES = [
    { 'name': "Base Content Resident Unique Memory", 'node': "resident-unique" },
    { 'name': "Base Content Heap Unclassified", 'node': "explicit/heap-unclassified" },
    { 'name': "Base Content JS", 'node': "js-main-runtime/" },
]

class TestMemoryUsage(AwsyTestCase):
    """
    Provides a base case test that just loads about:memory and reports the
    memory usage of a single content process.
    """

    def urls(self):
        return self._urls

    def perf_suites(self):
        return PERF_SUITES

    def perf_checkpoints(self):
        return CHECKPOINTS

    def setUp(self):
        AwsyTestCase.setUp(self)
        self.logger.info("setting up!")

        # Override AwsyTestCase value, this is always going to be 1 iteration.
        self._iterations = 1

        self._urls = ['about:blank'] * 4

        self.logger.info("areweslimyet run by %d pages, %d iterations, %d perTabPause, %d settleWaitTime"
                         % (self._pages_to_load, self._iterations, self._perTabPause, self._settleWaitTime))
        self.logger.info("done setting up!")

    def tearDown(self):
        self.logger.info("tearing down!")
        AwsyTestCase.tearDown(self)
        self.logger.info("done tearing down!")

    def test_open_tabs(self):
        """Marionette test entry that returns an array of checkoint arrays.

        This will generate a set of checkpoints for each iteration requested.
        Upon succesful completion the results will be stored in
        |self.testvars["results"]| and accessible to the test runner via the
        |testvars| object it passed in.
        """
        # setup the results array
        results = [[] for _ in range(self.iterations())]

        def create_checkpoint(name, iteration):
            checkpoint = self.do_memory_report(name, iteration)
            self.assertIsNotNone(checkpoint, "Checkpoint was recorded")
            results[iteration].append(checkpoint)

        for itr in range(self.iterations()):
            self.open_pages()
            self.settle()
            self.settle()
            self.assertTrue(self.do_full_gc())
            self.settle()
            create_checkpoint("TabsOpenForceGC", itr)

        # TODO(ER): Temporary hack until bug 1121139 lands
        self.logger.info("setting results")
        self.testvars["results"] = results
