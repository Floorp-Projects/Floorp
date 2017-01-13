/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

_("Making sure a failing sync reports a useful error");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/service.js");

function run_test() {
  let engine = new BookmarksEngine(Service);
  engine._syncStartup = function() {
    throw "FAIL!";
  };

  try {
    _("Try calling the sync that should throw right away");
    engine._sync();
    do_throw("Should have failed sync!");
  }
  catch(ex) {
    _("Making sure what we threw ended up as the exception:", ex);
    do_check_eq(ex, "FAIL!");
  }
}
