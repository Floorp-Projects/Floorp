/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-console.html";

let tab1, tab2, win1, win2;
let noErrors = true;

function tab1Loaded(aEvent) {
  browser.removeEventListener(aEvent.type, arguments.callee, true);

  win2 = OpenBrowserWindow();
  win2.addEventListener("load", win2Loaded, true);
}

function win2Loaded(aEvent) {
  win2.removeEventListener(aEvent.type, arguments.callee, true);

  tab2 = win2.gBrowser.addTab();
  win2.gBrowser.selectedTab = tab2;
  tab2.linkedBrowser.addEventListener("load", tab2Loaded, true);
  tab2.linkedBrowser.contentWindow.location = TEST_URI;
}

function tab2Loaded(aEvent) {
  tab2.linkedBrowser.removeEventListener(aEvent.type, arguments.callee, true);

  waitForFocus(function() {
    try {
      HUDService.activateHUDForContext(tab1);
    }
    catch (ex) {
      ok(false, "HUDService.activateHUDForContext(tab1) exception: " + ex);
      noErrors = false;
    }

    try {
      HUDService.activateHUDForContext(tab2);
    }
    catch (ex) {
      ok(false, "HUDService.activateHUDForContext(tab2) exception: " + ex);
      noErrors = false;
    }

    try {
      HUDService.deactivateHUDForContext(tab1);
    }
    catch (ex) {
      ok(false, "HUDService.deactivateHUDForContext(tab1) exception: " + ex);
      noErrors = false;
    }

    try {
      HUDService.deactivateHUDForContext(tab2);
    }
    catch (ex) {
      ok(false, "HUDService.deactivateHUDForContext(tab2) exception: " + ex);
      noErrors = false;
    }

    if (noErrors) {
      ok(true, "there were no errors");
    }

    win2.gBrowser.removeTab(tab2);

    executeSoon(function() {
      win2.close();
      tab1 = tab2 = win1 = win2 = null;
      finishTest();
    });

  }, tab2.linkedBrowser.contentWindow);
}

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", tab1Loaded, true);
  tab1 = gBrowser.selectedTab;
  win1 = window;
}

