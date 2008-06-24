Cu.import("resource://weave/util.js");

// ----------------------------------------
// Fake Data
// ----------------------------------------

let __fakeLogins = [
  // Fake nsILoginInfo object.
  {hostname: "www.boogle.com",
   formSubmitURL: "http://www.boogle.com/search",
   httpRealm: "",
   username: "",
   password: "",
   usernameField: "test_person",
   passwordField: "test_password"}
];

// ----------------------------------------
// Test Logic
// ----------------------------------------

function run_test() {
  // The JS module we're testing, with all members exposed.
  var passwords = loadInSandbox("resource://weave/engines/passwords.js");

  // Ensure that _hashLoginInfo() works.
  var fakeUserHash = passwords._hashLoginInfo(__fakeLogins[0]);
  do_check_eq(typeof fakeUserHash, 'string');
  do_check_eq(fakeUserHash.length, 40);

  // Ensure that PasswordSyncCore._itemExists() works.
  var psc = new passwords.PasswordSyncCore();
  psc.__loginManager = {getAllLogins: function() { return __fakeLogins; }};
  do_check_false(psc._itemExists("invalid guid"));
  do_check_true(psc._itemExists(fakeUserHash));

  // Make sure the engine can sync.
  var syncTesting = new SyncTestingInfrastructure();
  var fakeLoginManager = new FakeLoginManager(__fakeLogins);

  function freshEngineSync(cb) {
    let engine = new passwords.PasswordEngine();
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

  syncTesting.fakeFilesystem.fakeContents = {};
  fakeLoginManager.fakeLogins = [];

  syncTesting.runAsyncFunc("resync on second computer", freshEngineSync);
}

// ----------------------------------------
// Fake Infrastructure
// ----------------------------------------

function FakeLoginManager(fakeLogins) {
  this.fakeLogins = fakeLogins;

  let self = this;

  Utils.getLoginManager = function fake_getLoginManager() {
    // Return a fake nsILoginManager object.
    return {
      getAllLogins: function() { return self.fakeLogins; },
      addLogin: function(login) {
        getTestLogger().info("nsILoginManager.addLogin() called " +
                             "with hostname '" + login.hostname + "'.");
        self.fakeLogins.push(login);
      }
    };
  };
}
