/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

initTestLogging("Trace");

function BlaEngine() {
  SyncEngine.call(this, "Bla", Service);
}
BlaEngine.prototype = {
  __proto__: SyncEngine.prototype,

  removed: false,
  async removeClientData() {
    this.removed = true;
  }

};

add_task(async function setup() {
  await Service.engineManager.register(BlaEngine);
});

add_task(async function test_resetLocalData() {
  await configureIdentity();
  Service.status.enforceBackoff = true;
  Service.status.backoffInterval = 42;
  Service.status.minimumNextSync = 23;

  // Verify set up.
  do_check_eq(Service.status.checkSetup(), STATUS_OK);

  // Verify state that the observer sees.
  let observerCalled = false;
  Svc.Obs.add("weave:service:start-over", function onStartOver() {
    Svc.Obs.remove("weave:service:start-over", onStartOver);
    observerCalled = true;

    do_check_eq(Service.status.service, CLIENT_NOT_CONFIGURED);
  });

  await Service.startOver();
  do_check_true(observerCalled);

  // Verify the site was nuked from orbit.
  do_check_eq(Svc.Prefs.get("username"), undefined);

  do_check_eq(Service.status.service, CLIENT_NOT_CONFIGURED);
  do_check_false(Service.status.enforceBackoff);
  do_check_eq(Service.status.backoffInterval, 0);
  do_check_eq(Service.status.minimumNextSync, 0);
});

add_task(async function test_removeClientData() {
  let engine = Service.engineManager.get("bla");

  // No cluster URL = no removal.
  do_check_false(engine.removed);
  await Service.startOver();
  do_check_false(engine.removed);

  Service.clusterURL = "https://localhost/";

  do_check_false(engine.removed);
  await Service.startOver();
  do_check_true(engine.removed);
});

add_task(async function test_reset_SyncScheduler() {
  // Some non-default values for SyncScheduler's attributes.
  Service.scheduler.idle = true;
  Service.scheduler.hasIncomingItems = true;
  Svc.Prefs.set("clients.devices.desktop", 42);
  Service.scheduler.nextSync = Date.now();
  Service.scheduler.syncThreshold = MULTI_DEVICE_THRESHOLD;
  Service.scheduler.syncInterval = Service.scheduler.activeInterval;

  await Service.startOver();

  do_check_false(Service.scheduler.idle);
  do_check_false(Service.scheduler.hasIncomingItems);
  do_check_eq(Service.scheduler.numClients, 0);
  do_check_eq(Service.scheduler.nextSync, 0);
  do_check_eq(Service.scheduler.syncThreshold, SINGLE_USER_THRESHOLD);
  do_check_eq(Service.scheduler.syncInterval, Service.scheduler.singleDeviceInterval);
});
