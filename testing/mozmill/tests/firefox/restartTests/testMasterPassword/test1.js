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
var RELATIVE_ROOT = '../../../shared-modules';
var MODULE_REQUIRES = ['ModalDialogAPI','PrefsAPI', 'TabbedBrowsingAPI', 'UtilsAPI'];

const gDelay = 0;
const gTimeout = 5000;

var setupModule = function(module)
{
  controller = mozmill.getBrowserController();
  tabBrowser = new TabbedBrowsingAPI.tabBrowser(controller);
}

var teardownModule = function(module)
{
  // Remove all logins set by the testscript
  var pm = Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
  pm.removeAllLogins();
}

/**
 * Test saving login information and setting a master password
 */
var testSetMasterPassword = function()
{
  var testSite = "http://www-archive.mozilla.org/quality/browser/front-end/testcases/wallet/login.html";

  // Go to the sample login page and perform a test log-in with inputted fields
  controller.open(testSite);
  controller.waitForPageLoad();

  var userField = new elementslib.ID(controller.tabs.activeTab, "uname");
  var passField = new elementslib.ID(controller.tabs.activeTab, "Password");

  controller.waitForElement(userField, gTimeout);
  controller.type(userField, "bar");
  controller.type(passField, "foo");

  controller.waitThenClick(new elementslib.ID(controller.tabs.activeTab, "LogIn"), gTimeout);
  controller.sleep(500);

  // After logging in, remember the login information
  var label = UtilsAPI.getProperty("chrome://passwordmgr/locale/passwordmgr.properties",
                                   "notifyBarRememberButtonText");
  var button = tabBrowser.getTabPanelElement(tabBrowser.selectedIndex,
                                             '/{"value":"password-save"}/{"label":"' + label + '"}');

  UtilsAPI.assertElementVisible(controller, button, true);
  controller.waitThenClick(button, gTimeout);
  controller.sleep(500);
  controller.assertNodeNotExist(button);

  // Call preferences dialog and invoke master password functionality
  PrefsAPI.openPreferencesDialog(prefDialogSetMasterPasswordCallback);
}

/**
 * Test invoking master password dialog when opening password manager
 */
var testInvokeMasterPassword = function()
{
  // Call preferences dialog and invoke master password functionality
  PrefsAPI.openPreferencesDialog(prefDialogInvokeMasterPasswordCallback);
}

/**
 * Test removing the master password
 */
var testRemoveMasterPassword = function()
{
  // Call preferences dialog and invoke master password functionality
  PrefsAPI.openPreferencesDialog(prefDialogDeleteMasterPasswordCallback);
}

/**
 * Handler for preferences dialog to set the Master Password
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 */
var prefDialogSetMasterPasswordCallback = function(controller)
{
  var prefDialog = new PrefsAPI.preferencesDialog(controller);

  prefDialog.paneId = 'paneSecurity';

  var masterPasswordCheck = new elementslib.ID(controller.window.document, "useMasterPassword");
  controller.waitForElement(masterPasswordCheck, gTimeout);
  controller.sleep(gDelay);

  // Call setMasterPassword dialog and set a master password to your profile
  var md = new ModalDialogAPI.modalDialog(masterPasswordHandler);
  md.start(200);

  controller.click(masterPasswordCheck);

  // Close the Preferences dialog
  prefDialog.close(true);
}

/**
 * Set the master password via the master password dialog
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 */
var masterPasswordHandler = function(controller)
{
  var pw1 = new elementslib.ID(controller.window.document, "pw1");
  var pw2 = new elementslib.ID(controller.window.document, "pw2");

  // Fill in the master password into both input fields and click ok
  controller.waitForElement(pw1, gTimeout);
  controller.type(pw1, "test1");
  controller.type(pw2, "test1");

  // Call the confirmation dialog and click ok to go back to the preferences dialog
  var md = new ModalDialogAPI.modalDialog(confirmHandler);
  md.start(200);

  var button = new elementslib.Lookup(controller.window.document,
                           '/id("changemp")/anon({"anonid":"buttons"})/{"dlgtype":"accept"}');
  controller.waitThenClick(button, gTimeout);
}

/**
 * Call the confirmation dialog and click ok to go back to the preferences dialog
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 */
var confirmHandler = function(controller)
{
  var button = new elementslib.Lookup(controller.window.document,
                               '/id("commonDialog")/anon({"anonid":"buttons"})/{"dlgtype":"accept"}');
  controller.waitThenClick(button, gTimeout);
}

/**
 * Bring up the master password dialog via the preferences window
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 */
var prefDialogInvokeMasterPasswordCallback = function(controller)
{
  var prefDialog = new PrefsAPI.preferencesDialog(controller);

  prefDialog.paneId = 'paneSecurity';

  var showPasswordButton = new elementslib.ID(controller.window.document, "showPasswords");
  controller.waitForElement(showPasswordButton, gTimeout);

  // Call showPasswords dialog and view the passwords on your profile
  var md = new ModalDialogAPI.modalDialog(checkMasterHandler);
  md.start(200);

  controller.click(showPasswordButton);

  // Check if the password manager has been opened
  controller.sleep(500);

  var window = mozmill.wm.getMostRecentWindow('Toolkit:PasswordManager');
  var pwdController = new mozmill.controller.MozMillController(window);

  var togglePasswords = new elementslib.ID(pwdController.window.document, "togglePasswords");
  var passwordCol = new elementslib.ID(pwdController.window.document, "passwordCol");

  pwdController.waitForElement(togglePasswords, gTimeout);
  UtilsAPI.assertElementVisible(pwdController, passwordCol, false);

  // Call showPasswords dialog and view the passwords on your profile
  var md = new ModalDialogAPI.modalDialog(checkMasterHandler);
  md.start(200);

  pwdController.click(togglePasswords);

  UtilsAPI.assertElementVisible(pwdController, passwordCol, true);

  // Close the password manager and the preferences dialog
  pwdController.keypress(null, "w", {accelKey: true});
  pwdController.sleep(200);

  prefDialog.close(true);
}

/**
 * Verify the master password dialog is invoked
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 */
var checkMasterHandler = function(controller)
{
  var passwordBox = new elementslib.ID(controller.window.document, "password1Textbox");

  controller.waitForElement(passwordBox, gTimeout);
  controller.type(passwordBox, "test1");

  var button = new elementslib.Lookup(controller.window.document,
                               '/id("commonDialog")/anon({"anonid":"buttons"})/{"dlgtype":"accept"}');
  controller.click(button);
}

/**
 * Delete the master password using the preferences window
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 */
var prefDialogDeleteMasterPasswordCallback = function(controller)
{
  var prefDialog = new PrefsAPI.preferencesDialog(controller);

  prefDialog.paneId = 'paneSecurity';

  var masterPasswordCheck = new elementslib.ID(controller.window.document, "useMasterPassword");
  controller.waitForElement(masterPasswordCheck, gTimeout);

  // Call setMasterPassword dialog and remove the master password to your profile
  var md = new ModalDialogAPI.modalDialog(removeMasterHandler);
  md.start(200);

  controller.click(masterPasswordCheck);

  // Close the Preferences dialog
  prefDialog.close(true);
}

/**
 * Remove the master password via the master password dialog
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 */
var removeMasterHandler = function(controller)
{
  var removePwdField = new elementslib.ID(controller.window.document, "password");

  controller.waitForElement(removePwdField, gTimeout);
  controller.type(removePwdField, "test1");

  // Call the confirmation dialog and click ok to go back to the preferences dialog
  var md = new ModalDialogAPI.modalDialog(confirmHandler);
  md.start(200);

  controller.click(new elementslib.Lookup(controller.window.document,
                   '/id("removemp")/anon({"anonid":"buttons"})/{"dlgtype":"accept"}'));
}

/**
 * Map test functions to litmus tests
 */
// testSetMasterPassword.meta = {litmusids : [8316]};
// testInvokeMasterPassword.meta = {litmusids : [8108]};
// testRemoveMasterPassword.meta = {litmusids : [8317]};
