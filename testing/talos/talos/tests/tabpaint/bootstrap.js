/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This test will measure the performance of opening a tab and displaying
 * the simplest content (a blank page). This test is concerned primarily
 * with the time it takes to show the content area to the user, rather than
 * the performance of the tab opening or closing animations (which are
 * measured in the TART test). This is why this test disables tab animations.
 *
 * When opening tabs and showing content, there are two cases to consider:
 *
 * 1) The tab has been opened via a command from the parent process. For example,
 *    the user has clicked on a link in a separate application which is opening
 *    in this browser, so the message to open the tab and display the content
 *    comes from the operating system, to the parent.
 *
 * 2) The tab has been opened by clicking on a link in content. It is possible
 *    for certain types of links (_blank links for example) to open new tabs.
 */

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource:///modules/RecentWindow.jsm");

const TAB_ANIMATION_PREF = "browser.tabs.animate";

const PROCESS_COUNT_PREF = "dom.ipc.processCount";

const TARGET_URI = "chrome://tabpaint/content/target.html";

var TabPaint = {
  MESSAGES: [
    "TabPaint:Go",
    "TabPaint:Painted",
  ],

  /**
   * We'll hold the original tab animation preference here so
   * we can restore it once the test is done.
   */
  originalTabsAnimate: null,

  /**
   * We'll store a callback here to be fired once the TARGET_URI
   * reports that it has painted.
   */
  paintCallback: null,

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
    // We don't have a window in this scope, and in fact, there might
    // not be any browser windows around. Since pageloader is loading
    // this add-on, along with the tabpaint.html content, what we'll do
    // is wait for the tabpaint.html content to send us a message to
    // get us moving.
    for (let msgName of this.MESSAGES) {
      Services.mm.addMessageListener(msgName, this);
    }

    this.originalTabsAnimate = Services.prefs.getBoolPref(TAB_ANIMATION_PREF);
    Services.prefs.setBoolPref(TAB_ANIMATION_PREF, false);
    this.originalProcessCount = Services.prefs.getIntPref(PROCESS_COUNT_PREF);
    Services.prefs.setIntPref(PROCESS_COUNT_PREF, 1);
  },

  uninit() {
    for (let msgName of this.MESSAGES) {
      Services.mm.removeMessageListener(msgName, this);
    }

    Services.prefs.setBoolPref(TAB_ANIMATION_PREF, this.originalTabsAnimate);
    Services.prefs.setIntPref(PROCESS_COUNT_PREF, this.originalProcessCount);
  },

  receiveMessage(msg) {
    let browser = msg.target;

    let gBrowser = browser.ownerGlobal.gBrowser;

    switch(msg.name) {
      case "TabPaint:Go": {
        // Our document has loaded, and we're off to the races!
        this.go(gBrowser).then((results) => {
          this.reportResults(results);
        });

        break;
      }

      case "TabPaint:Painted": {
        // Content has reported that it has painted.
        if (!this.paintCallback) {
          throw new Error("TabPaint:Painted fired without a paintCallback set");
        }

        let tab = gBrowser.getTabForBrowser(browser);
        let delta = msg.data.delta;
        this.paintCallback({tab, delta});
        break;
      }
    }
  },

  /**
   * Start a single run of the test. This will measure the time
   * to open a new tab from the parent, and then measure the time
   * to open a new tab from content.
   *
   * @param gBrowser (<xul:tabbrowser>)
   *        The tabbrowser of the window to use.
   *
   * @return Promise
   *         Resolves with an object with two properties:
   *
   *          fromParent (int):
   *            The time (ms) it took to present a tab that
   *            was opened from the parent.
   *
   *          fromContent (int):
   *            The time (ms) it took to present a tab that
   *            was opened from content.
   */
  go: Task.async(function*(gBrowser) {
    let fromParent = yield this.openTabFromParent(gBrowser);
    let fromContent = yield this.openTabFromContent(gBrowser);

    return { fromParent, fromContent };
  }),

  /**
   * Opens a tab from the parent, waits until it is displayed, then
   * removes the tab.
   *
   * @param gBrowser (<xul:tabbrowser>)
   *        The tabbrowser of the window to use.
   *
   * @return Promise
   *         Resolves once the tab has been fully removed. Resolves
   *         with the time (in ms) it took to open the tab from the parent.
   */
  openTabFromParent(gBrowser) {
    let win = gBrowser.ownerGlobal;
    return new Promise((resolve) => {
      this.Profiler.resume("tabpaint parent start");

      gBrowser.selectedTab = gBrowser.addTab(TARGET_URI + "?" + Date.now());

      this.whenTabShown().then(({tab, delta}) => {
        this.Profiler.pause("tabpaint parent end");
        this.removeTab(tab).then(() => {
          resolve(delta);
        });
      });
    });
  },

  /**
   * Opens a tab from content, waits until it is displayed, then
   * removes the tab.
   *
   * @param gBrowser (<xul:tabbrowser>)
   *        The tabbrowser of the window to use.
   *
   * @return Promise
   *         Resolves once the tab has been fully removed. Resolves
   *         with the time (in ms) it took to open the tab from content.
   */
  openTabFromContent(gBrowser) {
    let win = gBrowser.ownerGlobal;
    return new Promise((resolve) => {
      this.Profiler.resume("tabpaint content start");

      Services.mm.broadcastAsyncMessage("TabPaint:OpenFromContent");

      this.whenTabShown().then(({tab, delta}) => {
        this.Profiler.pause("tabpaint content end");
        this.removeTab(tab).then(() => {
          resolve(delta);
        });
      });
    });
  },

  /**
   * Returns a Promise that will resolve once the next tab reports
   * it has shown.
   *
   * @return Promise
   */
  whenTabShown() {
    return new Promise((resolve) => {
      this.paintCallback = resolve;
    });
  },

  /**
   * Returns a Promise that removes a tab, and resolves once the final
   * message from that tab has come up.
   *
   * @param tab (<xul:tab>)
   *        The tab to remove.
   *
   * @return Promise
   */
  removeTab(tab) {
    return new Promise((resolve) => {
      let {messageManager: mm, frameLoader} = tab.linkedBrowser;
      mm.addMessageListener("SessionStore:update", function onMessage(msg) {
        if (msg.targetFrameLoader == frameLoader && msg.data.isFinal) {
          mm.removeMessageListener("SessionStore:update", onMessage);
          resolve();
        }
      }, true);

      tab.ownerDocument.defaultView.gBrowser.removeTab(tab);
    });
  },

  /**
   * Sends the results down to the tabpaint-test.html page, which will
   * pipe them out to the console.
   *
   * @param results (Object)
   *        An object with the following properties:
   *
   *          fromParent (int):
   *            The time (ms) it took to present a tab that
   *            was opened from the parent.
   *
   *          fromContent (int):
   *            The time (ms) it took to present a tab that
   *            was opened from content.
   */
  reportResults(results) {
    Services.mm.broadcastAsyncMessage("TabPaint:FinalResults", results);
  },
};

// Bootstrap functions below

function install(aData, aReason) {}

function startup(aData, aReason) {
  TabPaint.init();
}

function shutdown(aData, aReason) {
  TabPaint.uninit();
}

function uninstall(aData, aReason) {}
