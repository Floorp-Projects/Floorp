# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from luciddream import LucidDreamTestCase

class TestSample(LucidDreamTestCase):
    def test_sample(self):
        #TODO: make this better
        self.assertIsNotNone(self.marionette.session)
        self.assertIsNotNone(self.browser.session)

    def test_js(self):
        'Test that we can run a JavaScript test in both Marionette instances'
        self.run_js_test('test.js', self.marionette)
        self.run_js_test('test.js', self.browser)
