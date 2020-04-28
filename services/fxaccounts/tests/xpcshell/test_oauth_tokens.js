/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FxAccounts } = ChromeUtils.import(
  "resource://gre/modules/FxAccounts.jsm"
);
const { FxAccountsClient } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsClient.jsm"
);
var { AccountState } = ChromeUtils.import(
  "resource://gre/modules/FxAccounts.jsm",
  null
);

function promiseNotification(topic) {
  return new Promise(resolve => {
    let observe = () => {
      Services.obs.removeObserver(observe, topic);
      resolve();
    };
    Services.obs.addObserver(observe, topic);
  });
}

// Just enough mocks so we can avoid hawk and storage.
function MockStorageManager() {}

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
  },
};

function MockFxAccountsClient(activeTokens) {
  this._email = "nobody@example.com";
  this._verified = false;

  this.accountStatus = function(uid) {
    return Promise.resolve(!!uid && !this._deletedOnServer);
  };

  this.signOut = function() {
    return Promise.resolve();
  };
  this.registerDevice = function() {
    return Promise.resolve();
  };
  this.updateDevice = function() {
    return Promise.resolve();
  };
  this.signOutAndDestroyDevice = function() {
    return Promise.resolve();
  };
  this.getDeviceList = function() {
    return Promise.resolve();
  };
  this.oauthDestroy = sinon.stub().callsFake((_clientId, token) => {
    this.activeTokens.delete(token);
    return Promise.resolve();
  });

  // Test only stuff.
  this.activeTokens = activeTokens;

  FxAccountsClient.apply(this);
}

MockFxAccountsClient.prototype = {
  __proto__: FxAccountsClient.prototype,
};

function MockFxAccountsOAuthGrantClient(activeTokens) {
  // Test only stuff.
  this.numTokenFetches = 0;
  this.activeTokens = activeTokens;
}

MockFxAccountsOAuthGrantClient.prototype = {
  serverURL: { href: "http://localhost" },
  getTokenFromAssertion(assertion, scope, ttl) {
    let token = `token${this.numTokenFetches}`;
    if (ttl) {
      token += `-ttl-${ttl}`;
    }
    this.numTokenFetches += 1;
    this.activeTokens.add(token);
    print("getTokenFromAssertion returning token", token);
    return Promise.resolve({ access_token: token, ttl });
  },
};

function MockFxAccounts() {
  // The FxA "auth" and "oauth" servers both share the same db of tokens,
  // so we need to simulate the same here in the tests.
  const activeTokens = new Set();
  return new FxAccounts({
    fxAccountsClient: new MockFxAccountsClient(activeTokens),
    fxAccountsOAuthGrantClient: new MockFxAccountsOAuthGrantClient(
      activeTokens
    ),
    getAssertion: () => Promise.resolve("assertion"),
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
        return new Promise(resolve => {
          resolve({
            endpoint: "http://mochi.test:8888",
          });
        });
      },
    },
  });
}

async function createMockFxA() {
  let fxa = new MockFxAccounts();
  let credentials = {
    email: "foo@example.com",
    uid: "1234@lcip.org",
    assertion: "foobar",
    sessionToken: "dead",
    kSync: "beef",
    kXCS: "cafe",
    kExtSync: "bacon",
    kExtKbHash: "cheese",
    verified: true,
  };

  await fxa._internal.setSignedInUser(credentials);
  return fxa;
}

// The tests.

add_task(async function testRevoke() {
  let tokenOptions = { scope: "test-scope" };
  let fxa = await createMockFxA();
  let client = fxa._internal.fxAccountsClient;
  let oauthClient = fxa._internal.fxAccountsOAuthGrantClient;

  // get our first token and check we hit the mock.
  let token1 = await fxa.getOAuthToken(tokenOptions);
  equal(oauthClient.numTokenFetches, 1);
  equal(oauthClient.activeTokens.size, 1);
  ok(token1, "got a token");
  equal(token1, "token0");

  // drop the new token from our cache.
  await fxa.removeCachedOAuthToken({ token: token1 });
  ok(client.oauthDestroy.calledOnce);

  // the revoke should have been successful.
  equal(oauthClient.activeTokens.size, 0);
  // fetching it again hits the server.
  let token2 = await fxa.getOAuthToken(tokenOptions);
  equal(oauthClient.numTokenFetches, 2);
  equal(oauthClient.activeTokens.size, 1);
  ok(token2, "got a token");
  notEqual(token1, token2, "got a different token");
});

add_task(async function testSignOutDestroysTokens() {
  let fxa = await createMockFxA();
  let client = fxa._internal.fxAccountsClient;
  let oauthClient = fxa._internal.fxAccountsOAuthGrantClient;

  // get our first token and check we hit the mock.
  let token1 = await fxa.getOAuthToken({ scope: "test-scope" });
  equal(oauthClient.numTokenFetches, 1);
  equal(oauthClient.activeTokens.size, 1);
  ok(token1, "got a token");

  // get another
  let token2 = await fxa.getOAuthToken({ scope: "test-scope-2" });
  equal(oauthClient.numTokenFetches, 2);
  equal(oauthClient.activeTokens.size, 2);
  ok(token2, "got a token");
  notEqual(token1, token2, "got a different token");

  // FxA fires an observer when the "background" signout is complete.
  let signoutComplete = promiseNotification("testhelper-fxa-signout-complete");
  // now sign out - they should be removed.
  await fxa.signOut();
  await signoutComplete;
  ok(client.oauthDestroy.calledTwice);
  // No active tokens left.
  equal(oauthClient.activeTokens.size, 0);
});

add_task(async function testTokenRaces() {
  // Here we do 2 concurrent fetches each for 2 different token scopes (ie,
  // 4 token fetches in total).
  // This should provoke a potential race in the token fetching but we use
  // a map of in-flight token fetches, so we should still only perform 2
  // fetches, but each of the 4 calls should resolve with the correct values.
  let fxa = await createMockFxA();
  let client = fxa._internal.fxAccountsClient;
  let oauthClient = fxa._internal.fxAccountsOAuthGrantClient;

  let results = await Promise.all([
    fxa.getOAuthToken({ scope: "test-scope" }),
    fxa.getOAuthToken({ scope: "test-scope" }),
    fxa.getOAuthToken({ scope: "test-scope-2" }),
    fxa.getOAuthToken({ scope: "test-scope-2" }),
  ]);

  equal(oauthClient.numTokenFetches, 2, "should have fetched 2 tokens.");

  // Should have 2 unique tokens
  results.sort();
  equal(results[0], results[1]);
  equal(results[2], results[3]);
  // should be 2 active.
  equal(oauthClient.activeTokens.size, 2);
  await fxa.removeCachedOAuthToken({ token: results[0] });
  equal(oauthClient.activeTokens.size, 1);
  await fxa.removeCachedOAuthToken({ token: results[2] });
  equal(oauthClient.activeTokens.size, 0);
  ok(client.oauthDestroy.calledTwice);
});

add_task(async function testTokenTTL() {
  // This tests the TTL option passed into the method
  let fxa = await createMockFxA();
  let token = await fxa.getOAuthToken({ scope: "test-ttl", ttl: 1000 });
  equal(token, "token0-ttl-1000");
});
