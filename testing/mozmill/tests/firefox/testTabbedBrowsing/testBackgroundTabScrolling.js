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
var MODULE_REQUIRES = ['PrefsAPI', 'TabbedBrowsingAPI'];

const localTestFolder = collector.addHttpResource('./files');

const gDelay = 0;
const gTimeout = 5000;

var setupModule = function(module)
{
  controller = mozmill.getBrowserController();

  tabBrowser = new TabbedBrowsingAPI.tabBrowser(controller);
  tabBrowser.closeAllTabs();

  container = tabBrowser.getElement({type: "tabs_container"});
  animateBox = tabBrowser.getElement({type: "tabs_animateBox"});
  allTabsButton = tabBrowser.getElement({type: "tabs_allTabsButton"});
  allTabsPopup = tabBrowser.getElement({type: "tabs_allTabsPopup"});
}

var teardownModule = function()
{
  PrefsAPI.preferences.clearUserPref("browser.tabs.loadInBackground");

  // Just in case the popup hasn't been closed yet
  allTabsPopup.getNode().hidePopup();
}

var testScrollBackgroundTabIntoView = function()
{
  // Check that we open new tabs in the background
  PrefsAPI.openPreferencesDialog(prefDialogCallback);

  // Open the testcase
  controller.open(localTestFolder + "/openinnewtab.html");
  controller.waitForPageLoad();

  var link1 = new elementslib.Name(controller.tabs.activeTab, "link_1");
  var link2 = new elementslib.Name(controller.tabs.activeTab, "link_2");

  // Open new background tabs until the scroll arrows appear
  var count = 1;
  do {
    controller.middleClick(link1);

    // Wait until the new tab has been opened
    controller.waitForEval("subject.length == " + (++count), gTimeout, 100, tabBrowser);
  } while ((container.getNode().getAttribute("overflow") != 'true') || count > 50)

  // Scroll arrows will be shown when the overflow attribute has been added
  controller.assertJS("subject.getAttribute('overflow') == 'true'",
                      container.getNode())

  // Open one more tab but with another link for later verification
  controller.middleClick(link2);

  // Check that the List all Tabs button flashes
  controller.waitForEval("subject.window.getComputedStyle(subject.animateBox, null).opacity != 0",
                         gTimeout, 10, {window : controller.window, animateBox: animateBox.getNode()});
  controller.waitForEval("subject.window.getComputedStyle(subject.animateBox, null).opacity == 0",
                         gTimeout, 100, {window : controller.window, animateBox: animateBox.getNode()});

  // Check that the correct link has been loaded in the last tab
  var lastIndex = controller.tabs.length - 1;
  var linkId = new elementslib.ID(controller.tabs.getTab(lastIndex), "id");
  controller.assertText(linkId, "2");

  // and is displayed inside the all tabs popup menu
  controller.click(allTabsButton);
  controller.waitForEval("subject.state == 'open'", gTimeout, 100, allTabsPopup.getNode());

  for (var ii = 0; ii <= lastIndex; ii++) {
    if (ii < lastIndex)
      controller.assertJS("subject.childNodes[" + ii + "].label != '2'",
                          allTabsPopup.getNode());
    else
      controller.assertJS("subject.childNodes[" + ii + "].label == '2'",
                          allTabsPopup.getNode());
  }

  controller.click(allTabsButton);
  controller.waitForEval("subject.state == 'closed'", gTimeout, 100, allTabsPopup.getNode());
}

/**
 * Check that we open tabs in the background
 *
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 */
var prefDialogCallback = function(controller)
{
  var prefDialog = new PrefsAPI.preferencesDialog(controller);
  prefDialog.paneId = 'paneTabs';

  // Ensure that 'Switch to tabs immediately' is unchecked:
  var switchToTabsPref = new elementslib.ID(controller.window.document, "switchToNewTabs");
  controller.waitForElement(switchToTabsPref, gTimeout);
  controller.check(switchToTabsPref, false);

  prefDialog.close(true);
}

/**
 * Map test functions to litmus tests
 */
// testOpenInBackgroundTab.meta = {litmusids : [8259]};
