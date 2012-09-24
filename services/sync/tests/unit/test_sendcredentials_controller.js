/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/jpakeclient.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

function run_test() {
  setBasicCredentials("johndoe", "ilovejane", Utils.generatePassphrase());
  Service.serverURL  = "http://weave.server/";

  initTestLogging("Trace");
  Log4Moz.repository.getLogger("Sync.SendCredentialsController").level = Log4Moz.Level.Trace;
  Log4Moz.repository.getLogger("Sync.SyncScheduler").level = Log4Moz.Level.Trace;
  run_next_test();
}

function make_sendCredentials_test(topic) {
  return function test_sendCredentials() {
    _("Test sending credentials on " + topic + " observer notification.");

    let sendAndCompleteCalled = false;
    let jpakeclient = {
      sendAndComplete: function sendAndComplete(data) {
        // Verify that the controller unregisters itself as an observer
        // when the exchange is complete by faking another notification.
        do_check_false(sendAndCompleteCalled);
        sendAndCompleteCalled = true;

        // Verify it sends the correct data.
        do_check_eq(data.account,   Service.identity.account);
        do_check_eq(data.password,  Service.identity.basicPassword);
        do_check_eq(data.synckey,   Service.identity.syncKey);
        do_check_eq(data.serverURL, Service.serverURL);

        this.controller.onComplete();
        // Verify it schedules a sync for the expected interval.
        let expectedInterval = Service.scheduler.activeInterval;
        do_check_true(Service.scheduler.nextSync - Date.now() <= expectedInterval);

        // Signal the end of another sync. We shouldn't be registered anymore,
        // so we shouldn't re-enter this method (cf sendAndCompleteCalled above)
        Svc.Obs.notify(topic);

        Service.scheduler.setDefaults();
        Utils.nextTick(run_next_test);
      }
    };
    jpakeclient.controller = new SendCredentialsController(jpakeclient, Service);
    Svc.Obs.notify(topic);
  };
}

add_test(make_sendCredentials_test("weave:service:sync:finish"));
add_test(make_sendCredentials_test("weave:service:sync:error"));


add_test(function test_abort() {
  _("Test aborting the J-PAKE exchange.");

  let jpakeclient = {
    sendAndComplete: function sendAndComplete() {
      do_throw("Shouldn't get here!");
    }
  };
  jpakeclient.controller = new SendCredentialsController(jpakeclient, Service);

  // Verify that the controller unregisters itself when the exchange
  // was aborted.
  jpakeclient.controller.onAbort(JPAKE_ERROR_USERABORT);
  Svc.Obs.notify("weave:service:sync:finish");
  Utils.nextTick(run_next_test);
});


add_test(function test_startOver() {
  _("Test wiping local Sync config aborts transaction.");

  let abortCalled = false;
  let jpakeclient = {
    abort: function abort() {
      abortCalled = true;
      this.controller.onAbort(JPAKE_ERROR_USERABORT);
    },
    sendAndComplete: function sendAndComplete() {
      do_throw("Shouldn't get here!");
    }
  };
  jpakeclient.controller = new SendCredentialsController(jpakeclient, Service);

  Svc.Obs.notify("weave:service:start-over");
  do_check_true(abortCalled);

  // Ensure that the controller no longer does anything if a sync
  // finishes now or -- more likely -- errors out.
  Svc.Obs.notify("weave:service:sync:error");

  Utils.nextTick(run_next_test);
});
