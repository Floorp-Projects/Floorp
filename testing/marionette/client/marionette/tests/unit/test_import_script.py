# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from marionette_test import MarionetteTestCase
from errors import JavascriptException

class TestImportScript(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)

    def clear_other_context(self):
        self.marionette.set_context("chrome")
        self.marionette.clear_imported_scripts()
        self.marionette.set_context("content")

    def check_file_exists(self):
        return self.marionette.execute_script("""
          let FileUtils = SpecialPowers.Cu.import("resource://gre/modules/FileUtils.jsm").FileUtils;
          let importedScripts = FileUtils.getFile('TmpD', ['marionetteContentScripts']); 
          return importedScripts.exists();
        """, special_powers=True)

    def get_file_size(self):
        return self.marionette.execute_script("""
          let FileUtils = SpecialPowers.Cu.import("resource://gre/modules/FileUtils.jsm").FileUtils;
          let importedScripts = FileUtils.getFile('TmpD', ['marionetteContentScripts']); 
          return importedScripts.fileSize;
        """, special_powers=True)

    def test_import_script(self):
        js = os.path.abspath(os.path.join(__file__, os.path.pardir, "importscript.js"))
        self.marionette.import_script(js)
        self.assertEqual("i'm a test function!", self.marionette.execute_script("return testFunc();"))
        self.assertEqual("i'm a test function!", self.marionette.execute_async_script("marionetteScriptFinished(testFunc());"))

    def test_import_script_twice(self):
        js = os.path.abspath(os.path.join(__file__, os.path.pardir, "importscript.js"))
        self.marionette.import_script(js)
        self.assertEqual("i'm a test function!", self.marionette.execute_script("return testFunc();"))
        self.assertEqual("i'm a test function!", self.marionette.execute_async_script("marionetteScriptFinished(testFunc());"))
        self.assertTrue(self.check_file_exists())
        file_size = self.get_file_size()
        self.assertNotEqual(file_size, None)
        self.marionette.import_script(js)
        file_size = self.get_file_size()
        self.assertEqual(file_size, self.get_file_size())
        self.assertEqual("i'm a test function!", self.marionette.execute_script("return testFunc();"))
        self.assertEqual("i'm a test function!", self.marionette.execute_async_script("marionetteScriptFinished(testFunc());"))

    def test_import_two_scripts_twice(self):
        js = os.path.abspath(os.path.join(__file__, os.path.pardir, "importscript.js"))
        self.marionette.import_script(js)
        self.assertEqual("i'm a test function!", self.marionette.execute_script("return testFunc();"))
        self.assertEqual("i'm a test function!", self.marionette.execute_async_script("marionetteScriptFinished(testFunc());"))
        self.assertTrue(self.check_file_exists())
        file_size = self.get_file_size()
        self.assertNotEqual(file_size, None)
        self.marionette.import_script(js)
        # same script should not append to file
        self.assertEqual(file_size, self.get_file_size())
        self.assertEqual("i'm a test function!", self.marionette.execute_script("return testFunc();"))
        self.assertEqual("i'm a test function!", self.marionette.execute_async_script("marionetteScriptFinished(testFunc());"))
        js = os.path.abspath(os.path.join(__file__, os.path.pardir, "importanotherscript.js"))
        self.marionette.import_script(js)
        new_size = self.get_file_size()
        # new script should append to file
        self.assertNotEqual(file_size, new_size)
        file_size = new_size
        self.assertEqual("i'm yet another test function!",
                    self.marionette.execute_script("return testAnotherFunc();"))
        self.assertEqual("i'm yet another test function!",
                    self.marionette.execute_async_script("marionetteScriptFinished(testAnotherFunc());"))
        self.marionette.import_script(js)
        # same script should not append to file
        self.assertEqual(file_size, self.get_file_size())

    def test_import_script_and_clear(self):
        js = os.path.abspath(os.path.join(__file__, os.path.pardir, "importscript.js"))
        self.marionette.import_script(js)
        self.assertEqual("i'm a test function!", self.marionette.execute_script("return testFunc();"))
        self.assertEqual("i'm a test function!", self.marionette.execute_async_script("marionetteScriptFinished(testFunc());"))
        self.marionette.clear_imported_scripts()
        self.assertFalse(self.check_file_exists())
        self.assertRaises(JavascriptException, self.marionette.execute_script, "return testFunc();")
        self.assertRaises(JavascriptException, self.marionette.execute_async_script, "marionetteScriptFinished(testFunc());")

    def test_import_script_and_clear_in_chrome(self):
        js = os.path.abspath(os.path.join(__file__, os.path.pardir, "importscript.js"))
        self.marionette.import_script(js)
        self.assertTrue(self.check_file_exists())
        file_size = self.get_file_size()
        self.assertEqual("i'm a test function!", self.marionette.execute_script("return testFunc();"))
        self.assertEqual("i'm a test function!", self.marionette.execute_async_script("marionetteScriptFinished(testFunc());"))
        self.clear_other_context()
        # clearing other context's script file should not affect ours
        self.assertTrue(self.check_file_exists())
        self.assertEqual(file_size, self.get_file_size())
        self.assertEqual("i'm a test function!", self.marionette.execute_script("return testFunc();"))
        self.assertEqual("i'm a test function!", self.marionette.execute_async_script("marionetteScriptFinished(testFunc());"))

    def test_importing_another_script_and_check_they_append(self):
        firstjs = os.path.abspath(
                os.path.join(__file__, os.path.pardir, "importscript.js"))
        secondjs = os.path.abspath(
                os.path.join(__file__, os.path.pardir, "importanotherscript.js"))

        self.marionette.import_script(firstjs)
        self.marionette.import_script(secondjs)

        self.assertEqual("i'm a test function!", 
                self.marionette.execute_script("return testFunc();"))

        self.assertEqual("i'm yet another test function!",
                    self.marionette.execute_script("return testAnotherFunc();"))

class TestImportScriptChrome(TestImportScript):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.set_script_timeout(30000)
        self.marionette.set_context("chrome")

    def clear_other_context(self):
        self.marionette.set_context("content")
        self.marionette.clear_imported_scripts()
        self.marionette.set_context("chrome")

    def check_file_exists(self):
        return self.marionette.execute_async_script("""
          Components.utils.import("resource://gre/modules/FileUtils.jsm");
          let checkTimer = Components.classes["@mozilla.org/timer;1"].createInstance(Components.interfaces.nsITimer);
          let f = function() {
            if (typeof FileUtils === 'undefined') {
              checkTimer.initWithCallback(f, 100, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
              return;
            }
            let importedScripts = FileUtils.getFile('TmpD', ['marionetteChromeScripts']); 
            marionetteScriptFinished(importedScripts.exists());
          };
          checkTimer.initWithCallback(f, 100, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
        """)

    def get_file_size(self):
        return self.marionette.execute_async_script("""
          Components.utils.import("resource://gre/modules/FileUtils.jsm");
          let checkTimer = Components.classes["@mozilla.org/timer;1"].createInstance(Components.interfaces.nsITimer);
          let f = function() {
            if (typeof FileUtils === 'undefined') {
              checkTimer.initWithCallback(f, 100, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
              return;
            }
            let importedScripts = FileUtils.getFile('TmpD', ['marionetteChromeScripts']); 
            marionetteScriptFinished(importedScripts.fileSize);
          };
          checkTimer.initWithCallback(f, 100, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
        """)

