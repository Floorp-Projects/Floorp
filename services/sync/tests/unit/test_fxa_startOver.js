/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://testing-common/services/sync/utils.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/browserid_identity.js");
Cu.import("resource://services-sync/service.js");

function run_test() {
  initTestLogging("Trace");
  run_next_test();
}

add_task(async function test_startover() {
  let oldValue = Services.prefs.getBoolPref("services.sync-testing.startOverKeepIdentity", true);
  Services.prefs.setBoolPref("services.sync-testing.startOverKeepIdentity", false);

  ensureLegacyIdentityManager();
  await configureIdentity({username: "johndoe"});

  // The boolean flag on the xpcom service should reflect a legacy provider.
  let xps = Cc["@mozilla.org/weave/service;1"]
            .getService(Components.interfaces.nsISupports)
            .wrappedJSObject;
  do_check_false(xps.fxAccountsEnabled);

  // we expect the "legacy" provider (but can't instanceof that, as BrowserIDManager
  // extends it)
  do_check_false(Service.identity instanceof BrowserIDManager);

  Service.serverURL = "https://localhost/";
  Service.clusterURL = Service.serverURL;

  Service.login();
  // We should have a cluster URL
  do_check_true(Service.clusterURL.length > 0);

  // remember some stuff so we can reset it after.
  let oldIdentity = Service.identity;
  let oldClusterManager = Service._clusterManager;
  let promiseStartOver = new Promise(resolve => {
    Services.obs.addObserver(function observeStartOverFinished() {
      Services.obs.removeObserver(observeStartOverFinished, "weave:service:start-over:finish");
      resolve();
    }, "weave:service:start-over:finish", false);
  });

  Service.startOver();
  await promiseStartOver; // wait for the observer to fire.

  // the xpcom service should indicate FxA is enabled.
  do_check_true(xps.fxAccountsEnabled);
  // should have swapped identities.
  do_check_true(Service.identity instanceof BrowserIDManager);
  // should have clobbered the cluster URL
  do_check_eq(Service.clusterURL, "");

  // we should have thrown away the old identity provider and cluster manager.
  do_check_neq(oldIdentity, Service.identity);
  do_check_neq(oldClusterManager, Service._clusterManager);

  // reset the world.
  Services.prefs.setBoolPref("services.sync-testing.startOverKeepIdentity", oldValue);
});
