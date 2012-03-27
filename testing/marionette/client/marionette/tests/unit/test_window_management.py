# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1 
# 
# The contents of this file are subject to the Mozilla Public License Version 
# 1.1 (the "License"); you may not use this file except in compliance with 
# the License. You may obtain a copy of the License at 
# http://www.mozilla.org/MPL/ # 
# Software distributed under the License is distributed on an "AS IS" basis, 
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License 
# for the specific language governing rights and limitations under the 
# License. 
# 
# The Original Code is Marionette Client. 
# 
# The Initial Developer of the Original Code is 
#   Mozilla Foundation. 
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved. 
# 
# Contributor(s): 
# 
# Alternatively, the contents of this file may be used under the terms of 
# either the GNU General Public License Version 2 or later (the "GPL"), or 
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"), 
# in which case the provisions of the GPL or the LGPL are applicable instead 
# of those above. If you wish to allow use of your version of this file only 
# under the terms of either the GPL or the LGPL, and not to allow others to 
# use your version of this file under the terms of the MPL, indicate your 
# decision by deleting the provisions above and replace them with the notice 
# and other provisions required by the GPL or the LGPL. If you do not delete 
# the provisions above, a recipient may use your version of this file under 
# the terms of any one of the MPL, the GPL or the LGPL. 
# 
# ***** END LICENSE BLOCK ***** 

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
        orig_win = self.marionette.get_window()
        orig_available = self.marionette.get_windows()
        self.open_new_window()
        #assert we're still in the original window
        self.assertEqual(self.marionette.get_window(), orig_win)
        now_available = self.marionette.get_windows()
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
        self.assertEqual(self.marionette.get_window(), new_win)
        #switch back
        self.marionette.switch_to_window(orig_win)
        self.close_new_window()
        self.assertEqual(self.marionette.get_window(), orig_win)
        self.assertEqual(len(self.marionette.get_windows()), len(orig_available))

    def tearDown(self):
        #ensure that we close the window, regardless of pass/failure
        self.close_new_window()
        MarionetteTestCase.tearDown(self)
