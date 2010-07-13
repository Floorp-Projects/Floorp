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

let working_tab;
function pageLoaded(url) {
  return function() {
    return !working_tab.isLoading() && working_tab.browser.currentURI.spec == url;
  }
}

let numberTests = 10;

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
  waitFor(verifyBlank(n), pageLoaded(testURL_blank));
}

function verifyBlank(n) {
  return function() {
    // Do sanity tests
    var uri = working_tab.browser.currentURI.spec;
    is(uri, testURL_blank, "URL Matches blank page "+n);

    // Check viewport settings
    let style = window.getComputedStyle(working_tab.browser, null);
    is(style.width, "800px", "Normal 'browser' width is 800 pixels");

    loadTest(n);
  }
}

function loadTest(n) {
  let url = testURL(n);
  BrowserUI.goToURI(url);
  waitFor(function() {
    // 1) endLoading is called
    // 2) updateDefaultZoom sees meta tag for the first time
    // 3) endLoading updates the browser style width or height
    // 4) browser flags as stopped loading
    //
    // At this point tests would be run without the setTimeout.  Then:
    // 6) screen size change event is received
    // 7) updateDefaultZoomLevel is called and scale changes
    //
    // setTimeout ensures that screen size event happens first
    setTimeout(verifyTest(n), 0);
  }, pageLoaded(url));
}

function is_approx(actual, expected, fuzz, description) {
  is(Math.abs(actual - expected) <= fuzz,
     true,
     description + " [got " + actual + ", expected " + expected + "]");
}

function getMetaTag(name) {
  let doc = working_tab.browser.contentDocument;
  let elements = doc.querySelectorAll("meta[name=\"" + name + "\"]");
  return elements[0] ? elements[0].getAttribute("content") : undefined;
}

function verifyTest(n) {
  return function() {
    is(window.innerWidth, 800, "Test assumes window width is 800px");
    is(Services.prefs.getIntPref("zoom.dpiScale") / 100, 1.5, "Test assumes zoom.dpiScale is 1.5");

    // Do sanity tests
    var uri = working_tab.browser.currentURI.spec;
    is(uri, testURL(n), "URL is "+testURL(n));

    // Expected results are grabbed from the test HTML file
    let data = (function() {
      let result = {};
      for (let i = 0; i < arguments.length; i++)
        result[arguments[i]] = getMetaTag("expected-" + arguments[i]);

      return result;
    })("width", "scale", "minWidth", "maxWidth", "disableZoom");

    let style = window.getComputedStyle(working_tab.browser, null);
    let actualWidth = parseFloat(style.width.replace(/[^\d\.]+/, ""));
    is_approx(actualWidth, parseFloat(data.width), .01, "Viewport width="+data.width);

    let bv = Browser._browserView;
    let zoomLevel = bv.getZoomLevel();
    is_approx(bv.getZoomLevel(), parseFloat(data.scale), .01, "Viewport scale="+data.scale);

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
  if (n+1 < numberTests) {
    startTest(n+1);
  } else {
    Browser.closeTab(working_tab);
    finish();
  }
}
