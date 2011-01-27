var btoa;

// initialize nss
let ch = Cc["@mozilla.org/security/hash;1"].
         createInstance(Ci.nsICryptoHash);

let ds = Cc["@mozilla.org/file/directory_service;1"]
  .getService(Ci.nsIProperties);

let provider = {
  getFile: function(prop, persistent) {
    persistent.value = true;
    switch (prop) {
      case "ExtPrefDL":
        return [ds.get("CurProcD", Ci.nsIFile)];
      case "ProfD":
        return ds.get("CurProcD", Ci.nsIFile);
      case "UHist":
        let histFile = ds.get("CurProcD", Ci.nsIFile);
        histFile.append("history.dat");
        return histFile;
      default:
        throw Cr.NS_ERROR_FAILURE;
    }
  },
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIDirectoryServiceProvider) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};
ds.QueryInterface(Ci.nsIDirectoryService).registerProvider(provider);

function loadInSandbox(aUri) {
  var sandbox = Components.utils.Sandbox(this);
  var request = Components.
                classes["@mozilla.org/xmlextras/xmlhttprequest;1"].
                createInstance();

  request.open("GET", aUri, false);
  request.overrideMimeType("application/javascript");
  request.send(null);
  Components.utils.evalInSandbox(request.responseText, sandbox, "1.8");

  return sandbox;
}

function FakeTimerService() {
  Cu.import("resource://services-sync/util.js");

  this.callbackQueue = [];

  var self = this;

  this.__proto__ = {
    makeTimerForCall: function FTS_makeTimerForCall(cb) {
      // Just add the callback to our queue and we'll call it later, so
      // as to simulate a real nsITimer.
      self.callbackQueue.push(cb);
      return "fake nsITimer";
    },
    processCallback: function FTS_processCallbacks() {
      var cb = self.callbackQueue.pop();
      if (cb) {
        cb();
        return true;
      }
      return false;
    }
  };

  Utils.makeTimerForCall = self.makeTimerForCall;
};

btoa = Cu.import("resource://services-sync/log4moz.js").btoa;
function getTestLogger(component) {
  return Log4Moz.repository.getLogger("Testing");
}

function initTestLogging(level) {
  function LogStats() {
    this.errorsLogged = 0;
  }
  LogStats.prototype = {
    format: function BF_format(message) {
      if (message.level == Log4Moz.Level.Error)
        this.errorsLogged += 1;
      return message.loggerName + "\t" + message.levelDesc + "\t" +
        message.message + "\n";
    }
  };
  LogStats.prototype.__proto__ = new Log4Moz.Formatter();

  var log = Log4Moz.repository.rootLogger;
  var logStats = new LogStats();
  var appender = new Log4Moz.DumpAppender(logStats);

  if (typeof(level) == "undefined")
    level = "Debug";
  getTestLogger().level = Log4Moz.Level[level];

  log.level = Log4Moz.Level.Trace;
  appender.level = Log4Moz.Level.Trace;
  // Overwrite any other appenders (e.g. from previous incarnations)
  log._appenders = [appender];

  return logStats;
}

function FakePrefService(contents) {
  Cu.import("resource://services-sync/util.js");
  this.fakeContents = contents;
  Utils.__prefs = this;
}

FakePrefService.prototype = {
  _getPref: function fake__getPref(pref) {
    getTestLogger().trace("Getting pref: " + pref);
    return this.fakeContents[pref];
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

function FakePasswordService(contents) {
  Cu.import("resource://services-sync/util.js");

  this.fakeContents = contents;
  let self = this;

  Utils.findPassword = function fake_findPassword(realm, username) {
    getTestLogger().trace("Password requested for " +
                          realm + ":" + username);
    if (realm in self.fakeContents && username in self.fakeContents[realm])
      return self.fakeContents[realm][username];
    else
      return null;
  };
};

function FakeFilesystemService(contents) {
  this.fakeContents = contents;
  let self = this;

  Utils.jsonSave = function jsonSave(filePath, that, obj, callback) {
    let json = typeof obj == "function" ? obj.call(that) : obj;
    self.fakeContents["weave/" + filePath + ".json"] = JSON.stringify(json);
    callback.call(that);
  };

  Utils.jsonLoad = function jsonLoad(filePath, that, callback) {
    let obj;
    let json = self.fakeContents["weave/" + filePath + ".json"];
    if (json) {
      obj = JSON.parse(json);
    }
    callback.call(that, obj);
  };
};

function FakeGUIDService() {
  let latestGUID = 0;

  Utils.makeGUID = function fake_makeGUID() {
    return "fake-guid-" + latestGUID++;
  };
}


/*
 * Mock implementation of IWeaveCrypto.  It does not encrypt or
 * decrypt, just returns the input verbatimly.
 */
function FakeCryptoService() {
  this.counter = 0;

  delete Svc.Crypto;  // get rid of the getter first
  Svc.Crypto = this;
  Utils.sha256HMAC = this.sha256HMAC;
}
FakeCryptoService.prototype = {

  sha256HMAC: function(message, key) {
     message = message.substr(0, 64);
     while (message.length < 64) {
       message += " ";
     }
     return message;
  },

  encrypt: function(aClearText, aSymmetricKey, aIV) {
    return aClearText;
  },

  decrypt: function(aCipherText, aSymmetricKey, aIV) {
    return aCipherText;
  },

  generateRandomKey: function() {
    return btoa("fake-symmetric-key-" + this.counter++);
  },

  generateRandomIV: function() {
    // A base64-encoded IV is 24 characters long
    return btoa("fake-fake-fake-random-iv");
  },

  expandData : function expandData(data, len) {
    return data;
  },

  deriveKeyFromPassphrase : function (passphrase, salt, keyLength) {
    return "some derived key string composed of bytes";
  },

  generateRandomBytes: function(aByteCount) {
    return "not-so-random-now-are-we-HA-HA-HA! >:)".slice(aByteCount);
  },
};


function SyncTestingInfrastructure(engineFactory) {
  let __fakePasswords = {
    'Mozilla Services Password': {foo: "bar"},
    'Mozilla Services Encryption Passphrase': {foo: "a-abcde-abcde-abcde-abcde-abcde"}
  };

  let __fakePrefs = {
    "encryption" : "none",
    "log.logger.service.crypto" : "Debug",
    "log.logger.service.engine" : "Debug",
    "log.logger.async" : "Debug",
    "xmpp.enabled" : false
  };

  Cu.import("resource://services-sync/identity.js");
  Cu.import("resource://services-sync/util.js");

  ID.set('WeaveID',
         new Identity('Mozilla Services Encryption Passphrase', 'foo'));
  ID.set('WeaveCryptoID',
         new Identity('Mozilla Services Encryption Passphrase', 'foo'));

  this.fakePasswordService = new FakePasswordService(__fakePasswords);
  this.fakePrefService = new FakePrefService(__fakePrefs);
  this.fakeTimerService = new FakeTimerService();
  this.logStats = initTestLogging();
  this.fakeFilesystem = new FakeFilesystemService({});
  this.fakeGUIDService = new FakeGUIDService();
  this.fakeCryptoService = new FakeCryptoService();

  this._logger = getTestLogger();
  this._engineFactory = engineFactory;
  this._clientStates = [];

  this.saveClientState = function pushClientState(label) {
    let state = Utils.deepCopy(this.fakeFilesystem.fakeContents);
    let currContents = this.fakeFilesystem.fakeContents;
    this.fakeFilesystem.fakeContents = [];
    let engine = this._engineFactory();
    let snapshot = Utils.deepCopy(engine._store.wrap());
    this._clientStates[label] = {state: state, snapshot: snapshot};
    this.fakeFilesystem.fakeContents = currContents;
  };

  this.restoreClientState = function restoreClientState(label) {
    let state = this._clientStates[label].state;
    let snapshot = this._clientStates[label].snapshot;

    function _restoreState() {
      let self = yield;

      this.fakeFilesystem.fakeContents = [];
      let engine = this._engineFactory();
      engine._store.wipe();
      let originalSnapshot = Utils.deepCopy(engine._store.wrap());

      engine._core.detectUpdates(self.cb, originalSnapshot, snapshot);
      let commands = yield;

      engine._store.applyCommands.async(engine._store, self.cb, commands);
      yield;

      this.fakeFilesystem.fakeContents = Utils.deepCopy(state);
    }

    let self = this;

    function restoreState(cb) {
      _restoreState.async(self, cb);
    }

    this.runAsyncFunc("restore client state of " + label,
                      restoreState);
  };

  this.__makeCallback = function __makeCallback() {
    this.__callbackCalled = false;
    let self = this;
    return function callback() {
      self.__callbackCalled = true;
    };
  };

  this.doSync = function doSync(name) {
    let self = this;

    function freshEngineSync(cb) {
      let engine = self._engineFactory();
      engine.sync(cb);
    }

    this.runAsyncFunc(name, freshEngineSync);
  };

  this.runAsyncFunc = function runAsyncFunc(name, func) {
    let logger = this._logger;

    logger.info("-----------------------------------------");
    logger.info("Step '" + name + "' starting.");
    logger.info("-----------------------------------------");
    func(this.__makeCallback());
    while (this.fakeTimerService.processCallback()) {}
    do_check_true(this.__callbackCalled);
    for (name in Async.outstandingGenerators)
      logger.warn("Outstanding generator exists: " + name);
    do_check_eq(this.logStats.errorsLogged, 0);
    do_check_eq(Async.outstandingGenerators.length, 0);
    logger.info("Step '" + name + "' succeeded.");
  };

  this.resetClientState = function resetClientState() {
    this.fakeFilesystem.fakeContents = {};
    let engine = this._engineFactory();
    engine._store.wipe();
  };
}


/*
 * Ensure exceptions from inside callbacks leads to test failures.
 */
function ensureThrows(func) {
  return function() {
    try {
      func.apply(this, arguments);
    } catch (ex) {
      do_throw(ex);
    }
  };
}


/**
 * Print some debug message to the console. All arguments will be printed,
 * separated by spaces.
 *
 * @param [arg0, arg1, arg2, ...]
 *        Any number of arguments to print out
 * @usage _("Hello World") -> prints "Hello World"
 * @usage _(1, 2, 3) -> prints "1 2 3"
 */
let _ = function(some, debug, text, to) print(Array.slice(arguments).join(" "));

_("Setting the identity for passphrase");
Cu.import("resource://services-sync/identity.js");


/*
 * Test setup helpers.
 */

// Turn WBO cleartext into "encrypted" payload as it goes over the wire
function encryptPayload(cleartext) {
  if (typeof cleartext == "object") {
    cleartext = JSON.stringify(cleartext);
  }

  return {ciphertext: cleartext, // ciphertext == cleartext with fake crypto
          IV: "irrelevant",
          hmac: Utils.sha256HMAC(cleartext, Utils.makeHMACKey(""))};
}

