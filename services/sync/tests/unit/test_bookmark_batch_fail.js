_("Making sure a failing sync reports a useful error");
Cu.import("resource://services-sync/engines/bookmarks.js");

function run_test() {
  let engine = new BookmarksEngine();
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
