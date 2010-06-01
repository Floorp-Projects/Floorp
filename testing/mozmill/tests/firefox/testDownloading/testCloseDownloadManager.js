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
 *   Anthony Hughes <anthony.s.hughes@gmail.com>
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

var RELATIVE_ROOT = '../../shared-modules';
var MODULE_REQUIRES = ['DownloadsAPI', 'UtilsAPI'];

const gDelay = 0;
const gTimeout = 5000;

var setupModule = function(module)
{
  module.controller = mozmill.getBrowserController();

  // Get an instance of the Download Manager class
  module.dm = new DownloadsAPI.downloadManager();
}

var teardownModule = function(module)
{
  // If we failed in closing the Download Manager window do it now
  if (dm.controller.window)
    dm.controller.window.close();
}

/**
 * Test closing the Download Manager
 */
var testCloseDownloadManager = function()
{
  // Get the initial window count
  var windowCount = mozmill.utils.getWindows().length;

  // Test ESC
  dm.open(controller, false);
  dm._controller.keypress(null, "VK_ESCAPE", {});
  controller.waitForEval("subject.getWindows().length == " + windowCount,
                         gTimeout, 100, mozmill.utils);

  // Test ACCEL+W
  // This is tested by dm.close()
  // L10N: Entity cmd.close.commandKey in downloads.dtd#35
  //       All locales use 'w' so harcoded is locale-safe
  //       Use DTD Helper once it becomes available (Bug 504635)
  dm.open(controller, false);
  dm.close();
  
  // Test ACCEL+SHIFT+Y
  // NOTE: This test is only performed on Linux
  // L10N: Entity cmd.close2Unix.commandKey in downloads.dtd#35
  //       All locales use 'y' so harcoded is locale-safe
  //       Use DTD Helper once it becomes available (Bug 504635)
  if (mozmill.isLinux) {
    dm.open(controller, false);
    dm._controller.keypress(null, 'y', {shiftKey:true, accelKey:true});
    controller.waitForEval("subject.getWindows().length == " + windowCount,
                           gTimeout, 100, mozmill.utils); 
  }

  // Test ACCEL+J
  // NOTE: This test is only performed on Windows and Mac
  // L10N: Entity cmd.close2.commandKey in downloads.dtd#35
  //       All locales use 'j' so harcoded is locale-safe
  //       Use DTD Helper once it becomes available (Bug 504635)
  if (!mozmill.isLinux) {
    dm.open(controller, false);
    dm._controller.keypress(null, 'j', {accelKey:true});
    controller.waitForEval("subject.getWindows().length == " + windowCount,
                           gTimeout, 100, mozmill.utils); 
  }
}

/**
 * Map test functions to litmus tests
 */
// testCloseDownloadManager.meta = {litmusids : [7980]};
