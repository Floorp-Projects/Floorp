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
const gTimeout = 200;

var setupModule = function(module) {
  module.controller = mozmill.getBrowserController();

  // Clear complete form history so we don't interfer with already added entries
  var formHistory = Cc["@mozilla.org/satchel/form-history;1"]
                       .getService(Ci.nsIFormHistory2);
  formHistory.removeAllEntries();
}

var teardownModule = function(module) {
  PrefsAPI.preferences.clearUserPref("browser.formfill.enable");
}

var testToggleFormManager = function() {
  // Open Preferences dialog and uncheck save form and search history in the privacy pane
  PrefsAPI.openPreferencesDialog(prefDialogFormCallback);

  var url = "http://www-archive.mozilla.org/wallet/samples/sample9.html";

  // Go to the sample form website and submit form data
  controller.open(url);
  controller.waitForPageLoad();

  var firstName = new elementslib.Name(controller.tabs.activeTab, "ship_fname");
  var fname = "John";
  var lastName = new elementslib.Name(controller.tabs.activeTab, "ship_lname");
  var lname = "Smith";

  controller.type(firstName, fname);
  controller.type(lastName, lname);

  controller.click(new elementslib.Name(controller.tabs.activeTab, "SubmitButton"));
  controller.waitForPageLoad();
  controller.waitForElement(firstName, gTimeout);

  // Verify no form completion in each submitted form field
  var popDownAutoCompList = new elementslib.Lookup(controller.window.content.document, '/id("main-window")/id("mainPopupSet")/id("PopupAutoComplete")/anon({"anonid":"tree"})/{"class":"autocomplete-treebody"}');

  controller.type(firstName, fname.substring(0,2));
  controller.sleep(gTimeout);
  controller.assertNodeNotExist(popDownAutoCompList);
  controller.assertValue(firstName, fname.substring(0,2));

  controller.type(lastName, lname.substring(0,2));
  controller.sleep(gTimeout);
  controller.assertNodeNotExist(popDownAutoCompList);
  controller.assertValue(lastName, lname.substring(0,2));
}

/**
 * Use preferences dialog to disable the form manager
 *
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 */
var prefDialogFormCallback = function(controller) {
  var prefDialog = new PrefsAPI.preferencesDialog(controller);
  prefDialog.paneId = 'panePrivacy';

  // Select custom settings for history and uncheck remember search and form history
  var historyMode = new elementslib.ID(controller.window.document, "historyMode");
  controller.waitForElement(historyMode);
  controller.select(historyMode, null, null, "custom");

  var rememberForms = new elementslib.ID(controller.window.document, "rememberForms");
  controller.waitThenClick(rememberForms);
  controller.sleep(gDelay);

  prefDialog.close(true);
}

/**
 * Map test functions to litmus tests
 */
// testToggleFormManager.meta = {litmusids : [8050]};
