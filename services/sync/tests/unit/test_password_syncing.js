Cu.import("resource://weave/engines/passwords.js");

load("fake_login_manager.js");

// ----------------------------------------
// Test Logic
// ----------------------------------------

function run_test() {
  var syncTesting = new SyncTestingInfrastructure(PasswordEngine);
  var fakeLoginManager = new FakeLoginManager(fakeSampleLogins);

  function freshEngineSync(cb) {
    let engine = new PasswordEngine();
    engine.sync(cb);
  };

  syncTesting.runAsyncFunc("initial sync", freshEngineSync);

  syncTesting.runAsyncFunc("trivial re-sync", freshEngineSync);

  fakeLoginManager.fakeLogins.push(
    {hostname: "www.yoogle.com",
     formSubmitURL: "http://www.yoogle.com/search",
     httpRealm: "",
     username: "",
     password: "",
     usernameField: "test_person2",
     passwordField: "test_password2"}
  );

  syncTesting.runAsyncFunc("add user and re-sync", freshEngineSync);

  fakeLoginManager.fakeLogins.pop();

  syncTesting.runAsyncFunc("remove user and re-sync", freshEngineSync);

  syncTesting.resetClientState();

  syncTesting.runAsyncFunc("resync on second computer", freshEngineSync);
}
