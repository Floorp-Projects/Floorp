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
 *   Henrik Skupin <hskupin@mozilla.com>
 *   Aaron Train <atrain@mozilla.com>
 *   Anthony Hughes <ahughes@mozilla.com>
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
const RELATIVE_ROOT = '../../shared-modules';
const MODULE_REQUIRES = ['PrivateBrowsingAPI', 'TabbedBrowsingAPI', 'UtilsAPI'];

const TIMEOUT = 5000;

const LOCAL_TEST_FOLDER = collector.addHttpResource('../test-files/');
const LOCAL_TEST_PAGES = [
  {url: LOCAL_TEST_FOLDER + 'layout/mozilla.html', id: 'community'},
  {url: 'about:', id: 'aboutPageList'}
];

var setupModule = function() {
  controller = mozmill.getBrowserController();
  modifier = controller.window.document.documentElement.
             getAttribute("titlemodifier_privatebrowsing");

  // Create Private Browsing instance and set handler
  pb = new PrivateBrowsingAPI.privateBrowsing(controller);
  pb.handler = pbStartHandler;

  TabbedBrowsingAPI.closeAllTabs(controller);
}

var teardownModule = function() {
  pb.reset();
}

/**
 * Enable Private Browsing Mode
 */
var testEnablePrivateBrowsingMode = function() {
  // Make sure we are not in PB mode and show a prompt
  pb.enabled = false;
  pb.showPrompt = true;

  // Open websites in separate tabs
  var newTab = new elementslib.Elem(controller.menus['file-menu'].menu_newNavigatorTab);
  
  for each (var page in LOCAL_TEST_PAGES) {
   controller.open(page.url);
   controller.click(newTab);
  }

  // Wait until all tabs have been finished loading
  for (var i = 0; i < LOCAL_TEST_PAGES.length; i++) {
   var elem = new elementslib.ID(controller.tabs.getTab(i), LOCAL_TEST_PAGES[i].id);
   controller.waitForElement(elem, TIMEOUT);
  }

  // Start the Private Browsing mode
  pb.start();

  // Check that only one tab is open
  controller.assertJS("subject.isOnlyOneTab == true", 
                      {isOnlyOneTab: controller.tabs.length == 1});

  // Title modifier should have been set
  controller.assertJS("subject.hasTitleModifier == true",
                      {hasTitleModifier: controller.window.document.
                                         title.indexOf(modifier) != -1});

  // Check descriptions on the about:privatebrowsing page
  var description = UtilsAPI.getEntity(pb.getDtds(), "privatebrowsingpage.description");
  var learnMore = UtilsAPI.getEntity(pb.getDtds(), "privatebrowsingpage.learnMore");
  var longDescElem = new elementslib.ID(controller.tabs.activeTab, "errorLongDescText");
  var moreInfoElem = new elementslib.ID(controller.tabs.activeTab, "moreInfoLink");
  controller.waitForElement(longDescElem, TIMEOUT);  
  controller.assertText(longDescElem, description);
  controller.assertText(moreInfoElem, learnMore);
}

/**
 * Stop the Private Browsing mode
 */
var testStopPrivateBrowsingMode = function() {
  // Force enable Private Browsing mode
  pb.enabled = true;

  // Stop Private Browsing mode
  pb.stop();

  // All tabs should be restored
  controller.assertJS("subject.allTabsRestored == true",
                      {allTabsRestored: controller.tabs.length == LOCAL_TEST_PAGES.length + 1});

  for (var i = 0; i < LOCAL_TEST_PAGES.length; i++) {
    var elem = new elementslib.ID(controller.tabs.getTab(i), LOCAL_TEST_PAGES[i].id);
    controller.waitForElement(elem, TIMEOUT);
  }

  // No title modifier should have been set
  controller.assertJS("subject.noTitleModifier == true",
                      {noTitleModifier: controller.window.document.
                                        title.indexOf(modifier) == -1});
}

/**
 * Verify Ctrl/Cmd+Shift+P keyboard shortcut for Private Browsing mode
 */
var testKeyboardShortcut = function() {
  // Make sure we are not in PB mode and show a prompt
  pb.enabled = false;
  pb.showPrompt = true;

  // Start the Private Browsing mode via the keyboard shortcut
  pb.start(true);

  // Stop the Private Browsing mode via the keyboard shortcut
  pb.stop(true);
}

/**
 * Handle the modal dialog to enter the Private Browsing mode
 *
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 */
var pbStartHandler = function(controller) {
  // Check to not ask anymore for entering Private Browsing mode
  var checkbox = new elementslib.ID(controller.window.document, 'checkbox');
  controller.waitThenClick(checkbox, TIMEOUT);

  var okButton = new elementslib.Lookup(controller.window.document, 
                                        '/id("commonDialog")' +
                                        '/anon({"anonid":"buttons"})' +
                                        '/{"dlgtype":"accept"}');
  controller.click(okButton);
}

/**
 * Map test functions to litmus tests
 */
// testEnablePrivateBrowsingMode.meta = {litmusids : [9154]};
// testStopPrivateBrowsingMode.meta = {litmusids : [9155]};
// testKeyboardShortcut.meta = {litmusids : [9186]};
