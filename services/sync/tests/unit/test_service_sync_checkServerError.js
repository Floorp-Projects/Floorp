Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/util.js");

Svc.DefaultPrefs.set("registerEngines", "");
Cu.import("resource://services-sync/service.js");

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
  handlers["/1.0/johndoe/storage/keys/pubkey"]
      = (new ServerWBO("pubkey")).handler();
  handlers["/1.0/johndoe/storage/keys/privkey"]
      = (new ServerWBO("privkey")).handler();
  handlers["/1.0/johndoe/storage/clients"]
      = (new ServerCollection()).handler();
  handlers["/1.0/johndoe/storage/crypto"]
      = (new ServerCollection()).handler();
  handlers["/1.0/johndoe/storage/crypto/clients"]
      = (new ServerWBO("clients", {})).handler();
  handlers["/1.0/johndoe/storage/meta/global"]
      = (new ServerWBO("global", {})).handler();
  return httpd_setup(handlers);
}

function setUp() {
  Service.username = "johndoe";
  Service.password = "ilovejane";
  Service.passphrase = "sekrit";
  Service.clusterURL = "http://localhost:8080/";
  new FakeCryptoService();
}

function test_backoff500() {
  _("Test: HTTP 500 sets backoff status.");
  let server = sync_httpd_setup();
  do_test_pending();
  setUp();

  Engines.register(CatapultEngine);
  let engine = Engines.get("catapult");
  engine.enabled = true;
  engine.exception = {status: 500};

  try {
    do_check_false(Status.enforceBackoff);
    Service.login();
    Service.sync();
    do_check_true(Status.enforceBackoff);
  } finally {
    server.stop(do_test_finished);
    Engines.unregister("catapult");
    Status.resetBackoff();
    Service.startOver();
  }
}

function test_backoff503() {
  _("Test: HTTP 503 with Retry-After header leads to backoff notification and sets backoff status.");
  let server = sync_httpd_setup();
  do_test_pending();
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
    server.stop(do_test_finished);
    Engines.unregister("catapult");
    Status.resetBackoff();
    Service.startOver();
  }
}

function test_overQuota() {
  _("Test: HTTP 400 with body error code 14 means over quota.");
  let server = sync_httpd_setup();
  do_test_pending();
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
    server.stop(do_test_finished);
    Engines.unregister("catapult");
    Status.resetSync();
    Service.startOver();
  }
}

function run_test() {
  if (DISABLE_TESTS_BUG_604565)
    return;

  test_backoff500();
  test_backoff503();
  test_overQuota();
}
