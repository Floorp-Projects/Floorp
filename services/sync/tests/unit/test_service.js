Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/wrap.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/dav.js");
Cu.import("resource://weave/crypto.js");
Cu.import("resource://weave/identity.js");

Function.prototype.async = Async.sugar;

function makeFakeAsyncFunc(retval) {
  function fakeAsyncFunc() {
    let self = yield;

    Utils.makeTimerForCall(self.cb);
    yield;

    self.done(retval);
  }

  return fakeAsyncFunc;
}

function FakePrefs() {}

FakePrefs.prototype = {
  __contents: {"log.logger.async" : "Debug",
               "username" : "foo",
               "serverURL" : "https://example.com/",
               "encryption" : true,
               "enabled" : true,
               "schedule" : 0},
  _getPref: function fake__getPref(pref) {
    Log4Moz.Service.rootLogger.trace("Getting pref: " + pref);
    return this.__contents[pref];
  },
  getCharPref: function fake_getCharPref(pref) {
    return this._getPref(pref);
  },
  getBoolPref: function fake_getBoolPref(pref) {
    return this._getPref(pref);
  },
  getIntPref: function fake_getIntPref(pref) {
    return this._getPref(pref);
  },
  addObserver: function fake_addObserver() {}
};

Utils.__prefs = new FakePrefs();

Utils.findPassword = function fake_findPassword(realm, username) {
  let contents = {
    'Mozilla Services Password': {foo: "bar"},
    'Mozilla Services Encryption Passphrase': {foo: "passphrase"}
  };
  return contents[realm][username];
};

Crypto.__proto__ = {
  RSAkeydecrypt: function fake_RSAkeydecrypt(identity) {
    let self = yield;

    if (identity.password == "passphrase" &&
        identity.privkey == "fake private key")
      self.done("fake public key");
    else
      throw new Error("Unexpected identity information.");
  }
};

DAV.__proto__ = {
  checkLogin: makeFakeAsyncFunc(true),

  __contents: {"meta/version" : "2",
               "private/privkey" : "fake private key"},

  GET: function fake_GET(path, onComplete) {
    Log4Moz.Service.rootLogger.info("Retrieving " + path);
    var result = {status: 404};
    if (path in this.__contents)
      result = {status: 200, responseText: this.__contents[path]};

    return makeFakeAsyncFunc(result).async(this, onComplete);
  }
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
}
