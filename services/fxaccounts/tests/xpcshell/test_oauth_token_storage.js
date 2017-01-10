/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/FxAccounts.jsm");
Cu.import("resource://gre/modules/FxAccountsClient.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/osfile.jsm");

// We grab some additional stuff via backstage passes.
var {AccountState} = Cu.import("resource://gre/modules/FxAccounts.jsm", {});

function promiseNotification(topic) {
  return new Promise(resolve => {
    let observe = () => {
      Services.obs.removeObserver(observe, topic);
      resolve();
    }
    Services.obs.addObserver(observe, topic, false);
  });
}

// A storage manager that doesn't actually write anywhere.
function MockStorageManager() {
}

MockStorageManager.prototype = {
  promiseInitialized: Promise.resolve(),

  initialize(accountData) {
    this.accountData = accountData;
  },

  finalize() {
    return Promise.resolve();
  },

  getAccountData() {
    return Promise.resolve(this.accountData);
  },

  updateAccountData(updatedFields) {
    for (let [name, value] of Object.entries(updatedFields)) {
      if (value == null) {
        delete this.accountData[name];
      } else {
        this.accountData[name] = value;
      }
    }
    return Promise.resolve();
  },

  deleteAccountData() {
    this.accountData = null;
    return Promise.resolve();
  }
}


// Just enough mocks so we can avoid hawk etc.
function MockFxAccountsClient() {
  this._email = "nobody@example.com";
  this._verified = false;

  this.accountStatus = function(uid) {
    let deferred = Promise.defer();
    deferred.resolve(!!uid && (!this._deletedOnServer));
    return deferred.promise;
  };

  this.signOut = function() { return Promise.resolve(); };
  this.registerDevice = function() { return Promise.resolve(); };
  this.updateDevice = function() { return Promise.resolve(); };
  this.signOutAndDestroyDevice = function() { return Promise.resolve(); };
  this.getDeviceList = function() { return Promise.resolve(); };

  FxAccountsClient.apply(this);
}

MockFxAccountsClient.prototype = {
  __proto__: FxAccountsClient.prototype
}

function MockFxAccounts(device = {}) {
  return new FxAccounts({
    fxAccountsClient: new MockFxAccountsClient(),
    newAccountState(credentials) {
      // we use a real accountState but mocked storage.
      let storage = new MockStorageManager();
      storage.initialize(credentials);
      return new AccountState(storage);
    },
    _getDeviceName() {
      return "mock device name";
    },
    fxaPushService: {
      registerPushEndpoint() {
        return new Promise((resolve) => {
          resolve({
            endpoint: "http://mochi.test:8888"
          });
        });
      },
    },
  });
}

function* createMockFxA() {
  let fxa = new MockFxAccounts();
  let credentials = {
    email: "foo@example.com",
    uid: "1234@lcip.org",
    assertion: "foobar",
    sessionToken: "dead",
    kA: "beef",
    kB: "cafe",
    verified: true
  };
  yield fxa.setSignedInUser(credentials);
  return fxa;
}

// The tests.
function run_test() {
  run_next_test();
}

add_task(function* testCacheStorage() {
  let fxa = yield createMockFxA();

  // Hook what the impl calls to save to disk.
  let cas = fxa.internal.currentAccountState;
  let origPersistCached = cas._persistCachedTokens.bind(cas)
  cas._persistCachedTokens = function() {
    return origPersistCached().then(() => {
      Services.obs.notifyObservers(null, "testhelper-fxa-cache-persist-done", null);
    });
  };

  let promiseWritten = promiseNotification("testhelper-fxa-cache-persist-done");
  let tokenData = {token: "token1", somethingelse: "something else"};
  let scopeArray = ["foo", "bar"];
  cas.setCachedToken(scopeArray, tokenData);
  deepEqual(cas.getCachedToken(scopeArray), tokenData);

  deepEqual(cas.oauthTokens, {"bar|foo": tokenData});
  // wait for background write to complete.
  yield promiseWritten;

  // Check the token cache made it to our mocked storage.
  deepEqual(cas.storageManager.accountData.oauthTokens, {"bar|foo": tokenData});

  // Drop the token from the cache and ensure it is removed from the json.
  promiseWritten = promiseNotification("testhelper-fxa-cache-persist-done");
  yield cas.removeCachedToken("token1");
  deepEqual(cas.oauthTokens, {});
  yield promiseWritten;
  deepEqual(cas.storageManager.accountData.oauthTokens, {});

  // sign out and the token storage should end up with null.
  let storageManager = cas.storageManager; // .signOut() removes the attribute.
  yield fxa.signOut( /* localOnly = */ true);
  deepEqual(storageManager.accountData, null);
});
