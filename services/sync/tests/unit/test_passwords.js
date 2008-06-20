Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/dav.js");
Cu.import("resource://weave/identity.js");

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

function run_test() {
  var fpasses = new FakePasswordService(__fakePasswords);
  var fprefs = new FakePrefService(__fakePrefs);
  var fds = new FakeDAVService({});
  var fts = new FakeTimerService();
  var logStats = initTestLogging();

  ID.set('Engine:PBE:default',
         new Identity('Mozilla Services Encryption Passphrase', 'foo'));

  // The JS module we're testing, with all members exposed.
  var passwords = loadInSandbox("resource://weave/engines/passwords.js");

  // Fake nsILoginInfo object.
  var fakeUser = {
    hostname: "www.boogle.com",
    formSubmitURL: "http://www.boogle.com/search",
    httpRealm: "",
    username: "",
    password: "",
    usernameField: "test_person",
    passwordField: "test_password"
    };

  Utils.getLoginManager = function fake_getLoginManager() {
    // Return a fake nsILoginManager object.
    return {getAllLogins: function() { return [fakeUser]; }};
  };

  Utils.getProfileFile = function fake_getProfileFile(arg) {
    return {exists: function() {return false;}};
  };

  Utils.open = function fake_open(file, mode) {
    let fakeStream = {
      writeString: function(data) {Log4Moz.Service.rootLogger.debug(data);},
      close: function() {}
    };
    return [fakeStream];
  };

  // Ensure that _hashLoginInfo() works.
  var fakeUserHash = passwords._hashLoginInfo(fakeUser);
  do_check_eq(typeof fakeUserHash, 'string');
  do_check_eq(fakeUserHash.length, 40);

  // Ensure that PasswordSyncCore._itemExists() works.
  var psc = new passwords.PasswordSyncCore();
  do_check_false(psc._itemExists("invalid guid"));
  do_check_true(psc._itemExists(fakeUserHash));

  // Make sure the engine can sync.
  var engine = new passwords.PasswordEngine();
  let calledBack = false;

  function cb() {
    calledBack = true;
  }

  engine.sync(cb);

  while (fts.processCallback()) {}

  do_check_true(calledBack);
  do_check_eq(logStats.errorsLogged, 0);
  do_check_eq(Async.outstandingGenerators, 0);
}
