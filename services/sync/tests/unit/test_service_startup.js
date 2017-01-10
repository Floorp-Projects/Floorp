/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/observers.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

Svc.Prefs.set("registerEngines", "Tab,Bookmarks,Form,History");
Cu.import("resource://services-sync/service.js");

function run_test() {
  validate_all_future_pings();
  _("When imported, Service.onStartup is called");
  initTestLogging("Trace");

  let xps = Cc["@mozilla.org/weave/service;1"]
              .getService(Ci.nsISupports)
              .wrappedJSObject;
  do_check_false(xps.enabled);

  // Test fixtures
  Service.identity.username = "johndoe";
  do_check_true(xps.enabled);

  Cu.import("resource://services-sync/service.js");

  _("Service is enabled.");
  do_check_eq(Service.enabled, true);

  _("Engines are registered.");
  let engines = Service.engineManager.getAll();
  do_check_true(Utils.deepEquals(engines.map(engine => engine.name),
                                 ['tabs', 'bookmarks', 'forms', 'history']));

  _("Observers are notified of startup");
  do_test_pending();

  do_check_false(Service.status.ready);
  do_check_false(xps.ready);
  Observers.add("weave:service:ready", function(subject, data) {
    do_check_true(Service.status.ready);
    do_check_true(xps.ready);

    // Clean up.
    Svc.Prefs.resetBranch("");
    do_test_finished();
  });
}
