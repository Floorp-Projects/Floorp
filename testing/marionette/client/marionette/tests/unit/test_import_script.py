
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from marionette_test import MarionetteTestCase

class TestImportScript(MarionetteTestCase):
    def test_import_script(self):
        js = os.path.abspath(os.path.join(__file__, os.path.pardir, "importscript.js"))
        self.marionette.import_script(js)
        self.assertEqual("i'm a test function!", self.marionette.execute_script("return testFunc();"))
        self.assertEqual("i'm a test function!", self.marionette.execute_async_script("marionetteScriptFinished(testFunc());"))

class TestImportScriptChrome(TestImportScript):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.set_context("chrome")
