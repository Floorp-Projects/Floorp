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
 *   Mark Finkle <mfinkle@mozilla.com>
 *   Matt Brubeck <mbrubeck@mozilla.com>
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


let testURL_blank = "chrome://mochikit/content/browser/mobile/chrome/browser_blank_01.html";
let testURL = function testURL(n) {
  return "chrome://mochikit/content/browser/mobile/chrome/browser_viewport_" +
         (n<10 ? "0" : "") + n + ".html";
}
let deviceWidth = {};

let working_tab;
let isLoading = function() { return !working_tab.isLoading(); };

let testData = [
  { width: undefined,   scale: undefined },
  { width: deviceWidth, scale: 1 },
  { width: 320,         scale: 1 },
  { width: undefined,   scale: 1,        disableZoom: true },
  { width: 200,         scale: undefined },
  { width: 2000,        minScale: 0.75 },
  { width: 100,         maxScale: 2 },
  { width: 2000,        scale: 0.75 }
];

//------------------------------------------------------------------------------
// Entry point (must be named "test")
function test() {
  // This test is async
  waitForExplicitFinish();

  working_tab = Browser.addTab("", true);
  ok(working_tab, "Tab Opened");
  startTest(0);
}

function startTest(n) {
  BrowserUI.goToURI(testURL_blank);
  waitFor(verifyBlank(n), isLoading);
}

function verifyBlank(n) {
  return function() {
    // Do sanity tests
    var uri = working_tab.browser.currentURI.spec;
    is(uri, testURL_blank, "URL Matches blank page "+n);

    // Check viewport settings
    ok(working_tab.browser.classList.contains("browser"), "Normal 'browser' class");
    let style = window.getComputedStyle(working_tab.browser, null);
    is(style.width, "800px", "Normal 'browser' width is 800 pixels");

    loadTest(n);
  }
}

function loadTest(n) {
  BrowserUI.goToURI(testURL(n));
  waitFor(verifyTest(n), isLoading);
}

function verifyTest(n) {
  return function() {
    let data = testData[n];

    // Do sanity tests
    var uri = working_tab.browser.currentURI.spec;
    is(uri, testURL(n), "URL Matches newly created Tab "+n);

    // Check viewport settings
    let width = data.width || (data.scale ? window.innerWidth : 800);
    if (width == deviceWidth) {
      width = window.innerWidth;
      ok(working_tab.browser.classList.contains("browser-viewport"), "Viewport 'browser-viewport' class");
    }

    let scale = data.scale || window.innerWidth / width;
    let minScale = data.minScale || 0.2;
    let maxScale = data.maxScale || 4;

    scale = Math.min(scale, maxScale);
    scale = Math.max(scale, minScale);

    // If pageZoom is "almost" 100%, zoom in to exactly 100% (bug 454456).
    if (0.9 < scale && scale < 1)
      scale = 1;

    // Width at initial scale can't be less than screen width (Bug 561413).
    if (width * scale < window.innerWidth)
      width = window.innerWidth / scale;

    let style = window.getComputedStyle(working_tab.browser, null);
    is(style.width, width + "px", "Viewport width="+width);

    let bv = Browser._browserView;
    let zoomLevel = bv.getZoomLevel();
    is(bv.getZoomLevel(), scale, "Viewport scale="+scale);

    // Test zooming
    if (data.disableZoom) {
      ok(!bv.allowZoom, "Zoom disabled");

      Browser.zoom(-1);
      is(bv.getZoomLevel(), zoomLevel, "Zoom in does nothing");

      Browser.zoom(1);
      is(bv.getZoomLevel(), zoomLevel, "Zoom out does nothing");
    }
    else {
      ok(bv.allowZoom, "Zoom enabled");
    }


    if (data.minScale) {
      do { // Zoom out until we can't go any farther.
        zoomLevel = bv.getZoomLevel();
        Browser.zoom(1);
      } while (bv.getZoomLevel() != zoomLevel);
      ok(bv.getZoomLevel() >= data.minScale, "Zoom out limited");
    }

    if (data.maxScale) {
      do { // Zoom in until we can't go any farther.
        zoomLevel = bv.getZoomLevel();
        Browser.zoom(-1);
      } while (bv.getZoomLevel() != zoomLevel);
      ok(bv.getZoomLevel() <= data.maxScale, "Zoom in limited");
    }

    finishTest(n);
  }
}

function finishTest(n) {
  if (n+1 < testData.length) {
    startTest(n+1);
  } else {
    Browser.closeTab(working_tab);
    finish();
  }
}
