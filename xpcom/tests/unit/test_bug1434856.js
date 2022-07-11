/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/  */

function run_test() {
  let complete = false;

  let runnable = {
    internalQI: ChromeUtils.generateQI(["nsIRunnable"]),
    // eslint-disable-next-line mozilla/use-chromeutils-generateqi
    QueryInterface(iid) {
      // Attempt to schedule another runnable.  This simulates a GC/CC
      // being scheduled while executing the JS QI.
      Services.tm.dispatchToMainThread(() => false);
      return this.internalQI(iid);
    },

    run() {
      complete = true;
    },
  };

  Services.tm.dispatchToMainThread(runnable);
  Services.tm.spinEventLoopUntil(
    "Test(test_bug1434856.js:run_test)",
    () => complete
  );
}
