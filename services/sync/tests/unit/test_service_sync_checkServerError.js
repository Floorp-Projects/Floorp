Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/constants.js");

Cu.import("resource://services-sync/util.js");
Svc.DefaultPrefs.set("registerEngines", "");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/record.js");

initTestLogging();

function CatapultEngine() {
  SyncEngine.call(this, "Catapult");
}
CatapultEngine.prototype = {
  __proto__: SyncEngine.prototype,
  exception: null, // tests fill this in
  sync: function sync() {
    throw this.exception;
  }
};

function sync_httpd_setup() {
  let collectionsHelper = track_collections_helper();
  let upd = collectionsHelper.with_updated_collection;
  let collections = collectionsHelper.collections;

  let catapultEngine = Engines.get("catapult");
  let engines        = {catapult: {version: catapultEngine.version,
                                   syncID:  catapultEngine.syncID}}

  // Track these using the collections helper, which keeps modified times
  // up-to-date.
  let clientsColl = new ServerCollection({}, true);
  let keysWBO     = new ServerWBO("keys");
  let globalWBO   = new ServerWBO("global", {storageVersion: STORAGE_VERSION,
                                             syncID: Utils.makeGUID(),
                                             engines: engines});

  let handlers = {
    "/1.0/johndoe/info/collections":    collectionsHelper.handler,
    "/1.0/johndoe/storage/meta/global": upd("meta",    globalWBO.handler()),
    "/1.0/johndoe/storage/clients":     upd("clients", clientsColl.handler()),
    "/1.0/johndoe/storage/crypto/keys": upd("crypto",  keysWBO.handler())
  }
  return httpd_setup(handlers);
}

function setUp() {
  Service.username = "johndoe";
  Service.password = "ilovejane";
  Service.passphrase = "aabcdeabcdeabcdeabcdeabcde";
  Service.clusterURL = "http://localhost:8080/";
  new FakeCryptoService();
}

function generateAndUploadKeys() {
  CollectionKeys.generateNewKeys();
  let serverKeys = CollectionKeys.asWBO("crypto", "keys");
  serverKeys.encrypt(Weave.Service.syncKeyBundle);
  return serverKeys.upload("http://localhost:8080/1.0/johndoe/storage/crypto/keys").success;
}

function test_backoff500(next) {
  _("Test: HTTP 500 sets backoff status.");
  let server = sync_httpd_setup();
  setUp();

  let engine = Engines.get("catapult");
  engine.enabled = true;
  engine.exception = {status: 500};

  try {
    do_check_false(Status.enforceBackoff);

    // Forcibly create and upload keys here -- otherwise we don't get to the 500!
    do_check_true(generateAndUploadKeys());

    Service.login();
    Service.sync();
    do_check_true(Status.enforceBackoff);
  } finally {
    Status.resetBackoff();
    Service.startOver();
  }
  server.stop(next);
}

function test_backoff503(next) {
  _("Test: HTTP 503 with Retry-After header leads to backoff notification and sets backoff status.");
  let server = sync_httpd_setup();
  setUp();

  const BACKOFF = 42;
  let engine = Engines.get("catapult");
  engine.enabled = true;
  engine.exception = {status: 503,
                      headers: {"retry-after": BACKOFF}};

  let backoffInterval;
  Svc.Obs.add("weave:service:backoff:interval", function (subject) {
    backoffInterval = subject;
  });

  try {
    do_check_false(Status.enforceBackoff);

    do_check_true(generateAndUploadKeys());

    Service.login();
    Service.sync();

    do_check_true(Status.enforceBackoff);
    do_check_eq(backoffInterval, BACKOFF);
  } finally {
    Status.resetBackoff();
    Service.startOver();
  }
  server.stop(next);
}

function test_overQuota(next) {
  _("Test: HTTP 400 with body error code 14 means over quota.");
  let server = sync_httpd_setup();
  setUp();

  let engine = Engines.get("catapult");
  engine.enabled = true;
  engine.exception = {status: 400,
                      toString: function() "14"};

  try {
    do_check_eq(Status.sync, SYNC_SUCCEEDED);

    do_check_true(generateAndUploadKeys());

    Service.login();
    Service.sync();

    do_check_eq(Status.sync, OVER_QUOTA);
  } finally {
    Status.resetSync();
    Service.startOver();
  }
  server.stop(next);
}

function test_service_networkError(next) {
  _("Test: Connection refused error from Service.sync() leads to the right status code.");
  setUp();
  // Provoke connection refused.
  Service.clusterURL = "http://localhost:12345/";
  Service._ignorableErrorCount = 0;

  try {
    do_check_eq(Status.sync, SYNC_SUCCEEDED);

    Service._loggedIn = true;
    Service.sync();

    do_check_eq(Status.sync, LOGIN_FAILED_NETWORK_ERROR);
    do_check_eq(Service._ignorableErrorCount, 1);
  } finally {
    Status.resetSync();
    Service.startOver();
  }
  next();
}

function test_service_offline(next) {
  _("Test: Wanting to sync in offline mode leads to the right status code but does not increment the ignorable error count.");
  setUp();
  Svc.IO.offline = true;
  Service._ignorableErrorCount = 0;

  try {
    do_check_eq(Status.sync, SYNC_SUCCEEDED);

    Service._loggedIn = true;
    Service.sync();

    do_check_eq(Status.sync, LOGIN_FAILED_NETWORK_ERROR);
    do_check_eq(Service._ignorableErrorCount, 0);
  } finally {
    Status.resetSync();
    Service.startOver();
  }
  Svc.IO.offline = false;
  next();
}

function test_service_reset_ignorableErrorCount(next) {
  _("Test: Successful sync resets the ignorable error count.");
  let server = sync_httpd_setup();
  setUp();
  Service._ignorableErrorCount = 10;

  // Disable the engine so that sync completes.
  let engine = Engines.get("catapult");
  engine.enabled = false;

  try {
    do_check_eq(Status.sync, SYNC_SUCCEEDED);

    do_check_true(generateAndUploadKeys());

    Service.login();
    Service.sync();

    do_check_eq(Status.sync, SYNC_SUCCEEDED);
    do_check_eq(Service._ignorableErrorCount, 0);
  } finally {
    Status.resetSync();
    Service.startOver();
  }
  server.stop(next);
}

function test_engine_networkError(next) {
  _("Test: Network related exceptions from engine.sync() lead to the right status code.");
  let server = sync_httpd_setup();
  setUp();
  Service._ignorableErrorCount = 0;

  let engine = Engines.get("catapult");
  engine.enabled = true;
  engine.exception = Components.Exception("NS_ERROR_UNKNOWN_HOST",
                                          Cr.NS_ERROR_UNKNOWN_HOST);

  try {
    do_check_eq(Status.sync, SYNC_SUCCEEDED);

    do_check_true(generateAndUploadKeys());

    Service.login();
    Service.sync();

    do_check_eq(Status.sync, LOGIN_FAILED_NETWORK_ERROR);
    do_check_eq(Service._ignorableErrorCount, 1);
  } finally {
    Status.resetSync();
    Service.startOver();
  }
  server.stop(next);
}

// Slightly misplaced test as it doesn't actually test checkServerError,
// but the observer for "weave:engine:sync:apply-failed".
function test_engine_applyFailed(next) {
  let server = sync_httpd_setup();
  setUp();

  let engine = Engines.get("catapult");
  engine.enabled = true;
  delete engine.exception;
  engine.sync = function sync() {
    Svc.Obs.notify("weave:engine:sync:apply-failed", {}, "steam");
  };

  try {
    do_check_eq(Status.engines["steam"], undefined);

    do_check_true(generateAndUploadKeys());

    Service.login();
    Service.sync();

    do_check_eq(Status.engines["steam"], ENGINE_APPLY_FAIL);
  } finally {
    Status.resetSync();
    Service.startOver();
  }
  server.stop(next);
}

function run_test() {
  if (DISABLE_TESTS_BUG_604565)
    return;

  do_test_pending();

  // Register engine once.
  Engines.register(CatapultEngine);
  asyncChainTests(test_backoff500,
                  test_backoff503,
                  test_overQuota,
                  test_service_networkError,
                  test_service_offline,
                  test_service_reset_ignorableErrorCount,
                  test_engine_networkError,
                  test_engine_applyFailed,
                  do_test_finished)();
}
