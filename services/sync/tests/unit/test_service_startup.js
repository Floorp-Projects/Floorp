/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/observers.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/util.js");

Svc.Prefs.set("registerEngines", "Tab,Bookmarks,Form,History");
Cu.import("resource://services-sync/service.js");

function run_test() {
  validate_all_future_pings();
  _("When imported, Service.onStartup is called");

  let xps = Cc["@mozilla.org/weave/service;1"]
              .getService(Ci.nsISupports)
              .wrappedJSObject;
  Assert.ok(!xps.enabled);

  // Test fixtures
  Service.identity.username = "johndoe";
  Assert.ok(xps.enabled);

  Cu.import("resource://services-sync/service.js");

  _("Service is enabled.");
  Assert.equal(Service.enabled, true);

  _("Observers are notified of startup");
  do_test_pending();

  Assert.ok(!Service.status.ready);
  Assert.ok(!xps.ready);

  Async.promiseSpinningly(promiseOneObserver("weave:service:ready"));

  Assert.ok(Service.status.ready);
  Assert.ok(xps.ready);

  _("Engines are registered.");
  let engines = Service.engineManager.getAll();
  Assert.ok(Utils.deepEquals(engines.map(engine => engine.name),
                             ["tabs", "bookmarks", "forms", "history"]));

  // Clean up.
  Svc.Prefs.resetBranch("");

  do_test_finished();
}
