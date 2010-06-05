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

const gDelay = 0;
const gTimeout = 5000;

var setupModule = function(module) {
  module.controller = mozmill.getBrowserController();
}

var testAutoCompleteOff = function() {
  var url = "http://www.google.com/webhp?complete=1&hl=en";
  var searchTerm = "mozillazine";

  // Open the google homepage
  controller.open(url);
  controller.waitForPageLoad();

  // Search for mozillazine on google
  var searchField = new elementslib.Name(controller.tabs.activeTab, "q");
  var submitButton = new elementslib.Name(controller.tabs.activeTab, "btnG");

  controller.waitForElement(searchField);
  controller.type(searchField, searchTerm);
  controller.click(submitButton);

  // Go back to the search page
  controller.open(url);
  controller.waitForPageLoad();

  // Enter a part of the search term only
  controller.waitForElement(searchField, gTimeout);
  controller.type(searchField, searchTerm.substring(0, 3));
  controller.sleep(500);

  // Verify source autocomplete=off
  var popupAutoCompList = new elementslib.ID(controller.window.document, "PopupAutoComplete");
  controller.assertProperty(popupAutoCompList, "popupOpen", false);
}

/**
 * Map test functions to litmus tests
 */
// testAutoCompleteOff.meta = {litmusids : [9067]};
