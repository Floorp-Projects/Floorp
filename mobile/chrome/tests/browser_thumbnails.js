// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/*
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Wes Johnston <wjohnston@mozilla.com>
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

let testURL_blank = baseURI + "browser_blank_01.html";

const DEFAULT_WIDTH = 800;

function testURL(n) {
  if (n < 0)
    return testURL_blank;

  return baseURI + "browser_viewport.sjs?" + encodeURIComponent(gTestData[n].metadata);
}

function scaleRatio(n) {
  if ("scaleRatio" in gTestData[n])
    return gTestData[n].scaleRatio;
  return 150; // Default value matches our main target hardware (N900, Nexus One, etc.)
}

let currentTab;

let loadURL = function loadURL(aPageURL) {
  BrowserUI.goToURI(aPageURL);
};

let gTestData = [
  { metadata: "", width: DEFAULT_WIDTH, scale: 1 },
  { metadata: "width=device-width, initial-scale=1" },
  { metadata: "width=device-width" },
  { metadata: "width=320, initial-scale=1" },
  { metadata: "initial-scale=1.0, user-scalable=no" },
  { metadata: "width=200,height=500" },
  { metadata: "width=2000, minimum-scale=0.75" },
  { metadata: "width=100, maximum-scale=2.0" },
  { metadata: "width=2000, initial-scale=0.75" },
  { metadata: "width=20000, initial-scale=100" },
  { metadata: "XHTML" },
  /* testing opening and switching between pages without a viewport */
  { metadata: "style=width:400px;margin:0px;" },
  { metadata: "style=width:1000px;margin:0px;" },
  { metadata: "style=width:800px;margin:0px;" },
  { metadata: "style=width:800px;height:300px;margin:0px;" },
  { metadata: "style=width:800px;height:1000px;margin:0px;" },
];



//------------------------------------------------------------------------------
// Entry point (must be named "test")
let gCurrentTest = -1;
let secondPass = false;
let oldRendererFactory;
let gDevice = null;
function test() {
  oldRendererFactory = rendererFactory;
  rendererFactory = newRendererFactory;

  // if we are on desktop, we can run tests in both portrait and landscape modes
  // on devices we should just use the current orientation
  let sysInfo = Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2);
  gDevice = sysInfo.get("device");
  if (gDevice != null)
    secondPass = true;

  // This test is async
  waitForExplicitFinish();

  currentTab = Browser.addTab("about:blank", true);
  ok(currentTab, "Tab Opened");

  startTest(gCurrentTest);
}

function startTest(n) {
  let url = testURL(gCurrentTest);
  info("Testing: " + url)
  loadURL(url);
}

function newRendererFactory(aBrowser, aCanvas) {
  let wrapper = {};
  let ctx = aCanvas.MozGetIPCContext("2d");
  let draw = function(browser, aLeft, aTop, aWidth, aHeight, aColor, aFlags) {
    is(aLeft,   0,"Thumbnail drawn at x=0");
    is(aTop,    0,"Thumbnail drawn at y=0");
    ok(aWidth <= browser.contentDocumentWidth,  "Thumbnail ok width: " + aWidth  + " <= " + browser.contentDocumentWidth);
    ok(aHeight <= browser.contentDocumentHeight,"Thumbnail ok height:" + aHeight + " <= " + browser.contentDocumentHeight);
    finishTest();
  };
  wrapper.checkBrowser = function(browser) {
    return browser.contentWindow;
  };
  wrapper.drawContent = function(callback) {
    callback(ctx, draw);
  };

  return wrapper;
};

function finishTest() {
  gCurrentTest++;
  if (gCurrentTest < gTestData.length) {
    startTest(gCurrentTest);
  } else if (secondPass) {

    if (gDevice == null)
      window.top.resizeTo(480,800);

    Browser.closeTab(currentTab);
    rendererFactory = oldRendererFactory;
    finish();
  } else {
    secondPass = true;
    gCurrentTest = -1;
    window.top.resizeTo(800,480);
    startTest(gCurrentTest);
  }
}
