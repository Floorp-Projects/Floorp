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
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aakash Desai <adesai@mozilla.com>
 *   Henrik Skupin <hskupin@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Vertributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Initial Developer of the Original Code is Mozihers to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Include necessary modules
var RELATIVE_ROOT = '../../shared-modules';
var MODULE_REQUIRES = ['PrefsAPI', 'TabbedBrowsingAPI', 'UtilsAPI'];

const gDelay = 0;
const gTimeout = 5000;

var setupModule = function(module) {
  module.controller = mozmill.getBrowserController();

  TabbedBrowsingAPI.closeAllTabs(controller);
}

var testWarningPages = function() {
  var urls = ['http://www.mozilla.com/firefox/its-a-trap.html',
              'http://www.mozilla.com/firefox/its-an-attack.html'];

  for (var i = 0; i < urls.length; i++ ) {
    // Open one of the mozilla phishing protection test pages
    controller.open(urls[i]);
    controller.waitForPageLoad(1000);

    // Test the getMeOutButton
    checkGetMeOutOfHereButton();

    // Go back to the warning page
    controller.open(urls[i]);
    controller.waitForPageLoad(1000);

    // Test the reportButton
    checkReportButton(i, urls[i]);

    // Go back to the warning page
    controller.open(urls[i]);
    controller.waitForPageLoad(1000);

    // Test the ignoreWarning button
    checkIgnoreWarningButton(urls[i]);
  }
}

/**
 * Check that the getMeOutButton sends the user to the firefox's default home page
 */
var checkGetMeOutOfHereButton = function()
{
  var getMeOutOfHereButton = new elementslib.ID(controller.tabs.activeTab, "getMeOutButton");

  // Wait for the getMeOutOfHereButton to be safely loaded on the warning page and click it
  controller.waitThenClick(getMeOutOfHereButton, gTimeout);
  controller.waitForPageLoad();

  // Check that the default home page has been opened
  var defaultHomePage = UtilsAPI.getProperty("resource:/browserconfig.properties", "browser.startup.homepage");
  UtilsAPI.assertLoadedUrlEqual(controller, defaultHomePage);
}

/*
 * Check that the reportButton sends the user to the forgery or attack site reporting page
 *
 * @param {number} type
 *        Type of malware site to check
 * @param {string} badUrl
 *        URL of malware site to check
 */
var checkReportButton = function(type, badUrl) {
  var reportButton = new elementslib.ID(controller.tabs.activeTab, "reportButton");

  // Wait for the reportButton to be safely loaded onto the warning page
  controller.waitThenClick(reportButton, gTimeout);
  controller.waitForPageLoad();

  var locale = PrefsAPI.preferences.getPref("general.useragent.locale", "");
  var url = "";

  if (type == 0) {
    // Build phishing URL be replacing identifiers with actual locale of browser
    url = UtilsAPI.formatUrlPref("browser.safebrowsing.warning.infoURL");

    var phishingElement = new elementslib.XPath(controller.tabs.activeTab, "/html/body[@id='phishing-protection']")
    controller.assertNode(phishingElement);

  } else if (type == 1) {
    // Build malware URL be replacing identifiers with actual locale of browser and Firefox being used
    url = UtilsAPI.formatUrlPref("browser.safebrowsing.malware.reportURL") + badUrl;

    var malwareElement = new elementslib.ID(controller.tabs.activeTab, "date");
    controller.assertNode(malwareElement);
  }

  UtilsAPI.assertLoadedUrlEqual(controller, url);
}

/*
 * Check that the ignoreWarningButton goes to proper page associated to the url provided
 *
 * @param {string} url
 *        URL of the target website which should be opened
 */
var checkIgnoreWarningButton = function(url) {
  var ignoreWarningButton = new elementslib.ID(controller.tabs.activeTab, "ignoreWarningButton");

  // Wait for the ignoreButton to be safely loaded on the warning page
  controller.waitThenClick(ignoreWarningButton, gTimeout);
  controller.waitForPageLoad();

  // Verify the warning button is not visible and the location bar displays the correct url
  var locationBar = new elementslib.ID(controller.window.document, "urlbar");

  controller.assertValue(locationBar, url);
  controller.assertNodeNotExist(ignoreWarningButton);
  controller.assertNode(new elementslib.ID(controller.tabs.activeTab, "main-feature"));
}

/**
 * Map test functions to litmus tests
 */
// testWarningPages.meta = {litmusids : [9014]};
