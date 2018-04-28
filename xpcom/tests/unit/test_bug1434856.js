/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/  */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

function run_test() {
  let complete = false;

  let runnable = {
    internalQI: ChromeUtils.generateQI([Ci.nsIRunnable]),
    QueryInterface(iid) {
      // Attempt to schedule another runnable.  This simulates a GC/CC
      // being scheduled while executing the JS QI.
      Services.tm.dispatchToMainThread(() => false);
      return this.internalQI(iid);
    },

    run() {
      complete = true;
    }
  };

  Services.tm.dispatchToMainThread(runnable);
  Services.tm.spinEventLoopUntil(() => complete);
}
