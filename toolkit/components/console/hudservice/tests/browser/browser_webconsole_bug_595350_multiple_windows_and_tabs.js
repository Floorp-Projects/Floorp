/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Patrick Walton <pcwalton@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

// Tests that the Web Console doesn't leak when multiple tabs and windows are
// opened and then closed.

const TEST_URI = "http://example.com/";

let mainWindowTabs = [], newWindowTabs = [];
let loadedTabCount = 0;
let newWindow;

function test() {
  window.open(TEST_URI);
  browser.addEventListener("DOMContentLoaded", onWindowLoad, false);
}

function onWindowLoad() {
  browser.removeEventListener("DOMContentLoaded", onWindowLoad, false);
  newWindow = Services.wm.getMostRecentWindow("navigator:browser");
  ok(newWindow, "we have the window");

  addTabs(mainWindowTabs, gBrowser);
  addTabs(newWindowTabs, newWindow.gBrowser);
}

let funcArr = [];

function addTabs(aTabList, aGBrowser) {
  for (let i = 0; i < 3; i++) {
    let tab = aGBrowser.addTab(TEST_URI);
    funcArr.push(function(){onTabLoad(tab, i);});
    tab.linkedBrowser.addEventListener("DOMContentLoaded", funcArr[i], false);
    aTabList.push(tab);
  }
}

function onTabLoad(aTab, idx) {
  aTab.linkedBrowser.removeEventListener("DOMContentLoaded", funcArr[idx] , false);
  loadedTabCount++;
  if (loadedTabCount < 6) {
    return;
  }

  testMultipleWindowsAndTabs();
}

function testMultipleWindowsAndTabs() {
  for (let i = 0; i < 3; i++) {
    HUDService.activateHUDForContext(mainWindowTabs[i]);
    HUDService.activateHUDForContext(newWindowTabs[i]);
  }

  executeSoon(function() {
    newWindow.close();
    for (let i = 0; i < 3; i++) {
      gBrowser.removeTab(mainWindowTabs[i]);
    }

    finishTest();
  });
}

