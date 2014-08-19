// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/SimpleServiceDiscovery.jsm");

function ok(passed, text) {
  do_report_result(passed, text, Components.stack.caller, false);
}

// The chrome window
let chromeWin;

// Track the <browser> where the tests are happening
let browser;

function middle(element) {
  let rect = element.getBoundingClientRect();
  let x = (rect.right - rect.left) / 2 + rect.left;
  let y = (rect.bottom - rect.top) / 2 + rect.top;
  return [x, y];
}

// We must register a target and make a "mock" service for the target
var testTarget = {
  target: "test:service",
  factory: function(service) { /* dummy */  },
  types: ["video/mp4", "video/webm"],
  extensions: ["mp4", "webm"]
};

add_test(function setup_browser() {
  chromeWin = Services.wm.getMostRecentWindow("navigator:browser");
  let BrowserApp = chromeWin.BrowserApp;

  do_register_cleanup(function cleanup() {
    BrowserApp.closeTab(BrowserApp.getTabForBrowser(browser));
    SimpleServiceDiscovery.unregisterTarget(testTarget);
  });

  // We need to register a target or processService will ignore us
  SimpleServiceDiscovery.registerTarget(testTarget);

  // Create a pretend service
  let service = {
    location: "http://mochi.test:8888/tests/robocop/simpleservice.xml",
    target: "test:service"
  };

  do_print("Force a detailed ping from a pretend service");

  // Poke the service directly to get the discovery to happen
  SimpleServiceDiscovery._processService(service);

  // Load our test web page with <video> elements
  let url = "http://mochi.test:8888/tests/robocop/video_discovery.html";
  browser = BrowserApp.addTab(url, { selected: true, parentId: BrowserApp.selectedTab.id }).browser;
  browser.addEventListener("load", function startTests(event) {
    browser.removeEventListener("load", startTests, true);
    Services.tm.mainThread.dispatch(run_next_test, Ci.nsIThread.DISPATCH_NORMAL);
  }, true);
});

let videoDiscoveryTests = [
  { id: "simple-mp4", source: "http://mochi.test:8888/simple.mp4", poster: "http://mochi.test:8888/simple.png", text: "simple video with mp4 src" },
  { id: "simple-fail", pass: false, text: "simple video with no mp4 src" },
  { id: "with-sources-mp4", source: "http://mochi.test:8888/simple.mp4", text: "video with mp4 extension source child" },
  { id: "with-sources-webm", source: "http://mochi.test:8888/simple.webm", text: "video with webm extension source child" },
  { id: "with-sources-fail", pass: false, text: "video with no mp4 extension source child" },
  { id: "with-sources-mimetype-mp4", source: "http://mochi.test:8888/simple-video-mp4", text: "video with mp4 mimetype source child" },
  { id: "with-sources-mimetype-webm", source: "http://mochi.test:8888/simple-video-webm", text: "video with webm mimetype source child" },
  { id: "video-overlay", source: "http://mochi.test:8888/simple.mp4", text: "div overlay covering a simple video with mp4 src" }
];

function execute_video_test(test) {
  let element = browser.contentDocument.getElementById(test.id);
  if (element) {
    let [x, y] = middle(element);
    let video = chromeWin.CastingApps.getVideo(element, x, y);
    if (video) {
      let matchPoster = (test.poster == video.poster);
      let matchSource = (test.source == video.source);
      ok(matchPoster && matchSource && test.pass, test.text);
    } else {
      ok(!test.pass, test.text);
    }
  } else {
    ok(false, "test element not found: [" + test.id + "]");
  }
  run_next_test();
}

let videoTest;
while ((videoTest = videoDiscoveryTests.shift())) {
  if (!("poster" in videoTest)) {
    videoTest.poster = "";
  }
  if (!("pass" in videoTest)) {
    videoTest.pass = true;
  }

  add_test(execute_video_test.bind(this, videoTest));
}

run_next_test();
