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
var MODULE_REQUIRES = ['AddonsAPI', 'PrefsAPI', 'TabbedBrowsingAPI', 'UtilsAPI'];

const gDelay = 0;
const gTimeout = 5000;
const gSearchTimeout = 30000;

var setupModule = function(module)
{
  controller = mozmill.getBrowserController();
  addonsManager = new AddonsAPI.addonsManager();

  TabbedBrowsingAPI.closeAllTabs(controller);
}

var teardownModule = function(module)
{
  addonsManager.close(true);
}

/**
 * Test launching the addons manager
 */
var testLaunchAddonsManager = function()
{
  // Open the addons manager via the menu entry
  addonsManager.open(controller);

  // Verify that panes are visible and can be selected
  for each (pane in ["search", "extensions", "themes", "plugins"]) {
    addonsManager.paneId = pane;

    // Verify the update button is visible for extensions and themes
    if (pane == "extensions" || pane == "themes") {
      var updatesButton = addonsManager.getElement({type: "button_findUpdates"});
      UtilsAPI.assertElementVisible(addonsManager.controller, updatesButton, true);
    }
  }

  addonsManager.close();
}

/**
 * Test the functionality of the get addons tab
 */
var testGetAddonsTab = function()
{
  addonsManager.open(controller);

  // Verify elements of the get addons pane are visible
  addonsManager.paneId = "search";

  var searchField = addonsManager.getElement({type: "search_field"});
  addonsManager.controller.assertProperty(searchField, "hidden", "false");

  var browseAllAddons = addonsManager.getElement({type: "link_browseAddons"});
  addonsManager.controller.assertProperty(browseAllAddons, "hidden", "false");

  var footer = addonsManager.getElement({type: "search_status", subtype: "footer"});
  addonsManager.controller.waitForElement(footer, gSearchTimeout);
  addonsManager.controller.assertProperty(footer, "hidden", false);

  // Verify the number of addons is in-between 0 and the maxResults pref
  var maxResults = PrefsAPI.preferences.getPref("extensions.getAddons.maxResults", -1);
  var listBox = addonsManager.getElement({type: "listbox"});

  addonsManager.controller.assertJS("subject.numSearchResults > 0",
                                    {numSearchResults: listBox.getNode().itemCount});
  addonsManager.controller.assertJS("subject.numSearchResults <= subject.maxResults",
                                    {numSearchResults: listBox.getNode().itemCount,
                                     maxResults: maxResults}
                                   );

  // Check if the see all recommended addons link is the same as the one in prefs
  // XXX: Bug 529412 - Mozmill cannot operate on XUL elements which are outside of the view
  // So we can only compare the URLs for now.
  var footerLabel = addonsManager.getElement({type: "search_statusLabel", value: footer});
  var recommendedUrl = UtilsAPI.formatUrlPref("extensions.getAddons.recommended.browseURL");
  addonsManager.controller.assertJS("subject.correctRecommendedURL",
                                    {correctRecommendedURL:
                                     footerLabel.getNode().getAttribute('recommendedURL') == recommendedUrl}
                                   );

  // Check if the browse all addons link goes to the correct page on AMO
  var browseAddonUrl = UtilsAPI.formatUrlPref("extensions.getAddons.browseAddons");
  addonsManager.controller.waitThenClick(browseAllAddons, gTimeout);

  // The target web page is loaded lazily so wait for the newly created tab first
  controller.waitForEval("subject.tabs.length == 2", gTimeout, 100, controller);
  controller.waitForPageLoad();
  UtilsAPI.assertLoadedUrlEqual(controller, browseAddonUrl);
}

/**
 * Map test functions to litmus tests
 */
// testLaunchAddonsManager.meta = {litmusids : [8154]};
// testGetAddonsTab.meta = {litmusids : [8155]};
