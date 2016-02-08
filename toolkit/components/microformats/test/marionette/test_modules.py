# Code example copied from mozilla-central/browser/components/loop/
# need to get this dir in the path so that we make the import work
import os
import sys
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'marionette'))

from microformats_tester import BaseTestFrontendUnits


class TestModulesUnits(BaseTestFrontendUnits):

    def setUp(self):
        super(TestModulesUnits, self).setUp()
        self.set_server_prefix("../module-tests/")

    def test_units(self):
        self.check_page("index.html")
