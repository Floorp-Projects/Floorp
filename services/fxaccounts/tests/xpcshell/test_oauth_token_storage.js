/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/FxAccounts.jsm");
Cu.import("resource://gre/modules/FxAccountsClient.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/osfile.jsm");

function promiseNotification(topic) {
  return new Promise(resolve => {
    let observe = () => {
      Services.obs.removeObserver(observe, topic);
      resolve();
    }
    Services.obs.addObserver(observe, topic, false);
  });
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

  FxAccountsClient.apply(this);
}

MockFxAccountsClient.prototype = {
  __proto__: FxAccountsClient.prototype
}

function MockFxAccounts() {
  return new FxAccounts({
    fxAccountsClient: new MockFxAccountsClient(),
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

add_task(function testCacheStorage() {
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

  deepEqual(cas.getAllCachedTokens(), [tokenData]);
  // wait for background write to complete.
  yield promiseWritten;

  // Check the token cache was written to signedInUserOAuthTokens.json.
  let path = OS.Path.join(OS.Constants.Path.profileDir, DEFAULT_OAUTH_TOKENS_FILENAME);
  let data = yield CommonUtils.readJSON(path);
  ok(data.tokens, "the data is in the json");
  equal(data.uid, "1234@lcip.org", "The user's uid is in the json");

  // Check it's all in the json.
  let expectedKey = "bar|foo";
  let entry = data.tokens[expectedKey];
  ok(entry, "our key is in the json");
  deepEqual(entry, tokenData, "correct token is in the json");

  // Drop the token from the cache and ensure it is removed from the json.
  promiseWritten = promiseNotification("testhelper-fxa-cache-persist-done");
  yield cas.removeCachedToken("token1");
  deepEqual(cas.getAllCachedTokens(), []);
  yield promiseWritten;
  data = yield CommonUtils.readJSON(path);
  ok(!data.tokens[expectedKey], "our key was removed from the json");

  // sign out and the token storage should end up with null.
  yield fxa.signOut( /* localOnly = */ true);
  data = yield CommonUtils.readJSON(path);
  ok(data === null, "data wiped on signout");
});

// Test that the tokens are available after a full read of credentials from disk.
add_task(function testCacheAfterRead() {
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
  yield promiseWritten;

  // trick things so the data is re-read from disk.
  cas.signedInUser = null;
  cas.oauthTokens = null;
  yield cas.getUserAccountData();
  ok(cas.oauthTokens, "token data was re-read");
  deepEqual(cas.getCachedToken(scopeArray), tokenData);
});

// Test that the tokens are saved after we read user credentials from disk.
add_task(function testCacheAfterRead() {
  let fxa = yield createMockFxA();
  // Hook what the impl calls to save to disk.
  let cas = fxa.internal.currentAccountState;
  let origPersistCached = cas._persistCachedTokens.bind(cas)

  // trick things so that FxAccounts is in the mode where we're reading data
  // from disk each time getSignedInUser() is called (ie, where .signedInUser
  // remains null)
  cas.signedInUser = null;
  cas.oauthTokens = null;

  yield cas.getUserAccountData();

  // hook our "persist" function.
  cas._persistCachedTokens = function() {
    return origPersistCached().then(() => {
      Services.obs.notifyObservers(null, "testhelper-fxa-cache-persist-done", null);
    });
  };
  let promiseWritten = promiseNotification("testhelper-fxa-cache-persist-done");

  // save a new token - it should be persisted.
  let tokenData = {token: "token1", somethingelse: "something else"};
  let scopeArray = ["foo", "bar"];
  cas.setCachedToken(scopeArray, tokenData);

  yield promiseWritten;

  // re-read the tokens directly from the storage to ensure they were persisted.
  let got = yield cas.signedInUserStorage.getOAuthTokens();
  ok(got, "got persisted data");
  ok(got.tokens, "have tokens");
  // this is internal knowledge of how scopes get turned into "keys", but that's OK
  ok(got.tokens["bar|foo"], "have our scope");
  equal(got.tokens["bar|foo"].token, "token1", "have our token");
});

// Test that the tokens are ignored when the token storage has an incorrect uid.
add_task(function testCacheAfterReadBadUID() {
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
  yield promiseWritten;

  // trick things so the data is re-read from disk.
  cas.signedInUser = null;
  cas.oauthTokens = null;

  // re-write the tokens data with an invalid UID.
  let path = OS.Path.join(OS.Constants.Path.profileDir, DEFAULT_OAUTH_TOKENS_FILENAME);
  let data = yield CommonUtils.readJSON(path);
  ok(data.tokens, "the data is in the json");
  equal(data.uid, "1234@lcip.org", "The user's uid is in the json");
  data.uid = "someone_else";
  yield CommonUtils.writeJSON(data, path);

  yield cas.getUserAccountData();
  deepEqual(cas.oauthTokens, {}, "token data ignored due to bad uid");
  equal(null, cas.getCachedToken(scopeArray), "no token available");
});
