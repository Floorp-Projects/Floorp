/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is MozMill Test code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henrik Skupin <hskupin@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// File names for crash reporter application on all platforms
const fileNames = {
                   "darwin" : "crashreporter.app",
                   "win32"  : "crashreporter.exe",
                   "linux"  : "crashreporter"
                  };

// Expected states of the crash reporter
const states = {
                "Enabled" : true,
                "ServerURL" : "https://crash-reports.mozilla.com/submit"
               };

var setupModule = function(module)
{
  controller = mozmill.getBrowserController();

  // Get the application folder
  var dirService = Cc["@mozilla.org/file/directory_service;1"].
                   getService(Ci.nsIProperties);
  module.appDir = dirService.get("XCurProcD", Ci.nsILocalFile);

  // Get the crash reporter service
  module.crashReporter = Cc["@mozilla.org/toolkit/crash-reporter;1"]
                           .getService(Ci.nsICrashReporter);
}

/**
 * Test that Breakpad is installed
 */
var testBreakpadInstalled = function()
{
  // Check that the crash reporter executable is present
  var execFile = Cc["@mozilla.org/file/local;1"]
                    .createInstance(Ci.nsILocalFile);
  execFile.initWithPath(appDir.path);
  execFile.append(fileNames[mozmill.platform]);

  controller.assertJS("subject.exists() == true", execFile);

  // Is the crash reporter enabled?
  controller.assertJS("subject.reporter.enabled == subject.stateEnabled",
                      {reporter: crashReporter, stateEnabled: states['Enabled']});

  // Do we have the correct server URL?
  controller.assertJS("subject.reporter.serverURL.spec == subject.stateServerURL",
                      {reporter: crashReporter, stateServerURL: states['ServerURL']});
}

/**
 * Map test functions to litmus tests
 */
// testBreakpadInstalled.meta = {litmusids : [7953]};
