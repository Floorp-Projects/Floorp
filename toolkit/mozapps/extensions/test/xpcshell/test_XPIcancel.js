/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test the cancellable doing/done/cancelAll API in XPIProvider

ChromeUtils.import("resource://gre/modules/addons/XPIInstall.jsm");

function run_test() {
  // Check that cancelling with nothing in progress doesn't blow up
  XPIInstall.cancelAll();

  // Check that a basic object gets cancelled
  let getsCancelled = {
    isCancelled: false,
    cancel() {
      if (this.isCancelled)
        do_throw("Already cancelled");
      this.isCancelled = true;
    }
  };
  XPIInstall.doing(getsCancelled);
  XPIInstall.cancelAll();
  Assert.ok(getsCancelled.isCancelled);

  // Check that if we complete a cancellable, it doesn't get cancelled
  let doesntGetCancelled = {
    cancel: () => do_throw("This should not have been cancelled")
  };
  XPIInstall.doing(doesntGetCancelled);
  Assert.ok(XPIInstall.done(doesntGetCancelled));
  XPIInstall.cancelAll();

  // A cancellable that adds a cancellable
  getsCancelled.isCancelled = false;
  let addsAnother = {
    isCancelled: false,
    cancel() {
      if (this.isCancelled)
        do_throw("Already cancelled");
      this.isCancelled = true;
      XPIInstall.doing(getsCancelled);
    }
  };
  XPIInstall.doing(addsAnother);
  XPIInstall.cancelAll();
  Assert.ok(addsAnother.isCancelled);
  Assert.ok(getsCancelled.isCancelled);

  // A cancellable that removes another. This assumes that Set() iterates in the
  // order that members were added
  let removesAnother = {
    isCancelled: false,
    cancel() {
      if (this.isCancelled)
        do_throw("Already cancelled");
      this.isCancelled = true;
      XPIInstall.done(doesntGetCancelled);
    }
  };
  XPIInstall.doing(removesAnother);
  XPIInstall.doing(doesntGetCancelled);
  XPIInstall.cancelAll();
  Assert.ok(removesAnother.isCancelled);
}
