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

// Include necessary modules
var RELATIVE_ROOT = '../../../shared-modules';
var MODULE_REQUIRES = ['SoftwareUpdateAPI', 'UtilsAPI'];

var setupModule = function(module)
{
  module.controller = mozmill.getBrowserController();
  module.update = new SoftwareUpdateAPI.softwareUpdate();

  // Collect some data of the current build
  module.persisted.preBuildId = UtilsAPI.appInfo.buildID;
  module.persisted.preLocale = UtilsAPI.appInfo.locale;
  module.persisted.preUserAgent = UtilsAPI.appInfo.userAgent;
  module.persisted.preVersion = UtilsAPI.appInfo.version;
}

var teardownModule = function(module)
{
  // Save the update properties for later usage
  module.persisted.updateBuildId = update.activeUpdate.buildID;
  module.persisted.updateType = update.isCompleteUpdate ? "complete" : "partial";
  module.persisted.updateVersion = update.activeUpdate.version;
}

/**
 * Download a minor update via the given update channel
 */
var testDirectUpdate_Download = function()
{
  // Check if the user has permissions to run the update
  controller.assertJS("subject.isUpdateAllowed == true",
                      {isUpdateAllowed: update.allowed});

  // Open the software update dialog and wait until the check has been finished
  update.openDialog(controller);
  update.waitForCheckFinished();

  // Check that an update is available
  update.assertUpdateStep('updatesfound');

  // Download the given type of update from the specified channel
  update.download(persisted.type, persisted.channel);

  // We should be ready for restart
  update.assertUpdateStep('finished');
}

/**
 * Map test functions to litmus tests
 */
// testDirectUpdate_Download.meta = {litmusids : [8696]};
