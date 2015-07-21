// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components; /*global Components */

Cu.import("resource://gre/modules/Promise.jsm"); /*global Promise */
Cu.import("resource://gre/modules/Services.jsm"); /*global Services */
Cu.import("resource://gre/modules/XPCOMUtils.jsm"); /*global XPCOMUtils */
XPCOMUtils.defineLazyModuleGetter(this, "WebChannel",
  "resource://gre/modules/WebChannel.jsm"); /*global WebChannel */

const HTTP_PATH = "http://mochi.test:8888";
const HTTP_ENDPOINT = "/tests/robocop/testWebChannel.html";

const gChromeWin = Services.wm.getMostRecentWindow("navigator:browser");
let BrowserApp = gChromeWin.BrowserApp;

// Keep this synced with /browser/base/content/test/general/browser_web_channel.js
// as much as possible.  (We only have this since we can't run browser chrome
// tests on Android.  Yet?)
let gTests = [
  {
    desc: "WebChannel generic message",
    run: function* () {
      return new Promise(function(resolve, reject) {
        let tab;
        let channel = new WebChannel("generic", Services.io.newURI(HTTP_PATH, null, null));
        channel.listen(function (id, message, target) {
          is(id, "generic");
          is(message.something.nested, "hello");
          channel.stopListening();
          BrowserApp.closeTab(tab);
          resolve();
        });

        tab = BrowserApp.addTab(HTTP_PATH + HTTP_ENDPOINT + "?generic");
      });
    }
  },
  {
    desc: "WebChannel two way communication",
    run: function* () {
      return new Promise(function(resolve, reject) {
        let tab;
        let channel = new WebChannel("twoway", Services.io.newURI(HTTP_PATH, null, null));

        channel.listen(function (id, message, sender) {
          is(id, "twoway");
          ok(message.command);

          if (message.command === "one") {
            channel.send({ data: { nested: true } }, sender);
          }

          if (message.command === "two") {
            is(message.detail.data.nested, true);
            channel.stopListening();
            BrowserApp.closeTab(tab);
            resolve();
          }
        });

        tab = BrowserApp.addTab(HTTP_PATH + HTTP_ENDPOINT + "?twoway");
      });
    }
  },
  {
    desc: "WebChannel multichannel",
    run: function* () {
      return new Promise(function(resolve, reject) {
        let tab;
        let channel = new WebChannel("multichannel", Services.io.newURI(HTTP_PATH, null, null));

        channel.listen(function (id, message, sender) {
          is(id, "multichannel");
          BrowserApp.closeTab(tab);
          resolve();
        });

        tab = BrowserApp.addTab(HTTP_PATH + HTTP_ENDPOINT + "?multichannel");
      });
    }
  }
]; // gTests

add_task(function test() {
  for (let test of gTests) {
    do_print("Running: " + test.desc);
    yield test.run();
  }
});

run_next_test();
