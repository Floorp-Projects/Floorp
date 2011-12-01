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
 * The Original Code is peptest.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Andrew Halberstadt <halbersa@gmail.com>
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


/**
 * This test is designed to test responsiveness while performing actions
 * on various context menus in both content and chrome.
 */

// Import mozmill and initialize a controller
Components.utils.import("resource://mozmill/driver/mozmill.js");
let c = getBrowserController();

// Open mozilla.org and wait for the page to load
c.open("http://mozilla.org");
c.waitForPageLoad();

// Grab reference to element on page (this is the <body> element in this case)
let page = findElement.ID(c.tabs.activeTab, 'header');
// Perform our first action, reload.
// It is very important to only place things that we
// are interested in testing inside of a performAction call
pep.performAction('content_reload', function() {
  page.rightClick();
  page.keypress('r');
});
c.waitForPageLoad();

c.open("http://google.com");
c.waitForPageLoad();

page = findElement.ID(c.tabs.activeTab, 'main');
// Perform our second action, go back
pep.performAction('content_back', function() {
  page.rightClick();
  page.keypress('b');
});
// Bug 699400 - waitForPageLoad times out when pressing back button
c.sleep(100);

page = findElement.ID(c.tabs.activeTab, 'home');
// Perform our third action, scroll through context menu
pep.performAction('content_scroll', function() {
  page.rightClick();
  for (let i = 0; i < 15; ++i) {
    page.keypress('VK_DOWN');
    // Sleep to better emulate a user
    c.sleep(10);
  }
});

// Now test context menus in chrome
let bar = findElement.ID(c.window.document, "appmenu-toolbar-button");
bar.click();
pep.performAction('chrome_menu', function() {
  bar.rightClick();
  bar.keypress('m');
});

pep.performAction('chrome_addon', function() {
  bar.rightClick();
  bar.keypress('a');
});

pep.performAction('chrome_scroll', function() {
  bar.rightClick();
  for (let i = 0; i < 15; ++i) {
    page.keypress('VK_DOWN');
    // Sleep to better emulate a user
    c.sleep(10);
  }
});

