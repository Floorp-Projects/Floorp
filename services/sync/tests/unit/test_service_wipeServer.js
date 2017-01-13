Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://testing-common/services/sync/fakeservices.js");
Cu.import("resource://testing-common/services/sync/utils.js");

Svc.DefaultPrefs.set("registerEngines", "");
Cu.import("resource://services-sync/service.js");

// configure the identity we use for this test.
const identityConfig = makeIdentityConfig({username: "johndoe"});

function FakeCollection() {
  this.deleted = false;
}
FakeCollection.prototype = {
  handler: function() {
    let self = this;
    return function(request, response) {
      let body = "";
      self.timestamp = new_timestamp();
      let timestamp = "" + self.timestamp;
      if (request.method == "DELETE") {
          body = timestamp;
          self.deleted = true;
      }
      response.setHeader("X-Weave-Timestamp", timestamp);
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(body, body.length);
    };
  }
};

async function setUpTestFixtures(server) {
  let cryptoService = new FakeCryptoService();

  Service.serverURL = server.baseURI + "/";
  Service.clusterURL = server.baseURI + "/";

  await configureIdentity(identityConfig);
}


function run_test() {
  initTestLogging("Trace");
  run_next_test();
}

add_identity_test(this, async function test_wipeServer_list_success() {
  _("Service.wipeServer() deletes collections given as argument.");

  let steam_coll = new FakeCollection();
  let diesel_coll = new FakeCollection();

  let server = httpd_setup({
    "/1.1/johndoe/storage/steam": steam_coll.handler(),
    "/1.1/johndoe/storage/diesel": diesel_coll.handler(),
    "/1.1/johndoe/storage/petrol": httpd_handler(404, "Not Found")
  });

  try {
    await setUpTestFixtures(server);
    await SyncTestingInfrastructure(server, "johndoe", "irrelevant");

    _("Confirm initial environment.");
    do_check_false(steam_coll.deleted);
    do_check_false(diesel_coll.deleted);

    _("wipeServer() will happily ignore the non-existent collection and use the timestamp of the last DELETE that was successful.");
    let timestamp = Service.wipeServer(["steam", "diesel", "petrol"]);
    do_check_eq(timestamp, diesel_coll.timestamp);

    _("wipeServer stopped deleting after encountering an error with the 'petrol' collection, thus only 'steam' has been deleted.");
    do_check_true(steam_coll.deleted);
    do_check_true(diesel_coll.deleted);

  } finally {
    await promiseStopServer(server);
    Svc.Prefs.resetBranch("");
  }
});

add_identity_test(this, async function test_wipeServer_list_503() {
  _("Service.wipeServer() deletes collections given as argument.");

  let steam_coll = new FakeCollection();
  let diesel_coll = new FakeCollection();

  let server = httpd_setup({
    "/1.1/johndoe/storage/steam": steam_coll.handler(),
    "/1.1/johndoe/storage/petrol": httpd_handler(503, "Service Unavailable"),
    "/1.1/johndoe/storage/diesel": diesel_coll.handler()
  });

  try {
    await setUpTestFixtures(server);
    await SyncTestingInfrastructure(server, "johndoe", "irrelevant");

    _("Confirm initial environment.");
    do_check_false(steam_coll.deleted);
    do_check_false(diesel_coll.deleted);

    _("wipeServer() will happily ignore the non-existent collection, delete the 'steam' collection and abort after an receiving an error on the 'petrol' collection.");
    let error;
    try {
      Service.wipeServer(["non-existent", "steam", "petrol", "diesel"]);
      do_throw("Should have thrown!");
    } catch(ex) {
      error = ex;
    }
    _("wipeServer() threw this exception: " + error);
    do_check_eq(error.status, 503);

    _("wipeServer stopped deleting after encountering an error with the 'petrol' collection, thus only 'steam' has been deleted.");
    do_check_true(steam_coll.deleted);
    do_check_false(diesel_coll.deleted);

  } finally {
    await promiseStopServer(server);
    Svc.Prefs.resetBranch("");
  }
});

add_identity_test(this, async function test_wipeServer_all_success() {
  _("Service.wipeServer() deletes all the things.");

  /**
   * Handle the bulk DELETE request sent by wipeServer.
   */
  let deleted = false;
  let serverTimestamp;
  function storageHandler(request, response) {
    do_check_eq("DELETE", request.method);
    do_check_true(request.hasHeader("X-Confirm-Delete"));
    deleted = true;
    serverTimestamp = return_timestamp(request, response);
  }

  let server = httpd_setup({
    "/1.1/johndoe/storage": storageHandler
  });
  await setUpTestFixtures(server);

  _("Try deletion.");
  await SyncTestingInfrastructure(server, "johndoe", "irrelevant");
  let returnedTimestamp = Service.wipeServer();
  do_check_true(deleted);
  do_check_eq(returnedTimestamp, serverTimestamp);

  await promiseStopServer(server);
  Svc.Prefs.resetBranch("");
});

add_identity_test(this, async function test_wipeServer_all_404() {
  _("Service.wipeServer() accepts a 404.");

  /**
   * Handle the bulk DELETE request sent by wipeServer. Returns a 404.
   */
  let deleted = false;
  let serverTimestamp;
  function storageHandler(request, response) {
    do_check_eq("DELETE", request.method);
    do_check_true(request.hasHeader("X-Confirm-Delete"));
    deleted = true;
    serverTimestamp = new_timestamp();
    response.setHeader("X-Weave-Timestamp", "" + serverTimestamp);
    response.setStatusLine(request.httpVersion, 404, "Not Found");
  }

  let server = httpd_setup({
    "/1.1/johndoe/storage": storageHandler
  });
  await setUpTestFixtures(server);

  _("Try deletion.");
  await SyncTestingInfrastructure(server, "johndoe", "irrelevant");
  let returnedTimestamp = Service.wipeServer();
  do_check_true(deleted);
  do_check_eq(returnedTimestamp, serverTimestamp);

  await promiseStopServer(server);
  Svc.Prefs.resetBranch("");
});

add_identity_test(this, async function test_wipeServer_all_503() {
  _("Service.wipeServer() throws if it encounters a non-200/404 response.");

  /**
   * Handle the bulk DELETE request sent by wipeServer. Returns a 503.
   */
  function storageHandler(request, response) {
    do_check_eq("DELETE", request.method);
    do_check_true(request.hasHeader("X-Confirm-Delete"));
    response.setStatusLine(request.httpVersion, 503, "Service Unavailable");
  }

  let server = httpd_setup({
    "/1.1/johndoe/storage": storageHandler
  });
  await setUpTestFixtures(server);

  _("Try deletion.");
  let error;
  try {
    await SyncTestingInfrastructure(server, "johndoe", "irrelevant");
    Service.wipeServer();
    do_throw("Should have thrown!");
  } catch (ex) {
    error = ex;
  }
  do_check_eq(error.status, 503);

  await promiseStopServer(server);
  Svc.Prefs.resetBranch("");
});

add_identity_test(this, async function test_wipeServer_all_connectionRefused() {
  _("Service.wipeServer() throws if it encounters a network problem.");
  let server = httpd_setup({});
  await setUpTestFixtures(server);

  Service.serverURL = "http://localhost:4352/";
  Service.clusterURL = "http://localhost:4352/";

  _("Try deletion.");
  try {
    Service.wipeServer();
    do_throw("Should have thrown!");
  } catch (ex) {
    do_check_eq(ex.result, Cr.NS_ERROR_CONNECTION_REFUSED);
  }

  Svc.Prefs.resetBranch("");
  await promiseStopServer(server);
});
