# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from marionette_test import MarionetteTestCase

class TestSwitchWindow(MarionetteTestCase):
    def open_new_window(self):
        self.marionette.set_context("chrome")
        self.marionette.set_script_timeout(5000)
        self.marionette.execute_async_script("""
                                        var ww = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                                                 .getService(Components.interfaces.nsIWindowWatcher); 
                                        var win = ww.openWindow(null, "chrome://browser/content/browser.xul", "testWin", null, null);
                                        win.addEventListener("load", function() { 
                                                                        win.removeEventListener("load", arguments.callee, true); 
                                                                        marionetteScriptFinished();
                                                                        }, null);
                                        """)
        self.marionette.set_context("content")

    def close_new_window(self):
        self.marionette.set_context("chrome")
        self.marionette.execute_script("""
                                        var ww = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                                                 .getService(Components.interfaces.nsIWindowWatcher); 
                                        var win = ww.getWindowByName("testWin", null);
                                        if (win != null)
                                          win.close();
                                        """)
        self.marionette.set_context("content")

    def test_windows(self):
        orig_win = self.marionette.current_window_handle
        orig_available = self.marionette.window_handles
        self.open_new_window()
        #assert we're still in the original window
        self.assertEqual(self.marionette.current_window_handle, orig_win)
        now_available = self.marionette.window_handles
        #assert we can find the new window
        self.assertEqual(len(now_available), len(orig_available) + 1) 
        #assert that our window is there
        self.assertTrue(orig_win in now_available)
        new_win = None
        for win in now_available:
            if win != orig_win:
                new_win = orig_win
        #switch to another window
        self.marionette.switch_to_window(new_win)
        self.assertEqual(self.marionette.current_window_handle, new_win)
        #switch back
        self.marionette.switch_to_window(orig_win)
        self.close_new_window()
        self.assertEqual(self.marionette.current_window_handle, orig_win)
        self.assertEqual(len(self.marionette.window_handles), len(orig_available))

    def testShouldLoadAWindowAndThenCloseIt(self):
        test_html = self.marionette.absolute_url("test_windows.html")
        self.marionette.navigate(test_html)
        current = self.marionette.current_window_handle
        
        self.marionette.find_element('link text',"Open new window").click()
        window_handles = self.marionette.window_handles
        window_handles.remove(current)
        
        self.marionette.switch_to_window(window_handles[0])
        self.assertEqual(self.marionette.title, "We Arrive Here")
        
        handle = self.marionette.current_window_handle
        
        self.assertEqual(self.marionette.current_window_handle, handle)
        self.assertEqual(2, len(self.marionette.window_handles))

        # Let's close and check
        self.marionette.close()
        self.marionette.switch_to_window(current)
        self.assertEqual(1, len(self.marionette.window_handles))

    def testShouldCauseAWindowToLoadAndCheckItIsOpenThenCloseIt(self):
        test_html = self.marionette.absolute_url("test_windows.html")
        self.marionette.navigate(test_html)
        current = self.marionette.current_window_handle
        
        self.marionette.find_element('link text',"Open new window").click()
        self.assertEqual(2, len(self.marionette.window_handles))

        # Let's close and check
        self.marionette.close()
        self.marionette.switch_to_window(current)
        self.assertEqual(1, len(self.marionette.window_handles))

    def tearDown(self):
        #ensure that we close the window, regardless of pass/failure
        self.close_new_window()
        MarionetteTestCase.tearDown(self)
