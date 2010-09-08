Cu.import("resource://services-sync/ext/Observers.js");
Cu.import("resource://services-sync/ext/Sync.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/engines.js");

function run_test() {
  _("When imported, Service.onStartup is called");

  // Test fixtures
  let observerCalled = false;
  Observers.add("weave:service:ready", function (subject, data) {
    observerCalled = true;
  });
  Svc.Prefs.set("registerEngines", "Tab,Bookmarks,Form,History");
  Svc.Prefs.set("username", "johndoe");

  try {
    Cu.import("resource://services-sync/service.js");

    _("Service is enabled.");
    do_check_eq(Service.enabled, true);

    _("Engines are registered.");
    let engines = Engines.getAll();
    do_check_true(Utils.deepEquals([engine.name for each (engine in engines)],
                                   ['tabs', 'bookmarks', 'forms', 'history']));

    _("Identities are registered.");
    do_check_eq(ID.get('WeaveID').username, "johndoe");
    do_check_eq(ID.get('WeaveCryptoID').username, "johndoe");

    _("Observers are notified of startup");
    // Synchronize with Service.onStartup's async notification
    Sync.sleep(0);
    do_check_true(observerCalled);

  } finally {
    Svc.Prefs.resetBranch("");
  }
}
