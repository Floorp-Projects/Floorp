/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["Memory"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
// How long we should wait for the Promise to resolve.
const TIMEOUT_INTERVAL = 2000;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Timer.jsm");

this.Memory = {
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
   *   },
   *   <pid>: {
   *     uss: <int>,
   *     rss: <int>,
   *   },
   *   ...
   * }
   */
  summary() {
    if (!this._pendingPromise) {
      this._pendingPromise = new Promise((resolve) => {
        this._pendingResolve = resolve;
        this._summaries = {};
        Services.ppmm.broadcastAsyncMessage("Memory:GetSummary");
        Services.ppmm.addMessageListener("Memory:Summary", this);
        this._pendingTimeout = setTimeout(() => { this.finish(); }, TIMEOUT_INTERVAL);
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
    if (Object.keys(this._summaries).length >= Services.ppmm.childCount - 1) {
      this.finish();
    }
  },

  finish() {
    // Code to gather the USS and RSS values for the parent process. This
    // functions the same way as in process-content.js.
    let memMgr = Cc["@mozilla.org/memory-reporter-manager;1"]
                   .getService(Ci.nsIMemoryReporterManager);
    let rss = memMgr.resident;
    let uss = memMgr.residentUnique;
    this._summaries["Parent"] = { uss, rss };
    this._pendingResolve(this._summaries);
    this._pendingResolve = null;
    this._summaries = null;
    this._pendingPromise = null;
    clearTimeout(this._pendingTimeout);
    Services.ppmm.removeMessageListener("Memory:Summary", this);
  }
};
