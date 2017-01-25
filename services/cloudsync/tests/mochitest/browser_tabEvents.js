/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {

  let local = {};

  Components.utils.import("resource://gre/modules/CloudSync.jsm", local);
  Components.utils.import("resource:///modules/sessionstore/TabStateFlusher.jsm", local);

  let cloudSync = local.CloudSync();
  let opentabs = [];

  waitForExplicitFinish();

  let testURL = "chrome://mochitests/content/browser/services/cloudsync/tests/mochitest/other_window.html";
  let expected = [
    testURL,
    testURL + "?x=1",
    testURL + "?x=%20a",
    // testURL+"?x=å",
  ];

  let nevents = 0;
  let nflushed = 0;
  function handleTabChangeEvent() {
    cloudSync.tabs.removeEventListener("change", handleTabChangeEvent);
    ++nevents;
    info("tab change event " + nevents);
    next();
  }

  function getLocalTabs() {
    cloudSync.tabs.getLocalTabs().then(
      function(tabs) {
        for (let tab of tabs) {
          ok(expected.indexOf(tab.url) >= 0, "found an expected tab");
        }

        is(tabs.length, expected.length, "found the right number of tabs");

        opentabs.forEach(function(tab) {
          gBrowser.removeTab(tab);
        });

        is(nevents, 1, "expected number of change events");

        finish();
      }
    )
  }

  cloudSync.tabs.addEventListener("change", handleTabChangeEvent);

  expected.forEach(function(url) {
    let tab = gBrowser.addTab(url);

    function flush() {
      tab.linkedBrowser.removeEventListener("load", flush, true);
      local.TabStateFlusher.flush(tab.linkedBrowser).then(() => {
        ++nflushed;
        info("flushed " + nflushed);
        next();
      });
    }

    tab.linkedBrowser.addEventListener("load", flush, true);

    opentabs.push(tab);
  });

  function next() {
    if (nevents == 1 && nflushed == expected.length) {
      getLocalTabs();
    }
  }

}
