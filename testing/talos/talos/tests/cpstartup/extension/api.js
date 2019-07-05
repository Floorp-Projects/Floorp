/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The purpose of this test it to measure the performance of a
 * content process startup.
 *
 * In practice it measures a bit more than the content process startup. First
 * the parent process starts the clock and requests a new tab with a simple
 * page and then the child stops the clock in the frame script that will be
 * able to process the URL to handle.  So it does measure a few things
 * pre process creation (browser element and tab creation on parent side) but
 * does not measure the part where we actually parse and render the page on
 * the content side, just the overhead of spawning a new content process.
 */

ChromeUtils.defineModuleGetter(
  this,
  "TalosParentProfiler",
  "resource://talos-powers/TalosParentProfiler.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

const PREALLOCATED_PREF = "dom.ipc.processPrelaunch.enabled";
const MESSAGES = ["CPStartup:Go", "Content:BrowserChildReady"];

/* global ExtensionAPI */

this.cpstartup = class extends ExtensionAPI {
  onStartup() {
    for (let msgName of MESSAGES) {
      Services.mm.addMessageListener(msgName, this);
    }

    this.framescriptURL = this.extension.baseURI.resolve("/framescript.js");
    Services.mm.loadFrameScript(this.framescriptURL, true);

    this.originalPreallocatedEnabled = Services.prefs.getBoolPref(
      PREALLOCATED_PREF
    );
    Services.prefs.setBoolPref(PREALLOCATED_PREF, false);

    this.readyCallback = null;
    this.startStamp = null;
    this.tab = null;
  }

  onShutdown() {
    Services.prefs.setBoolPref(
      PREALLOCATED_PREF,
      this.originalPreallocatedEnabled
    );
    Services.mm.removeDelayedFrameScript(this.framescriptURL);

    for (let msgName of MESSAGES) {
      Services.mm.removeMessageListener(msgName, this);
    }
  }

  receiveMessage(msg) {
    let browser = msg.target;
    let gBrowser = browser.ownerGlobal.gBrowser;

    switch (msg.name) {
      case "CPStartup:Go": {
        this.openTab(gBrowser, msg.data.target).then(results =>
          this.reportResults(results)
        );
        break;
      }

      case "Content:BrowserChildReady": {
        // Content has reported that it's ready to process an URL.
        if (!this.readyCallback) {
          throw new Error(
            "Content:BrowserChildReady fired without a readyCallback set"
          );
        }
        let tab = gBrowser.getTabForBrowser(browser);
        if (tab != this.tab) {
          // Let's ignore the message if it's not from the tab we've just opened.
          break;
        }
        // The child stopped the timer when it was ready to process the first URL, it's time to
        // calculate the difference and report it.
        let delta = msg.data.time - this.startStamp;
        this.readyCallback({ tab, delta });
        break;
      }
    }
  }

  async openTab(gBrowser, url) {
    // Start the timer and the profiler right before the tab open on the parent side.
    TalosParentProfiler.resume("tab opening starts");
    this.startStamp = Services.telemetry.msSystemNow();
    this.tab = gBrowser.selectedTab = gBrowser.addTrustedTab(url);

    let { tab, delta } = await this.whenTabReady();
    TalosParentProfiler.pause("tab opening end");
    await this.removeTab(tab);
    return delta;
  }

  whenTabReady() {
    return new Promise(resolve => {
      this.readyCallback = resolve;
    });
  }

  removeTab(tab) {
    return new Promise(resolve => {
      let { messageManager: mm, frameLoader } = tab.linkedBrowser;
      mm.addMessageListener(
        "SessionStore:update",
        function onMessage(msg) {
          if (msg.targetFrameLoader == frameLoader && msg.data.isFinal) {
            mm.removeMessageListener("SessionStore:update", onMessage);
            resolve();
          }
        },
        true
      );

      tab.ownerGlobal.gBrowser.removeTab(tab);
    });
  }

  reportResults(results) {
    Services.mm.broadcastAsyncMessage("CPStartup:FinalResults", results);
  }
};
