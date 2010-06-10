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
var MODULE_REQUIRES = ['TabbedBrowsingAPI', 'UtilsAPI'];

const gDelay = 0;
const gTimeout = 5000;

var setupModule = function(module)
{
  controller = mozmill.getBrowserController();

  tabBrowser = new TabbedBrowsingAPI.tabBrowser(controller);
  tabBrowser.closeAllTabs();
}

var testNewTab = function()
{
  // Ensure current tab does not have blank page loaded
  var section = new elementslib.ID(controller.tabs.activeTab, "sub");

  controller.open('http://www.mozilla.org');
  controller.waitForPageLoad();
  controller.waitForElement(section, gTimeout);

  // Test all different ways to open a tab
  checkOpenTab({type: "menu"});
  checkOpenTab({type: "shortcut"});
  checkOpenTab({type: "tabStrip"});
  checkOpenTab({type: "newTabButton"});
}

/**
 * Check if a new tab has been opened, has a title and can be closed
 *
 * @param {object} event
 *        Object which specifies how to open the new tab
 */
var checkOpenTab = function(event)
{
  // Open a new tab and check that 'about:blank' has been opened
  tabBrowser.openTab(event);
  controller.waitForEval("subject.length == 2", gTimeout, 100, controller.tabs);
  controller.assertJS("subject.activeTab.location == 'about:blank'",
                      controller.tabs);

  // The tabs title should be 'Untitled'
  var title = UtilsAPI.getProperty("chrome://browser/locale/tabbrowser.properties",
                                   "tabs.untitled");
  var tab = tabBrowser.getTab();
  controller.assertJS("subject.label == '" + title + "'", tab.getNode());

  // Close the tab again
  tabBrowser.closeTab({type: "shortcut"});
  controller.waitForEval("subject.length == 1", gTimeout, 100, controller.tabs);
}

/**
 * Map test functions to litmus tests
 */
// testNewTab.meta = {litmusids : [8086]};
