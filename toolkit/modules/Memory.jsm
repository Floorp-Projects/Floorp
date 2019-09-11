/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["Memory"];

// How long we should wait for the Promise to resolve.
const TIMEOUT_INTERVAL = 2000;

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { setTimeout, clearTimeout } = ChromeUtils.import(
  "resource://gre/modules/Timer.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(
  this,
  "memory",
  "@mozilla.org/memory-reporter-manager;1",
  "nsIMemoryReporterManager"
);

var Memory = {
  /**
   * This function returns a Promise that resolves with an Object that
   * describes basic memory usage for each content process and the parent
   * process.
   * @returns Promise
   * @resolves JS Object
   * An Object in the following format:
   * {
   *   "parent": {
   *     uss: <int>,
   *     rss: <int>,
   *     ghosts: <int>,
   *   },
   *   <pid>: {
   *     uss: <int>,
   *     rss: <int>,
   *     ghosts: <int>,
   *   },
   *   ...
   * }
   */
  summary() {
    if (!this._pendingPromise) {
      this._pendingPromise = new Promise(resolve => {
        this._pendingResolve = resolve;
        this._summaries = {};
        this._origChildCount = Services.ppmm.childCount;
        Services.ppmm.broadcastAsyncMessage("Memory:GetSummary");
        Services.ppmm.addMessageListener("Memory:Summary", this);
        this._pendingTimeout = setTimeout(() => {
          this.finish();
        }, TIMEOUT_INTERVAL);
      });
    }
    return this._pendingPromise;
  },

  receiveMessage(msg) {
    if (msg.name != "Memory:Summary" || !this._pendingResolve) {
      return;
    }
    this._summaries[msg.data.pid] = msg.data.summary;
    // Now we check if we are done for all content processes.
    // Services.ppmm.childCount is a count of how many processes currently
    // exist that might respond to messages sent through the ppmm, including
    // the parent process. So we subtract the parent process with the "- 1",
    // and that’s how many content processes we’re waiting for.
    if (
      Object.keys(this._summaries).length >=
      Math.min(this._origChildCount, Services.ppmm.childCount) - 1
    ) {
      this.finish();
    }
  },

  finish() {
    // Code to gather the USS and RSS values for the parent process. This
    // functions the same way as in process-content.js.
    let rss = memory.resident;
    let uss = memory.residentUnique;
    let ghosts = memory.ghostWindows;
    this._summaries.Parent = { uss, rss, ghosts };
    this._pendingResolve(this._summaries);
    this._pendingResolve = null;
    this._summaries = null;
    this._pendingPromise = null;
    clearTimeout(this._pendingTimeout);
    Services.ppmm.removeMessageListener("Memory:Summary", this);
  },
};
