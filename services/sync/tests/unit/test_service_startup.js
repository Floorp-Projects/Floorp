/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/ext/Observers.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/engines.js");

function run_test() {
  _("When imported, Service.onStartup is called");

  Svc.Prefs.set("registerEngines", "Tab,Bookmarks,Form,History");
  new SyncTestingInfrastructure();

  // Test fixtures
  Identity.username = "johndoe";

  Cu.import("resource://services-sync/service.js");

  _("Service is enabled.");
  do_check_eq(Service.enabled, true);

  _("Engines are registered.");
  let engines = Engines.getAll();
  do_check_true(Utils.deepEquals([engine.name for each (engine in engines)],
                                 ['tabs', 'bookmarks', 'forms', 'history']));

  _("Observers are notified of startup");
  do_test_pending();
  do_check_false(Status.ready);
  Observers.add("weave:service:ready", function (subject, data) {
    do_check_true(Status.ready);

    // Clean up.
    Svc.Prefs.resetBranch("");
    do_test_finished();
  });
}
