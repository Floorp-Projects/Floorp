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
  let handlers = {};
  handlers["/1.0/johndoe/info/collections"]
      = (new ServerWBO("collections", {})).handler(),
  handlers["/1.0/johndoe/storage/clients"]
      = (new ServerCollection()).handler();
  handlers["/1.0/johndoe/storage/crypto"]
      = (new ServerCollection()).handler();
  handlers["/1.0/johndoe/storage/crypto/keys"]
      = (new ServerWBO("keys", {})).handler();
  handlers["/1.0/johndoe/storage/crypto/clients"]
      = (new ServerWBO("clients", {})).handler();
  handlers["/1.0/johndoe/storage/meta/global"]
      = (new ServerWBO("global", {})).handler();
  return httpd_setup(handlers);
}

function setUp() {
  Service.username = "johndoe";
  Service.password = "ilovejane";
  Service.passphrase = "aabcdeabcdeabcdeabcdeabcde";
  Service.clusterURL = "http://localhost:8080/";
  new FakeCryptoService();
}

function test_backoff500(next) {
  _("Test: HTTP 500 sets backoff status.");
  let server = sync_httpd_setup();
  setUp();

  Engines.register(CatapultEngine);
  let engine = Engines.get("catapult");
  engine.enabled = true;
  engine.exception = {status: 500};

  try {
    do_check_false(Status.enforceBackoff);
    
    // Forcibly create and upload keys here -- otherwise we don't get to the 500!
    CollectionKeys.generateNewKeys();
    do_check_true(CollectionKeys.asWBO().upload("http://localhost:8080/1.0/johndoe/storage/crypto/keys").success);
    
    Service.login();
    Service.sync();
    do_check_true(Status.enforceBackoff);
  } finally {
    Engines.unregister("catapult");
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
  Engines.register(CatapultEngine);
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

    Service.login();
    Service.sync();

    do_check_true(Status.enforceBackoff);
    do_check_eq(backoffInterval, BACKOFF);
  } finally {
    Engines.unregister("catapult");
    Status.resetBackoff();
    Service.startOver();
  }
  server.stop(next);
}

function test_overQuota(next) {
  _("Test: HTTP 400 with body error code 14 means over quota.");
  let server = sync_httpd_setup();
  setUp();

  Engines.register(CatapultEngine);
  let engine = Engines.get("catapult");
  engine.enabled = true;
  engine.exception = {status: 400,
                      toString: function() "14"};

  try {
    do_check_eq(Status.sync, SYNC_SUCCEEDED);

    Service.login();
    Service.sync();

    do_check_eq(Status.sync, OVER_QUOTA);
  } finally {
    Engines.unregister("catapult");
    Status.resetSync();
    Service.startOver();
  }
  server.stop(next);
}

function test_service_networkError(next) {
  setUp();
  // Provoke connection refused.
  Service.clusterURL = "http://localhost:12345/";
  try {
    do_check_eq(Status.sync, SYNC_SUCCEEDED);

    Service._loggedIn = true;
    Service.sync();

    do_check_eq(Status.sync, LOGIN_FAILED_NETWORK_ERROR);
  } finally {
    Status.resetSync();
    Service.startOver();
  }
  next();
}

function test_engine_networkError(next) {
  _("Test: Network related exceptions from engine.sync() lead to the right status code.");
  let server = sync_httpd_setup();
  setUp();

  Engines.register(CatapultEngine);
  let engine = Engines.get("catapult");
  engine.enabled = true;
  engine.exception = Components.Exception("NS_ERROR_UNKNOWN_HOST",
                                          Cr.NS_ERROR_UNKNOWN_HOST);

  try {
    do_check_eq(Status.sync, SYNC_SUCCEEDED);

    Service.login();
    Service.sync();

    do_check_eq(Status.sync, LOGIN_FAILED_NETWORK_ERROR);
  } finally {
    Engines.unregister("catapult");
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

  Engines.register(CatapultEngine);
  let engine = Engines.get("catapult");
  engine.enabled = true;
  engine.sync = function sync() {
    Svc.Obs.notify("weave:engine:sync:apply-failed", {}, "steam");
  };

  try {
    do_check_eq(Status.engines["steam"], undefined);

    Service.login();
    Service.sync();

    do_check_eq(Status.engines["steam"], ENGINE_APPLY_FAIL);
  } finally {
    Engines.unregister("catapult");
    Status.resetSync();
    Service.startOver();
  }
  server.stop(next);
}

function run_test() {
  if (DISABLE_TESTS_BUG_604565)
    return;

  do_test_pending();
  asyncChainTests(test_backoff500,
                  test_backoff503,
                  test_overQuota,
                  test_service_networkError,
                  test_engine_networkError,
                  test_engine_applyFailed,
                  do_test_finished)();
}
