/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/async.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/engines.js");
let btoa;
let atob;

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

let timer;
function waitForZeroTimer(callback) {
  // First wait >100ms (nsITimers can take up to that much time to fire, so
  // we can account for the timer in delayedAutoconnect) and then two event
  // loop ticks (to account for the Utils.nextTick() in autoConnect).
  let ticks = 2;
  function wait() {
    if (ticks) {
      ticks -= 1;
      Utils.nextTick(wait);
      return;
    }
    callback();
  }
  timer = Utils.namedTimer(wait, 150, {}, "timer");
}

btoa = Cu.import("resource://services-sync/log4moz.js").btoa;
atob = Cu.import("resource://services-sync/log4moz.js").atob;
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

// This is needed for loadAddonTestFunctions().
let gGlobalScope = this;

function ExtensionsTestPath(path) {
  if (path[0] != "/") {
    throw Error("Path must begin with '/': " + path);
  }

  return "../../../../toolkit/mozapps/extensions/test/xpcshell" + path;
}

/**
 * Loads the AddonManager test functions by importing its test file.
 *
 * This should be called in the global scope of any test file needing to
 * interface with the AddonManager. It should only be called once, or the
 * universe will end.
 */
function loadAddonTestFunctions() {
  const path = ExtensionsTestPath("/head_addons.js");
  let file = do_get_file(path);
  let uri = Services.io.newFileURI(file);
  Services.scriptloader.loadSubScript(uri.spec, gGlobalScope);
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
}

function getAddonInstall(name) {
  let f = do_get_file(ExtensionsTestPath("/addons/" + name + ".xpi"));
  let cb = Async.makeSyncCallback();
  AddonManager.getInstallForFile(f, cb);

  return Async.waitForSyncCallback(cb);
}

/**
 * Obtains an addon from the add-on manager by id.
 *
 * This is merely a synchronous wrapper.
 *
 * @param  id
 *         ID of add-on to fetch
 * @return addon object on success or undefined or null on failure
 */
function getAddonFromAddonManagerByID(id) {
   let cb = Async.makeSyncCallback();
   AddonManager.getAddonByID(id, cb);
   return Async.waitForSyncCallback(cb);
}

/**
 * Installs an add-on synchronously from an addonInstall
 *
 * @param  install addonInstall instance to install
 */
function installAddonFromInstall(install) {
  let cb = Async.makeSyncCallback();
  let listener = {onInstallEnded: cb};
  AddonManager.addInstallListener(listener);
  install.install();
  Async.waitForSyncCallback(cb);
  AddonManager.removeAddonListener(listener);

  do_check_neq(null, install.addon);
  do_check_neq(null, install.addon.syncGUID);

  return install.addon;
}

/**
 * Convenience function to install an add-on from the extensions unit tests.
 *
 * @param  name
 *         String name of add-on to install. e.g. test_install1
 * @return addon object that was installed
 */
function installAddon(name) {
  let install = getAddonInstall(name);
  do_check_neq(null, install);
  return installAddonFromInstall(install);
}

/**
 * Convenience function to uninstall an add-on synchronously.
 *
 * @param addon
 *        Addon instance to uninstall
 */
function uninstallAddon(addon) {
  let cb = Async.makeSyncCallback();
  let listener = {onUninstalled: function(uninstalled) {
    if (uninstalled.id == addon.id) {
      AddonManager.removeAddonListener(listener);
      cb(uninstalled);
    }
  }};

  AddonManager.addAddonListener(listener);
  addon.uninstall();
  Async.waitForSyncCallback(cb);
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


function fakeSHA256HMAC(message) {
   message = message.substr(0, 64);
   while (message.length < 64) {
     message += " ";
   }
   return message;
}

/*
 * Mock implementation of WeaveCrypto. It does not encrypt or
 * decrypt, merely returning the input verbatim.
 */
function FakeCryptoService() {
  this.counter = 0;

  delete Svc.Crypto;  // get rid of the getter first
  Svc.Crypto = this;

  CryptoWrapper.prototype.ciphertextHMAC = function ciphertextHMAC(keyBundle) {
    return fakeSHA256HMAC(this.ciphertext);
  };
}
FakeCryptoService.prototype = {

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

function setBasicCredentials(username, password, syncKey) {
  let auth = Identity;
  auth.username = username;
  auth.basicPassword = password;
  auth.syncKey = syncKey;
}

function SyncTestingInfrastructure(username, password, syncKey) {
  Cu.import("resource://services-sync/service.js");

  Identity.account = username || "foo";
  Identity.basicPassword = password || "password";
  Identity.syncKey = syncKey || "foo";

  Service.serverURL = TEST_SERVER_URL;
  Service.clusterURL = TEST_CLUSTER_URL;

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

// Turn WBO cleartext into fake "encrypted" payload as it goes over the wire.
function encryptPayload(cleartext) {
  if (typeof cleartext == "object") {
    cleartext = JSON.stringify(cleartext);
  }

  return {ciphertext: cleartext, // ciphertext == cleartext with fake crypto
          IV: "irrelevant",
          hmac: fakeSHA256HMAC(cleartext, Utils.makeHMACKey(""))};
}

function generateNewKeys(collections) {
  let wbo = CollectionKeys.generateNewKeysWBO(collections);
  let modified = new_timestamp();
  CollectionKeys.setContents(wbo.cleartext, modified);
}

function do_check_empty(obj) {
  do_check_attribute_count(obj, 0);
}

function do_check_attribute_count(obj, c) {
  do_check_eq(c, Object.keys(obj).length);
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

    if (!(id in this.items)) {
      record.deleted = true;
      return record;
    }

    record.denomination = this.items[id] || "Data for new record: " + id;
    return record;
  },

  changeItemID: function(oldID, newID) {
    if (oldID in this.items) {
      this.items[newID] = this.items[oldID];
    }

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
    // This is a semaphore used for testing proper reconciling on dupe
    // detection.
    if (item.id == "DUPE_INCOMING") {
      return "DUPE_LOCAL";
    }

    for (let [id, value] in Iterator(this._store.items)) {
      if (item.denomination == value) {
        return id;
      }
    }
  }
};

deepCopy: function deepCopy(thing, noSort) {
  if (typeof(thing) != "object" || thing == null){
    return thing;
  }
  let ret;

  if (Array.isArray(thing)) {
    ret = [];
    for (let i = 0; i < thing.length; i++){
      ret.push(deepCopy(thing[i], noSort));
	}
  } else {
    ret = {};
    let props = [p for (p in thing)];
    if (!noSort){
      props = props.sort();
    }
    props.forEach(function(k) ret[k] = deepCopy(thing[k], noSort));
  }

  return ret;
};