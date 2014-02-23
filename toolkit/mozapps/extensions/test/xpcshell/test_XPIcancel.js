/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test the cancellable doing/done/cancelAll API in XPIProvider

let scope = Components.utils.import("resource://gre/modules/addons/XPIProvider.jsm");
let XPIProvider = scope.XPIProvider;

function run_test() {
  // Check that cancelling with nothing in progress doesn't blow up
  XPIProvider.cancelAll();

  // Check that a basic object gets cancelled
  let getsCancelled = {
    isCancelled: false,
    cancel: function () {
      if (this.isCancelled)
        do_throw("Already cancelled");
      this.isCancelled = true;
    }
  };
  XPIProvider.doing(getsCancelled);
  XPIProvider.cancelAll();
  do_check_true(getsCancelled.isCancelled);

  // Check that if we complete a cancellable, it doesn't get cancelled
  let doesntGetCancelled = {
    cancel: () => do_throw("This should not have been cancelled")
  };
  XPIProvider.doing(doesntGetCancelled);
  do_check_true(XPIProvider.done(doesntGetCancelled));
  XPIProvider.cancelAll();

  // A cancellable that adds a cancellable
  getsCancelled.isCancelled = false;
  let addsAnother = {
    isCancelled: false,
    cancel: function () {
      if (this.isCancelled)
        do_throw("Already cancelled");
      this.isCancelled = true;
      XPIProvider.doing(getsCancelled);
    }
  }
  XPIProvider.doing(addsAnother);
  XPIProvider.cancelAll();
  do_check_true(addsAnother.isCancelled);
  do_check_true(getsCancelled.isCancelled);

  // A cancellable that removes another. This assumes that Set() iterates in the
  // order that members were added
  let removesAnother = {
    isCancelled: false,
    cancel: function () {
      if (this.isCancelled)
        do_throw("Already cancelled");
      this.isCancelled = true;
      XPIProvider.done(doesntGetCancelled);
    }
  }
  XPIProvider.doing(removesAnother);
  XPIProvider.doing(doesntGetCancelled);
  XPIProvider.cancelAll();
  do_check_true(removesAnother.isCancelled);
}
