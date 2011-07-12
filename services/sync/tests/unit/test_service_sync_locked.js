Cu.import("resource://services-sync/main.js");

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
  
  Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());
  
  augmentLogger(Weave.Service._log);

  // Avoid daily ping
  Weave.Svc.Prefs.set("lastPing", Math.floor(Date.now() / 1000));
  
  _("Check that sync will log appropriately if already in 'progress'.");
  Weave.Service._locked = true;
  Weave.Service.sync();
  Weave.Service._locked = false;
  
  do_check_eq(debug[debug.length - 2],
              "Exception: Could not acquire lock. Label: \"service.js: login\". No traceback available");
  do_check_eq(info[info.length - 1],
              "Cannot start sync: already syncing?");
}

