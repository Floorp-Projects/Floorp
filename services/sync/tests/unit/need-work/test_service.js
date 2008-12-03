Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/crypto.js");

Function.prototype.async = Async.sugar;

let __fakePrefs = {
  "log.logger.async" : "Debug",
  "username" : "foo",
  "serverURL" : "https://example.com/",
  "encryption" : "aes-256-cbc",
  "enabled" : true,
  "schedule" : 0
};

let __fakeDAVContents = {
  "meta/version" : "3",
  "private/privkey" : '{"version":1,"algorithm":"RSA"}',
  "public/pubkey" : '{"version":1,"algorithm":"RSA"}'
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

Crypto.isPassphraseValid = function fake_isPassphraseValid(id) {
  let self = yield;

  do_check_eq(id.password, "passphrase");

  self.done(true);
};

function test_login_works() {
  var syncTesting = new SyncTestingInfrastructure();

  syncTesting.fakeDAVService.fakeContents = __fakeDAVContents;
  for (name in __fakePrefs)
    syncTesting.fakePrefService.fakeContents[name] = __fakePrefs[name];

  var testService = new TestService();

  function login(cb) {
    testService.login(cb);
  }

  syncTesting.runAsyncFunc("Logging in", login);
}
