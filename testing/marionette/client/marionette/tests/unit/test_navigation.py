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

class TestNavigate(MarionetteTestCase):
    def test_navigate(self):
        self.assertTrue(self.marionette.execute_script("window.location.href = 'about:blank'; return true;"))
        self.assertEqual("about:blank", self.marionette.execute_script("return window.location.href;"))
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.assertNotEqual("about:blank", self.marionette.execute_script("return window.location.href;"))
        self.assertEqual("Marionette Test", self.marionette.execute_script("return window.document.title;"))

    def test_getUrl(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.assertTrue(test_html in self.marionette.get_url())
        self.marionette.navigate("about:blank")
        self.assertEqual("about:blank", self.marionette.get_url())

    def test_goBack(self):
        self.assertTrue(self.marionette.execute_script("window.location.href = 'about:blank'; return true;"))
        self.assertEqual("about:blank", self.marionette.execute_script("return window.location.href;"))
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.assertNotEqual("about:blank", self.marionette.execute_script("return window.location.href;"))
        self.assertEqual("Marionette Test", self.marionette.execute_script("return window.document.title;"))
        self.marionette.navigate("about:blank")
        self.assertEqual("about:blank", self.marionette.execute_script("return window.location.href;"))
        self.marionette.go_back()
        self.assertNotEqual("about:blank", self.marionette.execute_script("return window.location.href;"))
        self.assertEqual("Marionette Test", self.marionette.execute_script("return window.document.title;"))

    def test_goForward(self):
        self.assertTrue(self.marionette.execute_script("window.location.href = 'about:blank'; return true;"))
        self.assertEqual("about:blank", self.marionette.execute_script("return window.location.href;"))
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.assertNotEqual("about:blank", self.marionette.execute_script("return window.location.href;"))
        self.assertEqual("Marionette Test", self.marionette.execute_script("return window.document.title;"))
        self.marionette.navigate("about:blank")
        self.assertEqual("about:blank", self.marionette.execute_script("return window.location.href;"))
        self.marionette.go_back()
        self.assertNotEqual("about:blank", self.marionette.execute_script("return window.location.href;"))
        self.assertEqual("Marionette Test", self.marionette.execute_script("return window.document.title;"))
        self.marionette.go_forward()
        self.assertEqual("about:blank", self.marionette.execute_script("return window.location.href;"))

    def test_refresh(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.assertEqual("Marionette Test", self.marionette.execute_script("return window.document.title;"))
        self.assertTrue(self.marionette.execute_script("var elem = window.document.createElement('div'); elem.id = 'someDiv';" +
                                        "window.document.body.appendChild(elem); return true;"))
        self.assertFalse(self.marionette.execute_script("return window.document.getElementById('someDiv') == undefined;"))
        self.marionette.refresh()
        self.assertEqual("Marionette Test", self.marionette.execute_script("return window.document.title;"))
        self.assertTrue(self.marionette.execute_script("return window.document.getElementById('someDiv') == undefined;"))

