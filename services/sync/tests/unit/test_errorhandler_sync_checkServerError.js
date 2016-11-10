/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/policies.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/fakeservices.js");
Cu.import("resource://testing-common/services/sync/utils.js");

initTestLogging("Trace");

var engineManager = Service.engineManager;
engineManager.clear();

function CatapultEngine() {
  SyncEngine.call(this, "Catapult", Service);
}
CatapultEngine.prototype = {
  __proto__: SyncEngine.prototype,
  exception: null, // tests fill this in
  _sync: function _sync() {
    throw this.exception;
  }
};

function sync_httpd_setup() {
  let collectionsHelper = track_collections_helper();
  let upd = collectionsHelper.with_updated_collection;
  let collections = collectionsHelper.collections;

  let catapultEngine = engineManager.get("catapult");
  let engines        = {catapult: {version: catapultEngine.version,
                                   syncID:  catapultEngine.syncID}};

  // Track these using the collections helper, which keeps modified times
  // up-to-date.
  let clientsColl = new ServerCollection({}, true);
  let keysWBO     = new ServerWBO("keys");
  let globalWBO   = new ServerWBO("global", {storageVersion: STORAGE_VERSION,
                                             syncID: Utils.makeGUID(),
                                             engines: engines});

  let handlers = {
    "/1.1/johndoe/info/collections":    collectionsHelper.handler,
    "/1.1/johndoe/storage/meta/global": upd("meta",    globalWBO.handler()),
    "/1.1/johndoe/storage/clients":     upd("clients", clientsColl.handler()),
    "/1.1/johndoe/storage/crypto/keys": upd("crypto",  keysWBO.handler())
  };
  return httpd_setup(handlers);
}

async function setUp(server) {
  await configureIdentity({username: "johndoe"});
  Service.serverURL = server.baseURI + "/";
  Service.clusterURL = server.baseURI + "/";
  new FakeCryptoService();
}

function generateAndUploadKeys(server) {
  generateNewKeys(Service.collectionKeys);
  let serverKeys = Service.collectionKeys.asWBO("crypto", "keys");
  serverKeys.encrypt(Service.identity.syncKeyBundle);
  let res = Service.resource(server.baseURI + "/1.1/johndoe/storage/crypto/keys");
  return serverKeys.upload(res).success;
}


add_identity_test(this, async function test_backoff500() {
  _("Test: HTTP 500 sets backoff status.");
  let server = sync_httpd_setup();
  await setUp(server);

  let engine = engineManager.get("catapult");
  engine.enabled = true;
  engine.exception = {status: 500};

  try {
    do_check_false(Status.enforceBackoff);

    // Forcibly create and upload keys here -- otherwise we don't get to the 500!
    do_check_true(generateAndUploadKeys(server));

    Service.login();
    Service.sync();
    do_check_true(Status.enforceBackoff);
    do_check_eq(Status.sync, SYNC_SUCCEEDED);
    do_check_eq(Status.service, SYNC_FAILED_PARTIAL);
  } finally {
    Status.resetBackoff();
    Service.startOver();
  }
  await promiseStopServer(server);
});

add_identity_test(this, async function test_backoff503() {
  _("Test: HTTP 503 with Retry-After header leads to backoff notification and sets backoff status.");
  let server = sync_httpd_setup();
  await setUp(server);

  const BACKOFF = 42;
  let engine = engineManager.get("catapult");
  engine.enabled = true;
  engine.exception = {status: 503,
                      headers: {"retry-after": BACKOFF}};

  let backoffInterval;
  Svc.Obs.add("weave:service:backoff:interval", function (subject) {
    backoffInterval = subject;
  });

  try {
    do_check_false(Status.enforceBackoff);

    do_check_true(generateAndUploadKeys(server));

    Service.login();
    Service.sync();

    do_check_true(Status.enforceBackoff);
    do_check_eq(backoffInterval, BACKOFF);
    do_check_eq(Status.service, SYNC_FAILED_PARTIAL);
    do_check_eq(Status.sync, SERVER_MAINTENANCE);
  } finally {
    Status.resetBackoff();
    Status.resetSync();
    Service.startOver();
  }
  await promiseStopServer(server);
});

add_identity_test(this, async function test_overQuota() {
  _("Test: HTTP 400 with body error code 14 means over quota.");
  let server = sync_httpd_setup();
  await setUp(server);

  let engine = engineManager.get("catapult");
  engine.enabled = true;
  engine.exception = {status: 400,
                      toString() {
                        return "14";
                      }};

  try {
    do_check_eq(Status.sync, SYNC_SUCCEEDED);

    do_check_true(generateAndUploadKeys(server));

    Service.login();
    Service.sync();

    do_check_eq(Status.sync, OVER_QUOTA);
    do_check_eq(Status.service, SYNC_FAILED_PARTIAL);
  } finally {
    Status.resetSync();
    Service.startOver();
  }
  await promiseStopServer(server);
});

add_identity_test(this, async function test_service_networkError() {
  _("Test: Connection refused error from Service.sync() leads to the right status code.");
  let server = sync_httpd_setup();
  await setUp(server);
  await promiseStopServer(server);
  // Provoke connection refused.
  Service.clusterURL = "http://localhost:12345/";

  try {
    do_check_eq(Status.sync, SYNC_SUCCEEDED);

    Service._loggedIn = true;
    Service.sync();

    do_check_eq(Status.sync, LOGIN_FAILED_NETWORK_ERROR);
    do_check_eq(Status.service, SYNC_FAILED);
  } finally {
    Status.resetSync();
    Service.startOver();
  }
});

add_identity_test(this, async function test_service_offline() {
  _("Test: Wanting to sync in offline mode leads to the right status code but does not increment the ignorable error count.");
  let server = sync_httpd_setup();
  await setUp(server);

  await promiseStopServer(server);
  Services.io.offline = true;
  Services.prefs.setBoolPref("network.dns.offline-localhost", false);

  try {
    do_check_eq(Status.sync, SYNC_SUCCEEDED);

    Service._loggedIn = true;
    Service.sync();

    do_check_eq(Status.sync, LOGIN_FAILED_NETWORK_ERROR);
    do_check_eq(Status.service, SYNC_FAILED);
  } finally {
    Status.resetSync();
    Service.startOver();
  }
  Services.io.offline = false;
  Services.prefs.clearUserPref("network.dns.offline-localhost");
});

add_identity_test(this, async function test_engine_networkError() {
  _("Test: Network related exceptions from engine.sync() lead to the right status code.");
  let server = sync_httpd_setup();
  await setUp(server);

  let engine = engineManager.get("catapult");
  engine.enabled = true;
  engine.exception = Components.Exception("NS_ERROR_UNKNOWN_HOST",
                                          Cr.NS_ERROR_UNKNOWN_HOST);

  try {
    do_check_eq(Status.sync, SYNC_SUCCEEDED);

    do_check_true(generateAndUploadKeys(server));

    Service.login();
    Service.sync();

    do_check_eq(Status.sync, LOGIN_FAILED_NETWORK_ERROR);
    do_check_eq(Status.service, SYNC_FAILED_PARTIAL);
  } finally {
    Status.resetSync();
    Service.startOver();
  }
  await promiseStopServer(server);
});

add_identity_test(this, async function test_resource_timeout() {
  let server = sync_httpd_setup();
  await setUp(server);

  let engine = engineManager.get("catapult");
  engine.enabled = true;
  // Resource throws this when it encounters a timeout.
  engine.exception = Components.Exception("Aborting due to channel inactivity.",
                                          Cr.NS_ERROR_NET_TIMEOUT);

  try {
    do_check_eq(Status.sync, SYNC_SUCCEEDED);

    do_check_true(generateAndUploadKeys(server));

    Service.login();
    Service.sync();

    do_check_eq(Status.sync, LOGIN_FAILED_NETWORK_ERROR);
    do_check_eq(Status.service, SYNC_FAILED_PARTIAL);
  } finally {
    Status.resetSync();
    Service.startOver();
  }
  await promiseStopServer(server);
});

function run_test() {
  validate_all_future_pings();
  engineManager.register(CatapultEngine);
  run_next_test();
}
