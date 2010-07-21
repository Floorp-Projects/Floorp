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

var RELATIVE_ROOT = '../../shared-modules';
var MODULE_REQUIRES = ['ModalDialogAPI', 'PrefsAPI', 'PrivateBrowsingAPI',
                       'TabbedBrowsingAPI', 'UtilsAPI'];

const gDelay = 0;
const gTimeout = 5000;

var websites = [
                "https://litmus.mozilla.org/testcase_files/firefox/5918/index.html",
                "https://litmus.mozilla.org/testcase_files/firefox/cookies/cookie_single.html"
               ];

var setupModule = function(module)
{
  controller = mozmill.getBrowserController();

  // Create Private Browsing instance
  pb = new PrivateBrowsingAPI.privateBrowsing(controller);
}

var teardownModule = function(module)
{
  pb.reset();

  // Reset the user cookie pref
  PrefsAPI.preferences.clearUserPref("network.cookie.lifetimePolicy");
}

/**
 * Verify various permissions are disabled when in Private Browsing mode
 */
var testPermissionsDisabled = function()
{
  // Make sure we are not in PB mode and don't show a prompt
  pb.enabled = false;
  pb.showPrompt = false;

  TabbedBrowsingAPI.closeAllTabs(controller);

  pb.start();

  // Check that the "allow" button for pop-ups is disabled in the preferences
  controller.open(websites[0]);
  controller.waitForPageLoad();

  // Open context menu and check "Allow Popups" is disabled
  var property = mozmill.isWindows ? "popupWarningButton.accesskey" : "popupWarningButtonUnix.accesskey";
  var accessKey = UtilsAPI.getProperty("chrome://browser/locale/browser.properties", property);

  controller.keypress(null, accessKey, {ctrlKey: mozmill.isMac, altKey: !mozmill.isMac});

  var allow = new elementslib.XPath(controller.window.document, "/*[name()='window']/*[name()='popupset'][1]/*[name()='popup'][2]/*[name()='menuitem'][1]");

  controller.waitForElement(allow);
  controller.assertProperty(allow, "disabled", true);

  controller.keypress(null, "VK_ESCAPE", {});

  // Enable the "Ask me every time" cookie behavior
  PrefsAPI.openPreferencesDialog(prefCookieHandler);

  // No cookie dialog should show up
  controller.open(websites[1]);
  controller.waitForPageLoad();
}

/**
 * Select "Ask me every time" for Cookies
 *
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 */
var prefCookieHandler = function(controller)
{
  var prefDialog = new PrefsAPI.preferencesDialog(controller);
  prefDialog.paneId = 'panePrivacy';

  // Go to custom history settings and select ask me every time for cookies
  var historyMode = new elementslib.ID(controller.window.document, "historyMode");
  controller.waitForElement(historyMode, gTimeout);
  controller.select(historyMode, null, null, "custom");
  controller.sleep(gDelay);

  var acceptCookies = new elementslib.ID(controller.window.document, "acceptCookies");
  controller.assertChecked(acceptCookies);

  // Select "ask me every time"
  var keepCookies = new elementslib.ID(controller.window.document, "keepCookiesUntil");
  controller.waitForElement(keepCookies, gTimeout);
  controller.select(keepCookies, null, null, 1);
  controller.sleep(gDelay);

  prefDialog.close(true);
}

/**
 * Just in case we open a modal dialog we have to mark the test as failed
 */
var cookieHandler = function(controller)
{
  var button = new elementslib.ID(controller.window.document, "ok");
  controller.assertNodeNotExist(button);
}

/**
 * Map test functions to litmus tests
 */
// testPermissionsDisabled.meta = {litmusids : [9157]};
