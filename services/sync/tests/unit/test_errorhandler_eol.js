/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/util.js");

Cu.import("resource://testing-common/services/sync/fakeservices.js");

function baseHandler(eolCode, request, response, statusCode, status, body) {
  let alertBody = {
    code: eolCode,
    message: "Service is EOLed.",
    url: "http://getfirefox.com",
  };
  response.setHeader("X-Weave-Timestamp", "" + new_timestamp(), false);
  response.setHeader("X-Weave-Alert", "" + JSON.stringify(alertBody), false);
  response.setStatusLine(request.httpVersion, statusCode, status);
  response.bodyOutputStream.write(body, body.length);
}

function handler513(request, response) {
  let statusCode = 513;
  let status = "Upgrade Required";
  let body = "{}";
  baseHandler("hard-eol", request, response, statusCode, status, body);
}

function handler200(eolCode) {
  return function(request, response) {
    let statusCode = 200;
    let status = "OK";
    let body = "{\"meta\": 123456789010}";
    baseHandler(eolCode, request, response, statusCode, status, body);
  };
}

function sync_httpd_setup(infoHandler) {
  let handlers = {
    "/1.1/johndoe/info/collections": infoHandler,
  };
  return httpd_setup(handlers);
}

async function setUp(server) {
  await configureIdentity({username: "johndoe"}, server);
  new FakeCryptoService();
}

function do_check_soft_eol(eh, start) {
  // We subtract 1000 because the stored value is in second precision.
  Assert.ok(eh.earliestNextAlert >= (start + eh.MINIMUM_ALERT_INTERVAL_MSEC - 1000));
  Assert.equal("soft-eol", eh.currentAlertMode);
}
function do_check_hard_eol(eh, start) {
  // We subtract 1000 because the stored value is in second precision.
  Assert.ok(eh.earliestNextAlert >= (start + eh.MINIMUM_ALERT_INTERVAL_MSEC - 1000));
  Assert.equal("hard-eol", eh.currentAlertMode);
  Assert.ok(Status.eol);
}

add_task(async function test_200_hard() {
  let eh = Service.errorHandler;
  let start = Date.now();
  let server = sync_httpd_setup(handler200("hard-eol"));
  await setUp(server);

  let promiseObserved = promiseOneObserver("weave:eol");

  await Service._fetchInfo();
  Service.scheduler.adjustSyncInterval(); // As if we failed or succeeded in syncing.

  let { subject } = await promiseObserved;
  Assert.equal("hard-eol", subject.code);
  do_check_hard_eol(eh, start);
  Assert.equal(Service.scheduler.eolInterval, Service.scheduler.syncInterval);
  eh.clearServerAlerts();
  await promiseStopServer(server);
});

add_task(async function test_513_hard() {
  let eh = Service.errorHandler;
  let start = Date.now();
  let server = sync_httpd_setup(handler513);
  await setUp(server);

  let promiseObserved = promiseOneObserver("weave:eol");

  try {
    await Service._fetchInfo();
    Service.scheduler.adjustSyncInterval(); // As if we failed or succeeded in syncing.
  } catch (ex) {
    // Because fetchInfo will fail on a 513.
  }
  let { subject } = await promiseObserved;
  Assert.equal("hard-eol", subject.code);
  do_check_hard_eol(eh, start);
  Assert.equal(Service.scheduler.eolInterval, Service.scheduler.syncInterval);
  eh.clearServerAlerts();

  await promiseStopServer(server);
});

add_task(async function test_200_soft() {
  let eh = Service.errorHandler;
  let start = Date.now();
  let server = sync_httpd_setup(handler200("soft-eol"));
  await setUp(server);

  let promiseObserved = promiseOneObserver("weave:eol");

  await Service._fetchInfo();
  Service.scheduler.adjustSyncInterval(); // As if we failed or succeeded in syncing.
  let { subject } = await promiseObserved;
  Assert.equal("soft-eol", subject.code);
  do_check_soft_eol(eh, start);
  Assert.equal(Service.scheduler.singleDeviceInterval, Service.scheduler.syncInterval);
  eh.clearServerAlerts();

  await promiseStopServer(server);
});
