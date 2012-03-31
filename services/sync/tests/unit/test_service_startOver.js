/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/policies.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/util.js");

function BlaEngine() {
  SyncEngine.call(this, "Bla");
}
BlaEngine.prototype = {
  __proto__: SyncEngine.prototype,

  removed: false,
  removeClientData: function() {
    this.removed = true;
  }

};
Engines.register(BlaEngine);


function run_test() {
  initTestLogging("Trace");
  run_next_test();
}

add_test(function test_resetLocalData() {
  // Set up.
  setBasicCredentials("foobar", "blablabla", // Law Blog
                      "abcdeabcdeabcdeabcdeabcdea");
  Status.enforceBackoff = true;
  Status.backoffInterval = 42;
  Status.minimumNextSync = 23;
  Service.persistLogin();

  // Verify set up.
  do_check_eq(Status.checkSetup(), STATUS_OK);

  // Verify state that the observer sees.
  let observerCalled = false;
  Svc.Obs.add("weave:service:start-over", function onStartOver() {
    Svc.Obs.remove("weave:service:start-over", onStartOver);
    observerCalled = true;

    do_check_eq(Status.service, CLIENT_NOT_CONFIGURED);
  });

  Service.startOver();
  do_check_true(observerCalled);

  // Verify the site was nuked from orbit.
  do_check_eq(Svc.Prefs.get("username"), undefined);
  do_check_eq(Identity.basicPassword, null);
  do_check_eq(Identity.syncKey, null);

  do_check_eq(Status.service, CLIENT_NOT_CONFIGURED);
  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.backoffInterval, 0);
  do_check_eq(Status.minimumNextSync, 0);

  run_next_test();
});

add_test(function test_removeClientData() {
  let engine = Engines.get("bla");

  // No cluster URL = no removal.
  do_check_false(engine.removed);
  Service.startOver();
  do_check_false(engine.removed);

  Svc.Prefs.set("serverURL", TEST_SERVER_URL);
  Svc.Prefs.set("clusterURL", TEST_CLUSTER_URL);
  
  do_check_false(engine.removed);
  Service.startOver();
  do_check_true(engine.removed);

  run_next_test();
});

add_test(function test_reset_SyncScheduler() {
  // Some non-default values for SyncScheduler's attributes.
  SyncScheduler.idle = true;
  SyncScheduler.hasIncomingItems = true;
  SyncScheduler.numClients = 42;
  SyncScheduler.nextSync = Date.now();
  SyncScheduler.syncThreshold = MULTI_DEVICE_THRESHOLD;
  SyncScheduler.syncInterval = SyncScheduler.activeInterval;

  Service.startOver();

  do_check_false(SyncScheduler.idle);
  do_check_false(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.numClients, 0);
  do_check_eq(SyncScheduler.nextSync, 0);
  do_check_eq(SyncScheduler.syncThreshold, SINGLE_USER_THRESHOLD);
  do_check_eq(SyncScheduler.syncInterval, SyncScheduler.singleDeviceInterval);

  run_next_test();
});
