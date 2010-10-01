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
 *   Aaron Train <atrain@mozilla.com>
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

const TIMEOUT = 5000;

const LOCAL_TEST_FOLDER = collector.addHttpResource('../test-files/');
const LOCAL_TEST_PAGES = [
  {url: LOCAL_TEST_FOLDER + 'layout/mozilla.html', id: 'community'},
  {url: LOCAL_TEST_FOLDER + 'layout/mozilla_mission.html', id: 'mission_statement'},
  {url: LOCAL_TEST_FOLDER + 'layout/mozilla_grants.html', id: 'accessibility'} 
];

var setupModule = function() {
  controller = mozmill.getBrowserController();
}

/**
 * Test the back and forward buttons
 */
var testBackAndForward = function() {
  // Open up the list of local pages statically assigned in the array
  for each(var localPage in LOCAL_TEST_PAGES) {
    controller.open(localPage.url);
    controller.waitForPageLoad();
   
    var element = new elementslib.ID(controller.tabs.activeTab, localPage.id);
    controller.waitForElement(element, TIMEOUT);
  }

  // Click on the Back button for the number of local pages visited
  for (var i = LOCAL_TEST_PAGES.length - 2; i >= 0; i--) {
    controller.goBack();
    controller.waitForPageLoad();

    var element = new elementslib.ID(controller.tabs.activeTab, LOCAL_TEST_PAGES[i].id);
    controller.waitForElement(element, TIMEOUT);
  }

  // Click on the Forward button for the number of websites visited
  for (var j = 1; j < LOCAL_TEST_PAGES.length; j++) {
    controller.goForward();
    controller.waitForPageLoad();

    var element = new elementslib.ID(controller.tabs.activeTab, LOCAL_TEST_PAGES[j].id);
    controller.waitForElement(element, TIMEOUT);
  }
}

/**
 * Map test functions to litmus tests
 */
// testBackAndForward.meta = {litmusids : [8032]};
