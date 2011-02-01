Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/passwords.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/service.js");

Engines.register(PasswordEngine);

function makeEngine() {
  return new PasswordEngine();
}
var syncTesting = new SyncTestingInfrastructure(makeEngine);

function run_test() {
  initTestLogging("Trace");
  Log4Moz.repository.getLogger("Engine.Passwords").level = Log4Moz.Level.Trace;
  Log4Moz.repository.getLogger("Store.Passwords").level = Log4Moz.Level.Trace;
  do_test_pending();

  CollectionKeys.generateNewKeys();

  // Verify that password entries with redundant fields don't cause an exception.
  
  const BOGUS_GUID_A = "zzzzzzzzzzzz";
  const BOGUS_GUID_B = "yyyyyyyyyyyy";
  let payloadA = {id: BOGUS_GUID_A,
                  hostname: "http://foo.bar.com",
                  formSubmitURL: "http://foo.bar.com/baz",
                  httpRealm: "secure",
                  username: "john",
                  password: "smith",
                  usernameField: "username",
                  passwordField: "password"};
  let payloadB = {id: BOGUS_GUID_B,
                  hostname: "http://foo.baz.com",
                  formSubmitURL: "http://foo.baz.com/baz",
                  username: "john",
                  password: "smith",
                  usernameField: "username",
                  passwordField: "password"};
  let badWBO  = new ServerWBO(BOGUS_GUID_A, encryptPayload(payloadA));
  let goodWBO = new ServerWBO(BOGUS_GUID_B, encryptPayload(payloadB));
  
  Svc.Prefs.set("clusterURL", "http://localhost:8080/");
  Svc.Prefs.set("username", "foo");

  let engine = new PasswordEngine();
  let store = engine._store;
  let global = new ServerWBO("global",
                             {engines: {passwords: {version: engine.version,
                                                    syncID: engine.syncID}}});

  let collection = new ServerCollection({BOGUS_GUID_A: badWBO,
                                         BOGUS_GUID_B: goodWBO});
  let server = httpd_setup({
    "/1.0/foo/storage/meta/global": global.handler(),
    "/1.0/foo/storage/passwords": collection.handler()
  });
  
  try {
    let err;
    try {
      engine.sync();
    } catch (ex) {
      err = ex;
    }
    // No exception thrown when encountering bad record.
    do_check_true(!err);
    
    // Only the good record makes it to Svc.Login.
    let badCount = {};
    let goodCount = {};
    let badLogins = Svc.Login.findLogins(badCount, payloadA.hostname, payloadA.formSubmitURL, payloadA.httpRealm);
    let goodLogins = Svc.Login.findLogins(goodCount, payloadB.hostname, payloadB.formSubmitURL, null);
    
    _("Bad: " + JSON.stringify(badLogins));
    _("Good: " + JSON.stringify(goodLogins));
    _("Count: " + badCount.value + ", " + goodCount.value);
    
    do_check_eq(goodCount.value, 1);
    do_check_eq(badCount.value, 0);
    
    do_check_true(!!store.getAllIDs()[BOGUS_GUID_B]);
    do_check_true(!store.getAllIDs()[BOGUS_GUID_A]);
  }
  finally {
    store.wipe();
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
    Records.clearCache();
  }
}
