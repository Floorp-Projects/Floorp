Cu.import("resource://weave/engines/passwords.js");

load("fake_login_manager.js");

// ----------------------------------------
// Test Logic
// ----------------------------------------

function run_test() {
  function passwdFactory() { return new PasswordEngine(); }
  var syncTesting = new SyncTestingInfrastructure(passwdFactory);
  var fakeLoginManager = new FakeLoginManager(fakeSampleLogins);

  syncTesting.doSync("initial sync");

  syncTesting.doSync("trivial re-sync");

  fakeLoginManager.fakeLogins.push(
    {hostname: "www.yoogle.com",
     formSubmitURL: "http://www.yoogle.com/search",
     httpRealm: "",
     username: "",
     password: "",
     usernameField: "test_person2",
     passwordField: "test_password2"}
  );

  syncTesting.doSync("add user and re-sync");

  fakeLoginManager.fakeLogins.pop();

  syncTesting.doSync("remove user and re-sync");

  syncTesting.resetClientState();

  syncTesting.doSync("resync on second computer");
}
