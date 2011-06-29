Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/service.js");
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

add_test(function test_removeClientData() {
  let engine = Engines.get("bla");

  // No cluster URL = no removal.
  do_check_false(engine.removed);
  Service.startOver();
  do_check_false(engine.removed);

  Svc.Prefs.set("clusterURL", "http://localhost:8080/");
  do_check_false(engine.removed);
  Service.startOver();
  do_check_true(engine.removed);

  run_next_test();
});

add_test(function test_reset_SyncScheduler() {
  // Some non-defualt values for SyncScheduler's attributes.
  SyncScheduler.idle = true;
  SyncScheduler.hasIncomingItems = true;
  SyncScheduler.numClients = 42;
  SyncScheduler.nextSync = Date.now();
  SyncScheduler.syncThreshold = MULTI_DEVICE_THRESHOLD;
  SyncScheduler.syncInterval = MULTI_DEVICE_ACTIVE_SYNC;

  Service.startOver();

  do_check_false(SyncScheduler.idle);
  do_check_false(SyncScheduler.hasIncomingItems);
  do_check_eq(SyncScheduler.numClients, 0);
  do_check_eq(SyncScheduler.nextSync, 0);
  do_check_eq(SyncScheduler.syncInterval, SINGLE_USER_SYNC);
  do_check_eq(SyncScheduler.syncThreshold, SINGLE_USER_THRESHOLD);

  run_next_test();
});
