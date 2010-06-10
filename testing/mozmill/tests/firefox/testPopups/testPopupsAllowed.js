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
 *   Anthony Hughes <ahughes@mozilla.com>
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
var MODULE_REQUIRES = ['PrefsAPI', 'TabbedBrowsingAPI', 'UtilsAPI'];

const gDelay = 0;
const gTimeout = 5000;

var setupModule = function(module)
{
  controller = mozmill.getBrowserController();
  tabBrowser = new TabbedBrowsingAPI.tabBrowser(controller);
  tabBrowser.closeAllTabs();
}

var teardownModule = function(module)
{
  // Reset the pop-up blocking pref
  PrefsAPI.preferences.clearUserPref("dom.disable_open_during_load");

  for each (window in mozmill.utils.getWindows("navigator:browser")) {
    if (!window.toolbar.visible)
      window.close();
  }
}

/**
 * Test to make sure pop-ups are not blocked
 *
 */
var testPopUpAllowed = function()
{
  var url = "https://litmus.mozilla.org/testcase_files/firefox/5918/index.html";

  PrefsAPI.openPreferencesDialog(prefDialogCallback);

  // Get the window count
  var windowCount = mozmill.utils.getWindows().length;

  // Open the Pop-up test site
  controller.open(url);
  controller.waitForPageLoad();

  // A notification bar always exists in the DOM so check the visibility of the X button
  var button = tabBrowser.getTabPanelElement(tabBrowser.selectedIndex,
                                             '/{"value":"popup-blocked"}/anon({"type":"warning"})' +
                                             '/{"class":"messageCloseButton tabbable"}');
  controller.assertNodeNotExist(button);

  // Check for the status bar icon
  var cssInfo = controller.window.getComputedStyle(controller.window.document.getElementById("page-report-button"), "");
  controller.assertJS("subject.isReportButtonVisible == false",
                      {isReportButtonVisible: cssInfo.getPropertyValue('display') != 'none'});

  // Check that the window count has changed
  controller.assertJS("subject.preWindowCount != subject.postWindowCount",
                      {preWindowCount: windowCount, postWindowCount: mozmill.utils.getWindows().length});
}

/**
 * Call-back handler for preferences dialog
 *
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 */
var prefDialogCallback = function(controller) {
  var prefDialog = new PrefsAPI.preferencesDialog(controller);
  prefDialog.paneId = 'paneContent';

  // Make sure the pref is unchecked
  var pref = new elementslib.ID(controller.window.document, "popupPolicy");
  controller.waitForElement(pref, gTimeout);
  controller.check(pref, false);

  prefDialog.close(true);
}

/**
 * Map test functions to litmus tests
 */
// testPopUpAllowed.meta = {litmusids : [8367]};
