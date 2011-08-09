Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/engines.js");
var btoa;

let provider = {
  getFile: function(prop, persistent) {
    persistent.value = true;
    switch (prop) {
      case "ExtPrefDL":
        return [Services.dirsvc.get("CurProcD", Ci.nsIFile)];
      default:
        throw Cr.NS_ERROR_FAILURE;
    }
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDirectoryServiceProvider])
};
Services.dirsvc.QueryInterface(Ci.nsIDirectoryService).registerProvider(provider);

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
  log.ownAppenders = [appender];
  log.updateAppenders();

  return logStats;
}

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
 * Mock implementation of WeaveCrypto. It does not encrypt or
 * decrypt, merely returning the input verbatim.
 */
function FakeCryptoService() {
  this.counter = 0;

  delete Svc.Crypto;  // get rid of the getter first
  Svc.Crypto = this;
  Utils.sha256HMAC = this.sha256HMAC;

  CryptoWrapper.prototype.ciphertextHMAC = this.ciphertextHMAC;
}
FakeCryptoService.prototype = {

  sha256HMAC: function Utils_sha256HMAC(message, hasher) {
     message = message.substr(0, 64);
     while (message.length < 64) {
       message += " ";
     }
     return message;
  },

  ciphertextHMAC: function CryptoWrapper_ciphertextHMAC(keyBundle) {
    return Utils.sha256HMAC(this.ciphertext);
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
  }
};


function SyncTestingInfrastructure() {
  Cu.import("resource://services-sync/identity.js");

  ID.set('WeaveID',
         new Identity('Mozilla Services Encryption Passphrase', 'foo'));
  ID.set('WeaveCryptoID',
         new Identity('Mozilla Services Encryption Passphrase', 'foo'));

  this.logStats = initTestLogging();
  this.fakeFilesystem = new FakeFilesystemService({});
  this.fakeGUIDService = new FakeGUIDService();
  this.fakeCryptoService = new FakeCryptoService();
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

function generateNewKeys(collections) {
  let wbo = CollectionKeys.generateNewKeysWBO(collections);
  let modified = new_timestamp();
  CollectionKeys.setContents(wbo.cleartext, modified);
}

function do_check_throws(aFunc, aResult, aStack)
{
  if (!aStack) {
    try {
      // We might not have a 'Components' object.
      aStack = Components.stack.caller;
    } catch (e) {}
  }

  try {
    aFunc();
  } catch (e) {
    do_check_eq(e.result, aResult, aStack);
    return;
  }
  do_throw("Expected result " + aResult + ", none thrown.", aStack);
}

/*
 * A fake engine implementation.
 * This is used all over the place.
 * 
 * Complete with record, store, and tracker implementations.
 */

function RotaryRecord(collection, id) {
  CryptoWrapper.call(this, collection, id);
}
RotaryRecord.prototype = {
  __proto__: CryptoWrapper.prototype
};
Utils.deferGetSet(RotaryRecord, "cleartext", ["denomination"]);

function RotaryStore() {
  Store.call(this, "Rotary");
  this.items = {};
}
RotaryStore.prototype = {
  __proto__: Store.prototype,

  create: function Store_create(record) {
    this.items[record.id] = record.denomination;
  },

  remove: function Store_remove(record) {
    delete this.items[record.id];
  },

  update: function Store_update(record) {
    this.items[record.id] = record.denomination;
  },

  itemExists: function Store_itemExists(id) {
    return (id in this.items);
  },

  createRecord: function(id, collection) {
    let record = new RotaryRecord(collection, id);
    record.denomination = this.items[id] || "Data for new record: " + id;
    return record;
  },

  changeItemID: function(oldID, newID) {
    this.items[newID] = this.items[oldID];
    delete this.items[oldID];
  },

  getAllIDs: function() {
    let ids = {};
    for (let id in this.items) {
      ids[id] = true;
    }
    return ids;
  },

  wipe: function() {
    this.items = {};
  }
};

function RotaryTracker() {
  Tracker.call(this, "Rotary");
}
RotaryTracker.prototype = {
  __proto__: Tracker.prototype
};


function RotaryEngine() {
  SyncEngine.call(this, "Rotary");
  // Ensure that the engine starts with a clean slate.
  this.toFetch        = [];
  this.previousFailed = [];
}
RotaryEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _storeObj: RotaryStore,
  _trackerObj: RotaryTracker,
  _recordObj: RotaryRecord,

  _findDupe: function(item) {
    for (let [id, value] in Iterator(this._store.items)) {
      if (item.denomination == value) {
        return id;
      }
    }
  }
};
