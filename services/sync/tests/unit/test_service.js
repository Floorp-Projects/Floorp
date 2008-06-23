Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/crypto.js");

let __fakePrefs = {
  "log.logger.async" : "Debug",
  "username" : "foo",
  "serverURL" : "https://example.com/",
  "encryption" : "aes-256-cbc",
  "enabled" : true,
  "schedule" : 0
};

let __fakeDAVContents = {
  "meta/version" : "2",
  "private/privkey" : "fake private key"
};

let __fakePasswords = {
  'Mozilla Services Password': {foo: "bar"},
  'Mozilla Services Encryption Passphrase': {foo: "passphrase"}
};

let Service = loadInSandbox("resource://weave/service.js");

function TestService() {
  this.__superclassConstructor = Service.WeaveSvc;
  this.__superclassConstructor([]);
}

TestService.prototype = {
  _initLogs: function TS__initLogs() {
    this._log = Log4Moz.Service.getLogger("Service.Main");
  }
};
TestService.prototype.__proto__ = Service.WeaveSvc.prototype;

function test_login_works() {
  var fds = new FakeDAVService(__fakeDAVContents);
  var fprefs = new FakePrefService(__fakePrefs);
  var fpasses = new FakePasswordService(__fakePasswords);
  var fts = new FakeTimerService();
  var logStats = initTestLogging();
  var testService = new TestService();
  var finished = false;
  var successful = false;
  var onComplete = function(result) {
    finished = true;
    successful = result;
  };

  testService.login(onComplete);

  while (fts.processCallback()) {}

  do_check_true(finished);
  do_check_true(successful);
  do_check_eq(logStats.errorsLogged, 0);
  do_check_eq(Async.outstandingGenerators.length, 0);
}
