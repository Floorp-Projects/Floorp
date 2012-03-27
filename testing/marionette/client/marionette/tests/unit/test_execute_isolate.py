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

from marionette_test import MarionetteTestCase, skip_if_b2g
from errors import JavascriptException, MarionetteException, ScriptTimeoutException

class TestExecuteIsolationContent(MarionetteTestCase):
    def setUp(self):
        super(TestExecuteIsolationContent, self).setUp()
        self.content = True

    def test_execute_async_isolate(self):
        # Results from one execute call that has timed out should not
        # contaminate a future call.
        multiplier = "*3" if self.content else "*1"
        self.marionette.set_script_timeout(500)
        self.assertRaises(ScriptTimeoutException,
                          self.marionette.execute_async_script,
                          ("setTimeout(function() { marionetteScriptFinished(5%s); }, 3000);"
                               % multiplier))

        self.marionette.set_script_timeout(6000)
        result = self.marionette.execute_async_script("""
setTimeout(function() { marionetteScriptFinished(10%s); }, 5000);
""" % multiplier)
        self.assertEqual(result, 30 if self.content else 10)

class TestExecuteIsolationChrome(TestExecuteIsolationContent):
    def setUp(self):
        super(TestExecuteIsolationChrome, self).setUp()
        self.marionette.set_context("chrome")
        self.content = False


