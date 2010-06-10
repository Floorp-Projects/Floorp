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
var MODULE_REQUIRES = ['PrefsAPI'];

const gDelay = 0;
const gTimeout = 5000;

var setupModule = function(module)
{
  controller = mozmill.getBrowserController();

  module.cm = Cc["@mozilla.org/cookiemanager;1"]
                 .getService(Ci.nsICookieManager2);
  cm.removeAll();
}

var teardownModule = function(module)
{
  PrefsAPI.preferences.clearUserPref("network.cookie.cookieBehavior");
  cm.removeAll();
}

/**
 * Tests disabling cookies from the preferences dialog
 */
var testDisableCookies = function()
{
  // Call preferences dialog and disable cookies
  PrefsAPI.openPreferencesDialog(prefDisableCookieDialogCallback);

  // Go to mozilla.org to build a list of cookies
  controller.open("http://www.mozilla.org/");
  controller.waitForPageLoad();

  // Call preferences dialog and check cookies
  PrefsAPI.openPreferencesDialog(prefCheckDisableDialogCallback);
}

/**
 * Go to the privacy pane and disable saving cookies
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 */
var prefDisableCookieDialogCallback = function(controller)
{
  var prefDialog = new PrefsAPI.preferencesDialog(controller);
  prefDialog.paneId = 'panePrivacy';

  // Go to custom history settings and click on the show cookies button
  var historyMode = new elementslib.ID(controller.window.document, "historyMode");
  controller.waitForElement(historyMode, gTimeout);
  controller.select(historyMode, null, null, "custom");
  controller.sleep(gDelay);

  // Disable cookies
  var acceptCookiesPref = new elementslib.ID(controller.window.document, "acceptCookies");
  controller.check(acceptCookiesPref, false);

  // Close the preferences dialog
  prefDialog.close(true);
}

/**
 * Go to the privacy pane and check that cookies are not saved
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 */
var prefCheckDisableDialogCallback = function(controller)
{
  var prefDialog = new PrefsAPI.preferencesDialog(controller);

  // Go to custom history settings and click on the show cookies button
  var historyMode = new elementslib.ID(controller.window.document, "historyMode");
  controller.waitForElement(historyMode, gTimeout);
  controller.select(historyMode, null, null, "custom");
  controller.sleep(gDelay);

  var showCookies = new elementslib.ID(controller.window.document, "showCookiesButton");
  controller.waitThenClick(showCookies, gTimeout);
  controller.sleep(500);

  try {
    // Grab the cookies manager window
    var window = mozmill.wm.getMostRecentWindow('Browser:Cookies');
    var cmController = new mozmill.controller.MozMillController(window);
  
    // Search for a cookie from mozilla.org and verify cookies are not saved
    var removeCookieButton = new elementslib.ID(cmController.window.document, "removeCookie");
    cmController.waitThenClick(removeCookieButton, gTimeout);
  
    // XXX: Bug 513820 - Remove Cookies button is not cleared when cookie list is cleared
    //cmController.assertProperty(removeCookieButton, "disabled", true);
    cmController.assertJS("subject.cookieCount == 0",
                          {cookieCount : cm.countCookiesFromHost(".mozilla.org")});
  } catch (ex) {
    throw ex;
  } finally {
    // Close the cookies manager
    cmController.keypress(null, "w", {accelKey: true});
    controller.sleep(200);
  }

  prefDialog.close(true);
}

/**
 * Map test functions to litmus tests
 */
// testDisableCookies.meta = {litmusids : [8053]};
