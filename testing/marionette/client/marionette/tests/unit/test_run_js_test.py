# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from marionette_test import MarionetteTestCase
import unittest

class TestRunJSTest(MarionetteTestCase):
    def test_basic(self):
        self.run_js_test('test_simpletest_pass.js')
        self.run_js_test('test_simpletest_fail.js')
