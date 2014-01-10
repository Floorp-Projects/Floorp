/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let gBugWindow;

function onLoad() {
  gBugWindow.removeEventListener("load", onLoad);
  gBugWindow.addEventListener("unload", onUnload);
  gBugWindow.close();
}

function onUnload() {
  gBugWindow.removeEventListener("unload", onUnload);
  window.focus();
  finish();
}

// This test opens and then closes the certificate manager to test that it
// does not leak. The test harness keeps track of and reports leaks, so
// there are no actual checks here.
function test() {
  waitForExplicitFinish();
  gBugWindow = window.openDialog("chrome://pippki/content/certManager.xul");
  gBugWindow.addEventListener("load", onLoad);
}
