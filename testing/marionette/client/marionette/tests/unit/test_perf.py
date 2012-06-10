# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from marionette_test import MarionetteTestCase

class TestPerf(MarionetteTestCase):
    def test_perf_basic(self):
        self.marionette.add_perf_data("testgroup", "testperf", 10)
        data = self.marionette.get_perf_data()
        self.assertTrue(data.has_key("testgroup"))
        self.assertTrue(data["testgroup"].has_key("testperf"))
        self.assertEqual(10, data["testgroup"]["testperf"][0])
        self.marionette.add_perf_data("testgroup", "testperf", 20)
        data = self.marionette.get_perf_data()
        self.assertTrue(data.has_key("testgroup"))
        self.assertTrue(data["testgroup"].has_key("testperf"))
        self.assertEqual(20, data["testgroup"]["testperf"][1])
        self.marionette.add_perf_data("testgroup", "testperf2", 20)
        data = self.marionette.get_perf_data()
        self.assertTrue(data.has_key("testgroup"))
        self.assertTrue(data["testgroup"].has_key("testperf2"))
        self.assertEqual(20, data["testgroup"]["testperf2"][0])
        self.marionette.add_perf_data("testgroup2", "testperf3", 30)
        data = self.marionette.get_perf_data()
        self.assertTrue(data.has_key("testgroup2"))
        self.assertTrue(data["testgroup2"].has_key("testperf3"))
        self.assertEqual(30, data["testgroup2"]["testperf3"][0])

    def test_perf_script(self):
        self.marionette.execute_script("addPerfData('testgroup', 'testperf', 10);")
        data = self.marionette.get_perf_data()
        self.assertTrue(data.has_key("testgroup"))
        self.assertTrue(data["testgroup"].has_key("testperf"))
        self.assertEqual(10, data["testgroup"]["testperf"][0])
        self.marionette.execute_script("addPerfData('testgroup', 'testperf', 20);")
        data = self.marionette.get_perf_data()
        self.assertTrue(data.has_key("testgroup"))
        self.assertTrue(data["testgroup"].has_key("testperf"))
        self.assertEqual(20, data["testgroup"]["testperf"][1])
        self.marionette.execute_script("addPerfData('testgroup', 'testperf2', 20);")
        data = self.marionette.get_perf_data()
        self.assertTrue(data.has_key("testgroup"))
        self.assertTrue(data["testgroup"].has_key("testperf2"))
        self.assertEqual(20, data["testgroup"]["testperf2"][0])
        self.marionette.execute_script("addPerfData('testgroup2', 'testperf3', 30);")
        data = self.marionette.get_perf_data()
        self.assertTrue(data.has_key("testgroup2"))
        self.assertTrue(data["testgroup2"].has_key("testperf3"))
        self.assertEqual(30, data["testgroup2"]["testperf3"][0])

class TestPerfChrome(TestPerf):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.set_context("chrome")

