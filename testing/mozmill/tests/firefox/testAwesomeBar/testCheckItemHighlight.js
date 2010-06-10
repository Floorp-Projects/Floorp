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
 *   Tracy Walker <twalker@mozilla.com>
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
 * ***** END LICENSE BLOCK *****/

// Include necessary modules
var RELATIVE_ROOT = '../../shared-modules';
var MODULE_REQUIRES = ['PlacesAPI', 'PrefsAPI','ToolbarAPI'];

const gTimeout = 5000;
const gDelay = 200;

var setupModule = function(module)
{
  module.controller = mozmill.getBrowserController();
  module.locationBar =  new ToolbarAPI.locationBar(controller);

  // Clear complete history so we don't get interference from previous entries
  try {
    var historyService = Cc["@mozilla.org/browser/nav-history-service;1"].
                     getService(Ci.nsINavHistoryService);
    historyService.removeAllPages();
  }
  catch (ex) {}
}

/**
 * Check matched awesomebar items are highlighted.
 */
var testCheckItemHighlight = function()
{
  // Use preferences dialog to select "When Using the location bar suggest:" History and Bookmarks
  PrefsAPI.openPreferencesDialog(prefDialogSuggestsCallback);

  var websites = ['http://www.google.com/', 'about:blank'];

  // Open the test page then about:blank to set up the test test environment
  for (var k = 0; k < websites.length; k++) {
    locationBar.loadURL(websites[k]);
    controller.waitForPageLoad();
  }

  // wait for 4 seconds to work around Firefox LAZY ADD of items to the db
  controller.sleep(4000);

  var testString = "google";

  // Focus the locationbar, delete any contents there, then type in a match string
  locationBar.clear();

  // Use type and sleep on each letter to allow the autocomplete to populate with results.
  for (var i = 0; i < testString.length; i++) {
    locationBar.type(testString[i]);
    controller.sleep(gDelay);
  }

  // Result to check for underlined text
  var richlistItem = locationBar.autoCompleteResults.getResult(0);

  // For the page title check matched text is underlined
  controller.waitForEval('subject.isOpened == true', 3000, 100, locationBar.autoCompleteResults);
  
  var entries = locationBar.autoCompleteResults.getUnderlinedText(richlistItem, "title");
  controller.assertJS("subject.underlinedTextCount == 1",
                      {underlinedTextCount: entries.length})
  for each (entry in entries) {
    controller.assertJS("subject.enteredTitle == subject.underlinedTitle",
                        {enteredTitle: testString, underlinedTitle: entry.toLowerCase()});
  }

  // For the url check matched text is underlined
  entries = locationBar.autoCompleteResults.getUnderlinedText(richlistItem, "url");
  controller.assertJS("subject.underlinedUrlCount == 1",
                      {underlinedUrlCount: entries.length})
  for each (entry in entries) {
    controller.assertJS("subject.enteredUrl == subject.underlinedUrl",
                        {enteredUrl: testString, underlinedUrl: entry.toLowerCase()});
  }
}

/**
 * Set matching of the location bar to "History and Bookmarks"
 *
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 */
var prefDialogSuggestsCallback = function(controller)
{
  var prefDialog = new PrefsAPI.preferencesDialog(controller);
  prefDialog.paneId = 'panePrivacy';

  var suggests = new elementslib.ID(controller.window.document, "locationBarSuggestion");
  controller.waitForElement(suggests);
  controller.select(suggests, null, null, 0);
  controller.sleep(gDelay);

  prefDialog.close(true);
}

/**
 * Map test functions to litmus tests
 */
// testCheckItemHighlight.meta = {litmusids : [8774]};
