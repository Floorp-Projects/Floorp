Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/dav.js");
Cu.import("resource://weave/identity.js");

// ----------------------------------------
// Fake Data
// ----------------------------------------

let __fakePasswords = {
  'Mozilla Services Password': {foo: "bar"},
  'Mozilla Services Encryption Passphrase': {foo: "passphrase"}
};

let __fakePrefs = {
  "encryption" : "none",
  "log.logger.service.crypto" : "Debug",
  "log.logger.service.engine" : "Debug",
  "log.logger.async" : "Debug"
};

let __fakeUsers = [
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
  ID.set('Engine:PBE:default',
         new Identity('Mozilla Services Encryption Passphrase', 'foo'));

  // The JS module we're testing, with all members exposed.
  var passwords = loadInSandbox("resource://weave/engines/passwords.js");

  // Ensure that _hashLoginInfo() works.
  var fakeUserHash = passwords._hashLoginInfo(__fakeUsers[0]);
  do_check_eq(typeof fakeUserHash, 'string');
  do_check_eq(fakeUserHash.length, 40);

  // Ensure that PasswordSyncCore._itemExists() works.
  var psc = new passwords.PasswordSyncCore();
  do_check_false(psc._itemExists("invalid guid"));
  do_check_true(psc._itemExists(fakeUserHash));

  // Make sure the engine can sync.
  function freshEngineSync(cb) {
    let engine = new passwords.PasswordEngine();
    engine.sync(cb);
  };

  runAndEnsureSuccess("initial sync", freshEngineSync);

  runAndEnsureSuccess("trivial re-sync", freshEngineSync);

  __fakeUsers.push({hostname: "www.yoogle.com",
                    formSubmitURL: "http://www.yoogle.com/search",
                    httpRealm: "",
                    username: "",
                    password: "",
                    usernameField: "test_person2",
                    passwordField: "test_password2"});

  runAndEnsureSuccess("add user and re-sync", freshEngineSync);
}

// ----------------------------------------
// Helper Functions
// ----------------------------------------

var callbackCalled = false;

function __makeCallback() {
  callbackCalled = false;
  return function callback() {
    callbackCalled = true;
  };
}

function runAndEnsureSuccess(name, func) {
  getTestLogger().info("Step '" + name + "' starting.");
  func(__makeCallback());
  while (fts.processCallback()) {}
  do_check_true(callbackCalled);
  for (name in Async.outstandingGenerators)
    getTestLogger().warn("Outstanding generator exists: " + name);
  do_check_eq(logStats.errorsLogged, 0);
  do_check_eq(Async.outstandingGenerators.length, 0);
  getTestLogger().info("Step '" + name + "' succeeded.");
}

// ----------------------------------------
// Fake Infrastructure
// ----------------------------------------

var fpasses = new FakePasswordService(__fakePasswords);
var fprefs = new FakePrefService(__fakePrefs);
var fds = new FakeDAVService({});
var fts = new FakeTimerService();
var logStats = initTestLogging();
var ffs = new FakeFilesystemService({});

Utils.makeGUID = function fake_makeGUID() {
  return "fake-guid";
};

Utils.getLoginManager = function fake_getLoginManager() {
  // Return a fake nsILoginManager object.
  return {getAllLogins: function() { return __fakeUsers; }};
};
