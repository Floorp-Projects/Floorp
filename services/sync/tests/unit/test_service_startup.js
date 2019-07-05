/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Svc.Prefs.set("registerEngines", "Tab,Bookmarks,Form,History");

add_task(async function run_test() {
  validate_all_future_pings();
  _("When imported, Service.onStartup is called");

  let xps = Cc["@mozilla.org/weave/service;1"].getService(Ci.nsISupports)
    .wrappedJSObject;
  Assert.ok(!xps.enabled);

  // Test fixtures
  let { Service } = ChromeUtils.import("resource://services-sync/service.js");
  Service.identity.username = "johndoe";
  Assert.ok(xps.enabled);

  _("Service is enabled.");
  Assert.equal(Service.enabled, true);

  _("Observers are notified of startup");
  Assert.ok(!Service.status.ready);
  Assert.ok(!xps.ready);

  await promiseOneObserver("weave:service:ready");

  Assert.ok(Service.status.ready);
  Assert.ok(xps.ready);

  _("Engines are registered.");
  let engines = Service.engineManager.getAll();
  Assert.ok(
    Utils.deepEquals(engines.map(engine => engine.name), [
      "tabs",
      "bookmarks",
      "forms",
      "history",
    ])
  );

  // Clean up.
  Svc.Prefs.resetBranch("");

  do_test_finished();
});
