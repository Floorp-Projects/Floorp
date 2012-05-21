// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let testURL_blank = baseURI + "browser_blank_01.html";

const DEFAULT_WIDTH = 800;

function testURL(n) {
  return baseURI + "browser_viewport.sjs" +
    "?metadata=" + encodeURIComponent(gTestData[n].metadata || "") +
    "&style=" + encodeURIComponent(gTestData[n].style || "") +
    "&xhtml=" + encodeURIComponent(!!gTestData[n].xhtml);
}

function scaleRatio(n) {
  if ("scaleRatio" in gTestData[n])
    return gTestData[n].scaleRatio;
  return 150; // Default value matches our main target hardware (N900, Nexus One, etc.)
}

let currentTab;

let loadURL = function loadURL(aPageURL, aCallback, aScale) {
  messageManager.addMessageListener("pageshow", function(aMessage) {
    if (aMessage.target.currentURI.spec == aPageURL) {
      messageManager.removeMessageListener(aMessage.name, arguments.callee);

      waitFor(aCallback, function() {
        return !aScale || aScale == aMessage.target.scale;
      });
    }
  });

  BrowserUI.goToURI(aPageURL);
};

// XXX Tests do not yet run correctly in portrait.
window.resizeTo(800, 480);

let gTestData = [
  { metadata: "", width: DEFAULT_WIDTH, scale: 1 },
  { metadata: "width=device-width, initial-scale=1", width: 533.33, scale: 1.5 },
  { metadata: "width=device-width", width: 533.33, scale: 1.5 },
  { metadata: "width=device-width, initial-scale=1", scaleRatio: 100, width: 800, scale: 1 },
  { metadata: "width=320, initial-scale=1", width: 533.33, scale: 1.5 },
  { metadata: "initial-scale=1.0, user-scalable=no", width: 533.33, scale: 1.5, disableZoom: true },
  { metadata: "initial-scale=1.0, user-scalable=0", width: 533.33, scale: 1.5, disableZoom: true },
  { metadata: "initial-scale=1.0, user-scalable=false", width: 533.33, scale: 1.5, disableZoom: true },
  { metadata: "initial-scale=1.0, user-scalable=NO", width: 533.33, scale: 1.5, disableZoom: false }, // values are case-sensitive
  { metadata: "width=200,height=500", width: 200, scale: 4 },
  { metadata: "width=2000, minimum-scale=0.75", width: 2000, scale: 1.125, minScale: 1.125 },
  { metadata: "width=100, maximum-scale=2.0", width: 266.67, scale: 3, maxScale: 3 },
  { metadata: "width=2000, initial-scale=0.75", width: 2000, scale: 1.125 },
  { metadata: "width=20000, initial-scale=100", width: 10000, scale: 4 },
  { xhtml: true, width: 533.33, scale: 1.5, disableZoom: false },
  /* testing spaces between arguments (bug 572696) */
  { metadata: "width= 2000, minimum-scale=0.75", width: 2000, scale: 1.125 },
  { metadata: "width = 2000, minimum-scale=0.75", width: 2000, scale: 1.125 },
  { metadata: "width = 2000 , minimum-scale=0.75", width: 2000, scale: 1.125 },
  { metadata: "width = 2000 , minimum-scale =0.75", width: 2000, scale: 1.125 },
  { metadata: "width = 2000 , minimum-scale = 0.75", width: 2000, scale: 1.125 },
  { metadata: "width =  2000   ,    minimum-scale      =       0.75", width: 2000, scale: 1.125 },
  /* testing opening and switching between pages without a viewport */
  { style: "width:400px;margin:0px;", width: DEFAULT_WIDTH, scale: 1 },
  { style: "width:2000px;margin:0px;", width: 980, scale: window.innerWidth/2000 },
  { style: "width:800px;margin:0px;", width: DEFAULT_WIDTH, scale: 1 },
];


//------------------------------------------------------------------------------
// Entry point (must be named "test")
function test() {
  // This test is async
  waitForExplicitFinish();
  requestLongerTimeout(2);

  currentTab = Browser.addTab("about:blank", true);
  ok(currentTab, "Tab Opened");

  startTest(0);
}

function startTest(n) {
  info(JSON.stringify(gTestData[n]));
  BrowserUI.goToURI(testURL_blank);
  loadURL(testURL_blank, verifyBlank(n));
  Services.prefs.setIntPref("browser.viewport.scaleRatio", scaleRatio(n));
}

function verifyBlank(n) {
  return function() {
    // Do sanity tests
    let uri = currentTab.browser.currentURI.spec;
    is(uri, testURL_blank, "URL Matches blank page " + n);

    waitFor(function() {
      loadURL(testURL(n), verifyTest(n), gTestData[n].scale);
    }, function() {
      return currentTab.browser.contentWindowWidth == DEFAULT_WIDTH;
    });
  }
}

function is_approx(actual, expected, fuzz, description) {
  ok(Math.abs(actual - expected) <= fuzz,
     description + " [got " + actual + ", expected " + expected + "]");
}

function verifyTest(n) {
  let assumedWidth = 480;
  if (!Util.isPortrait())
    assumedWidth = 800;

  return function() {
    is(window.innerWidth, assumedWidth, "Test assumes window width is " + assumedWidth + "px");

    // Do sanity tests
    let uri = currentTab.browser.currentURI.spec;
    is(uri, testURL(n), "URL is " + testURL(n));

    let data = gTestData[n];
    let actualWidth = currentTab.browser.contentWindowWidth;
    is_approx(actualWidth, parseFloat(data.width), .01, "Viewport width=" + data.width);

    let zoomLevel = getBrowser().scale;
    is_approx(zoomLevel, parseFloat(data.scale), .01, "Viewport scale=" + data.scale);

    // Test zooming
    if (data.disableZoom) {
      ok(!currentTab.allowZoom, "Zoom disabled");

      Browser.zoom(-1);
      is(getBrowser().scale, zoomLevel, "Zoom in does nothing");

      Browser.zoom(1);
      is(getBrowser().scale, zoomLevel, "Zoom out does nothing");
    }
    else {
      ok(Browser.selectedTab.allowZoom, "Zoom enabled");
    }


    if (data.minScale) {
      do { // Zoom out until we can't go any farther.
        zoomLevel = getBrowser().scale;
        Browser.zoom(1);
      } while (getBrowser().scale != zoomLevel);
      ok(getBrowser().scale >= data.minScale, "Zoom out limited");
    }

    if (data.maxScale) {
      do { // Zoom in until we can't go any farther.
        zoomLevel = getBrowser().scale;
        Browser.zoom(-1);
      } while (getBrowser().scale != zoomLevel);
      ok(getBrowser().scale <= data.maxScale, "Zoom in limited");
    }

    finishTest(n);
  }
}

function finishTest(n) {
  Services.prefs.clearUserPref("browser.viewport.scaleRatio");
  if (n + 1 < gTestData.length) {
    startTest(n + 1);
  } else {
    window.resizeTo(480, 800);
    Browser.closeTab(currentTab);
    finish();
  }
}
