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
var MODULE_REQUIRES = ['UtilsAPI'];

const gTimeout = 5000;

var setupModule = function(module) {
  controller = mozmill.getBrowserController();
}

var testGoogleSuggestedTerms = function() {
  // Open the Google web page
  controller.open("http://www.google.com/webhp?complete=1&hl=en");
  controller.waitForPageLoad();

  // Enter a search term into the Google search field
  var searchField = new elementslib.Name(controller.tabs.activeTab, "q");
  controller.type(searchField, "area");

  // The auto-complete box has a different markup for nightly builds
  // Official releases will not have the 'pre' suffix in the version number
  if (UtilsAPI.appInfo.platformVersion.indexOf("pre") == -1) {
    var autoComplete = new elementslib.XPath(controller.tabs.activeTab, "/html/body/span[@id='main']/center/span[@id='body']/center/form/table[2]/tbody/tr[2]/td");
  } else {
    var autoComplete = new elementslib.XPath(controller.tabs.activeTab, "/html/body/center/form/table[1]/tbody/tr/td[2]");
  }

  // Click the first element in the pop-down autocomplete
  controller.waitThenClick(autoComplete, gTimeout);

  // Start the search
  controller.click(new elementslib.Name(controller.tabs.activeTab, "btnG"));
  controller.waitForPageLoad();

  // Check if Search page has come up
  var nextField = new elementslib.Link(controller.tabs.activeTab, "Next");

  controller.waitForElement(searchField, gTimeout);
  controller.waitForElement(nextField, gTimeout);
}

/**
 * Map test functions to litmus tests
 */
// testGoogleSuggestedTerms.meta = {litmusids : [8083]};
