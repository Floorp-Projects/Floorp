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
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Anthony Hughes <ashughes@mozilla.com>
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

var setupModule = function(module) {
  module.controller = mozmill.getBrowserController();
}

var teardownModule = function(module) {
  // Reset the SSL and TLS pref
  PrefsAPI.preferences.clearUserPref("security.enable_ssl3");
  PrefsAPI.preferences.clearUserPref("security.enable_tls");
}

/**
 * Test that SSL and TLS are checked by default
 *
 */
var testDisableSSL = function() {
  // Open a blank page so we don't have any error page shown
  controller.open("about:blank");
  controller.waitForPageLoad();

  PrefsAPI.openPreferencesDialog(prefDialogCallback);

  controller.open("https://www.verisign.com");
  controller.waitForPageLoad(1000);

  // Verify "Secure Connection Failed" error page title
  var title = new elementslib.ID(controller.tabs.activeTab, "errorTitleText");
  controller.waitForElement(title, gTimeout);
  controller.assertJS("subject.errorTitle == 'Secure Connection Failed'",
                      {errorTitle: title.getNode().textContent});

  // Verify "Try Again" button appears
  controller.assertNode(new elementslib.ID(controller.tabs.activeTab, "errorTryAgain"));

  // Verify the error message is correct
  var text = new elementslib.ID(controller.tabs.activeTab, "errorShortDescText");
  controller.waitForElement(text, gTimeout);
  controller.assertJS("subject.errorMessage.indexOf('ssl_error_ssl_disabled') != -1",
                      {errorMessage: text.getNode().textContent});

  controller.assertJS("subject.errorMessage.indexOf('www.verisign.com') != -1",
                      {errorMessage: text.getNode().textContent});

  controller.assertJS("subject.errorMessage.indexOf('SSL protocol has been disabled') != -1",
                      {errorMessage: text.getNode().textContent});
}

/**
 * Disable SSL 3.0 and TLS for secure connections
 *
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 */
var prefDialogCallback = function(controller) {
  var prefDialog = new PrefsAPI.preferencesDialog(controller);
  prefDialog.paneId = 'paneAdvanced';

  // Get the Encryption tab
  var encryption = new elementslib.ID(controller.window.document, "encryptionTab");
  controller.waitThenClick(encryption, gTimeout);
  controller.sleep(gDelay);

  // Make sure the Use SSL pref is not checked
  var sslPref = new elementslib.ID(controller.window.document, "useSSL3");
  controller.waitForElement(sslPref, gTimeout);
  controller.check(sslPref, false);

  // Make sure the Use TLS pref is not checked
  var tlsPref = new elementslib.ID(controller.window.document, "useTLS1");
  controller.waitForElement(tlsPref, gTimeout);
  controller.check(tlsPref, false);

  prefDialog.close(true);
}

/**
 * Map test functions to litmus tests
 */
// testDisableSSL.meta = {litmusids : [9345]};
