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
 * The Original Code is Mozmill Test Code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aakash Desai <adesai@mozilla.com>
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
var RELATIVE_ROOT = '../../shared-modules';
var MODULE_REQUIRES = ['PrefsAPI', 'TabbedBrowsingAPI', 'UtilsAPI'];

const gDelay = 0;
const gTimeout = 5000;

var setupModule = function(module) {
  module.controller = mozmill.getBrowserController();

  TabbedBrowsingAPI.closeAllTabs(controller);
}

var teardownModule = function(module) {
  PrefsAPI.preferences.clearUserPref("browser.startup.homepage");
}

/**
 * Restore home page to default
 */
var testRestoreHomeToDefault = function() {
  // Open a web page for the temporary home page
  controller.open('http://www.mozilla.org/');
  controller.waitForPageLoad();

  var link = new elementslib.Link(controller.tabs.activeTab, "Mozilla");
  controller.assertNode(link);

  // Call Preferences dialog and set home page
  PrefsAPI.openPreferencesDialog(prefDialogHomePageCallback);

  // Go to the saved home page and verify it's the correct page
  controller.click(new elementslib.ID(controller.window.document, "home-button"));
  controller.waitForPageLoad();
  controller.assertNode(link);

  // Open Preferences dialog and reset home page to default
  PrefsAPI.openPreferencesDialog(prefDialogDefHomePageCallback);
}

/**
 * Set the current page as home page via the preferences dialog
 *
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 */
var prefDialogHomePageCallback = function(controller) {
  var prefDialog = new PrefsAPI.preferencesDialog(controller);
  prefDialog.paneId = 'paneMain';

  // Set home page to the current page
  var useCurrent = new elementslib.ID(controller.window.document, "useCurrent");
  controller.waitThenClick(useCurrent);
  controller.sleep(gDelay);

  prefDialog.close(true);
}

var prefDialogDefHomePageCallback = function(controller) {
  var prefDialog = new PrefsAPI.preferencesDialog(controller);

  // Reset home page to the default page
  var useDefault = new elementslib.ID(controller.window.document, "restoreDefaultHomePage");
  controller.waitForElement(useDefault, gTimeout);
  controller.click(useDefault);

  // Check the browserconfig file to get the get default homepage
  var defaultHomePage = UtilsAPI.getProperty("resource:/browserconfig.properties", "browser.startup.homepage");
  var browserHomePage = new elementslib.ID(controller.window.document, "browserHomePage");
  controller.assertValue(browserHomePage, defaultHomePage);

  prefDialog.close();
}

/**
 * Map test functions to litmus tests
 */
// testRestoreHomeToDefault.meta = {litmusids : [8327]};
