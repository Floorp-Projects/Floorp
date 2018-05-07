/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/Services.jsm");

const PREALLOCATED_PREF = "dom.ipc.processPrelaunch.enabled";

const TARGET_PATH = "tests/cpstartup/content/target.html";
const WEBSERVER = Services.prefs.getCharPref("addon.test.cpstartup.webserver");
const TARGET_URI = `${WEBSERVER}/${TARGET_PATH}`;

/**
 * The purpose of this test it to measure the performance of a content process startup.
 *
 * In practice it measures a bit more than the content process startup. First the parent
 * process starts the clock and requests a new tab with a simple page and then the child
 * stops the clock in the frame script that will be able to process the URL to handle.
 * So it does measure a few things pre process creation (browser element and tab creation
 * on parent side) but does not measure the part where we actually parse and render the
 * page on the content side, just the overhead of spawning a new content process.
 */
var CPStartup = {
  MESSAGES: [
    "CPStartup:Go",
    "Content:BrowserChildReady",
  ],

  readyCallback: null,

  startStamp: null,

  tab: null,
  /**
   * Shortcut to getting at the TalosParentProfiler.
   */
  get Profiler() {
    delete this.Profiler;
    let context = {};
    Services.scriptloader.loadSubScript("chrome://talos-powers-content/content/TalosParentProfiler.js", context);
    return this.Profiler = context.TalosParentProfiler;
  },

  init() {
    for (let msgName of this.MESSAGES) {
      Services.mm.addMessageListener(msgName, this);
    }

    this.originalPreallocatedEnabled = Services.prefs.getBoolPref(PREALLOCATED_PREF);
    Services.prefs.setBoolPref(PREALLOCATED_PREF, false);
  },

  uninit() {
    for (let msgName of this.MESSAGES) {
      Services.mm.removeMessageListener(msgName, this);
    }

    Services.prefs.setBoolPref(PREALLOCATED_PREF, this.originalPreallocatedEnabled);
  },

  receiveMessage(msg) {
    let browser = msg.target;
    let gBrowser = browser.ownerGlobal.gBrowser;

    switch (msg.name) {
      case "CPStartup:Go": {
        this.openTab(gBrowser).then(results =>
          this.reportResults(results));
        break;
      }

      case "Content:BrowserChildReady": {
        // Content has reported that it's ready to process an URL.
        if (!this.readyCallback) {
          throw new Error("Content:BrowserChildReady fired without a readyCallback set");
        }
        let tab = gBrowser.getTabForBrowser(browser);
        if (tab != this.tab) {
          // Let's ignore the message if it's not from the tab we've just opened.
          break;
        }
        // The child stopped the timer when it was ready to process the first URL, it's time to
        // calculate the difference and report it.
        let delta = msg.data.time - this.startStamp;
        this.readyCallback({tab, delta});
        break;
      }
    }
  },

  openTab(gBrowser) {
    return new Promise((resolve) => {
      // Start the timer and the profiler right before the tab open on the parent side.
      this.Profiler.resume("tab opening starts");
      this.startStamp = Services.telemetry.msSystemNow();
      this.tab = gBrowser.selectedTab = gBrowser.addTab(TARGET_URI, {
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      });

      this.whenTabReady().then(({tab, delta}) => {
        this.Profiler.pause("tab opening end");
        this.removeTab(tab).then(() => {
          resolve(delta);
        });
      });
    });
  },

  whenTabReady() {
    return new Promise((resolve) => {
      this.readyCallback = resolve;
    });
  },

  removeTab(tab) {
    return new Promise((resolve) => {
      let {messageManager: mm, frameLoader} = tab.linkedBrowser;
      mm.addMessageListener("SessionStore:update", function onMessage(msg) {
        if (msg.targetFrameLoader == frameLoader && msg.data.isFinal) {
          mm.removeMessageListener("SessionStore:update", onMessage);
          resolve();
        }
      }, true);

      tab.ownerGlobal.gBrowser.removeTab(tab);
    });
  },

  reportResults(results) {
    Services.mm.broadcastAsyncMessage("CPStartup:FinalResults", results);
  },
};

function install(aData, aReason) {}

function startup(aData, aReason) {
  CPStartup.init();
}

function shutdown(aData, aReason) {
  CPStartup.uninit();
}

function uninstall(aData, aReason) {}
