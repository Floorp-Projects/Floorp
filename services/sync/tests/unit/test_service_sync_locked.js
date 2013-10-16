/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

function run_test() {
  let debug = [];
  let info  = [];

  function augmentLogger(old) {
    let d = old.debug;
    let i = old.info;
    old.debug = function(m) { debug.push(m); d.call(old, m); }
    old.info  = function(m) { info.push(m);  i.call(old, m); }
    return old;
  }

  Log.repository.rootLogger.addAppender(new Log.DumpAppender());

  augmentLogger(Service._log);

  // Avoid daily ping
  Svc.Prefs.set("lastPing", Math.floor(Date.now() / 1000));

  _("Check that sync will log appropriately if already in 'progress'.");
  Service._locked = true;
  Service.sync();
  Service._locked = false;

  do_check_eq(debug[debug.length - 2],
              "Exception: Could not acquire lock. Label: \"service.js: login\". No traceback available");
  do_check_eq(info[info.length - 1],
              "Cannot start sync: already syncing?");
}

