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
 * The Initial Developer of the Original Code is the Mozilla Foundation.
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
var MODULE_REQUIRES = ['AddonsAPI', 'PrefsAPI'];

const gDelay = 0;
const gTimeout = 5000;
const gSearchTimeout = 30000;

var setupModule = function(module)
{
  controller = mozmill.getBrowserController();
  addonsManager = new AddonsAPI.addonsManager();
}

var teardownModule = function(module)
{
  addonsManager.close();
}

/**
 * Test the search for Add-ons
 */
var testSearchForAddons = function() 
{
  addonsManager.open(controller);
  controller = addonsManager.controller;

  addonsManager.search("rss");

  // Wait for search results to populate and verify elements of search functionality
  var footer = addonsManager.getElement({type: "search_status", subtype: "footer"});
  controller.waitForElement(footer, gSearchTimeout);
  controller.assertProperty(footer, "hidden", false);

  // Check if we show the x button in the search field
  var searchButton = addonsManager.getElement({type: "search_fieldButton"});
  var buttonPanel = searchButton.getNode().selectedPanel;
  controller.assertJS("subject.isClearButtonShown == true",
                      {isClearButtonShown: buttonPanel.getAttribute('class') == 'textbox-search-clear'});

  // Verify the number of addons is in-between 0 and the maxResults pref
  var maxResults = PrefsAPI.preferences.getPref("extensions.getAddons.maxResults", -1);
  var listBox = addonsManager.getElement({type: "listbox"});

  addonsManager.controller.assertJS("subject.numSearchResults > 0",
                                    {numSearchResults: listBox.getNode().itemCount});
  addonsManager.controller.assertJS("subject.numSearchResults <= subject.maxResults",
                                    {numSearchResults: listBox.getNode().itemCount,
                                     maxResults: maxResults}
                                   );

  // Clear the search field and verify elements of that functionality
  var searchField = addonsManager.getElement({type: "search_field"});
  controller.keypress(searchField, "VK_ESCAPE", {});

  buttonPanel = searchButton.getNode().selectedPanel;
  controller.assertJS("subject.isClearButtonShown == true",
                      {isClearButtonShown: buttonPanel.getAttribute('class') != 'textbox-search-clear'});
  controller.assertValue(searchField, "");

  // We still have to show the footer with recommended addons
  controller.waitForElement(footer, gSearchTimeout);
  controller.assertProperty(footer, "hidden", false);

  // Verify the number of recommended addons is in-between 0 and the maxResults pref
  addonsManager.controller.assertJS("subject.numSearchResults > 0",
                                    {numSearchResults: listBox.getNode().itemCount});
  addonsManager.controller.assertJS("subject.numSearchResults <= subject.maxResults",
                                    {numSearchResults: listBox.getNode().itemCount,
                                     maxResults: maxResults}
                                   );
}

/**
 * Map test functions to litmus tests
 */
// testSearchForAddons.meta = {litmusids : [8825]};
