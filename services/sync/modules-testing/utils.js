/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "encryptPayload",
  "makeIdentityConfig",
  "makeFxAccountsInternalMock",
  "configureFxAccountIdentity",
  "configureIdentity",
  "SyncTestingInfrastructure",
  "waitForZeroTimer",
  "promiseZeroTimer",
  "promiseNamedTimer",
  "MockFxaStorageManager",
  "AccountState", // from a module import
  "sumHistogram",
  "getLoginTelemetryScalar",
];

var {utils: Cu} = Components;

Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-crypto/utils.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/browserid_identity.js");
Cu.import("resource://testing-common/services/common/logging.js");
Cu.import("resource://testing-common/services/sync/fakeservices.js");
Cu.import("resource://gre/modules/FxAccounts.jsm");
Cu.import("resource://gre/modules/FxAccountsClient.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/Services.jsm");

// and grab non-exported stuff via a backstage pass.
const {AccountState} = Cu.import("resource://gre/modules/FxAccounts.jsm", {});

// A mock "storage manager" for FxAccounts that doesn't actually write anywhere.
function MockFxaStorageManager() {
}

MockFxaStorageManager.prototype = {
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
};

/**
 * First wait >100ms (nsITimers can take up to that much time to fire, so
 * we can account for the timer in delayedAutoconnect) and then two event
 * loop ticks (to account for the CommonUtils.nextTick() in autoConnect).
 */
this.waitForZeroTimer = function waitForZeroTimer(callback) {
  let ticks = 2;
  function wait() {
    if (ticks) {
      ticks -= 1;
      CommonUtils.nextTick(wait);
      return;
    }
    callback();
  }
  CommonUtils.namedTimer(wait, 150, {}, "timer");
};

this.promiseZeroTimer = function() {
  return new Promise(resolve => {
    waitForZeroTimer(resolve);
  });
};

this.promiseNamedTimer = function(wait, thisObj, name) {
  return new Promise(resolve => {
    CommonUtils.namedTimer(resolve, wait, thisObj, name);
  });
};

// Return an identity configuration suitable for testing with our identity
// providers.  |overrides| can specify overrides for any default values.
// |server| is optional, but if specified, will be used to form the cluster
// URL for the FxA identity.
this.makeIdentityConfig = function(overrides) {
  // first setup the defaults.
  let result = {
    // Username used in both fxaccount and sync identity configs.
    username: "foo",
    // fxaccount specific credentials.
    fxaccount: {
      user: {
        assertion: "assertion",
        email: "email",
        kA: "kA",
        kB: "kB",
        sessionToken: "sessionToken",
        uid: "a".repeat(32),
        verified: true,
      },
      token: {
        endpoint: null,
        duration: 300,
        id: "id",
        key: "key",
        hashed_fxa_uid: "f".repeat(32), // used during telemetry validation
        // uid will be set to the username.
      }
    }
  };

  // Now handle any specified overrides.
  if (overrides) {
    if (overrides.username) {
      result.username = overrides.username;
    }
    if (overrides.fxaccount) {
      // TODO: allow just some attributes to be specified
      result.fxaccount = overrides.fxaccount;
    }
  }
  return result;
};

this.makeFxAccountsInternalMock = function(config) {
  return {
    newAccountState(credentials) {
      // We only expect this to be called with null indicating the (mock)
      // storage should be read.
      if (credentials) {
        throw new Error("Not expecting to have credentials passed");
      }
      let storageManager = new MockFxaStorageManager();
      storageManager.initialize(config.fxaccount.user);
      let accountState = new AccountState(storageManager);
      return accountState;
    },
    _getAssertion(audience) {
      return Promise.resolve("assertion");
    },
  };
};

// Configure an instance of an FxAccount identity provider with the specified
// config (or the default config if not specified).
this.configureFxAccountIdentity = function(authService,
                                           config = makeIdentityConfig(),
                                           fxaInternal = makeFxAccountsInternalMock(config)) {
  // until we get better test infrastructure for bid_identity, we set the
  // signedin user's "email" to the username, simply as many tests rely on this.
  config.fxaccount.user.email = config.username;

  let fxa = new FxAccounts(fxaInternal);

  let MockFxAccountsClient = function() {
    FxAccountsClient.apply(this);
  };
  MockFxAccountsClient.prototype = {
    __proto__: FxAccountsClient.prototype,
    accountStatus() {
      return Promise.resolve(true);
    }
  };
  let mockFxAClient = new MockFxAccountsClient();
  fxa.internal._fxAccountsClient = mockFxAClient;

  let mockTSC = { // TokenServerClient
    getTokenFromBrowserIDAssertion(uri, assertion, cb) {
      config.fxaccount.token.uid = config.username;
      cb(null, config.fxaccount.token);
    },
  };
  authService._fxaService = fxa;
  authService._tokenServerClient = mockTSC;
  // Set the "account" of the browserId manager to be the "email" of the
  // logged in user of the mockFXA service.
  authService._signedInUser = config.fxaccount.user;
  authService._account = config.fxaccount.user.email;
};

this.configureIdentity = async function(identityOverrides, server) {
  let config = makeIdentityConfig(identityOverrides, server);
  let ns = {};
  Cu.import("resource://services-sync/service.js", ns);

  // If a server was specified, ensure FxA has a correct cluster URL available.
  if (server && !config.fxaccount.token.endpoint) {
    let ep = server.baseURI;
    if (!ep.endsWith("/")) {
      ep += "/";
    }
    ep += "1.1/" + config.username + "/";
    config.fxaccount.token.endpoint = ep;
  }

  configureFxAccountIdentity(ns.Service.identity, config);
  await ns.Service.identity.initializeWithCurrentIdentity();
  // The token is fetched in the background, whenReadyToAuthenticate is resolved
  // when we are ready.
  await ns.Service.identity.whenReadyToAuthenticate.promise;
  // and cheat to avoid requiring each test do an explicit login - give it
  // a cluster URL.
  if (config.fxaccount.token.endpoint) {
    ns.Service.clusterURL = config.fxaccount.token.endpoint;
  }
};

this.SyncTestingInfrastructure = async function(server, username) {
  let ns = {};
  Cu.import("resource://services-sync/service.js", ns);

  let config = makeIdentityConfig({ username });
  await configureIdentity(config, server);
  return {
    logStats: initTestLogging(),
    fakeFilesystem: new FakeFilesystemService({}),
    fakeGUIDService: new FakeGUIDService(),
    fakeCryptoService: new FakeCryptoService(),
  };
};

/**
 * Turn WBO cleartext into fake "encrypted" payload as it goes over the wire.
 */
this.encryptPayload = function encryptPayload(cleartext) {
  if (typeof cleartext == "object") {
    cleartext = JSON.stringify(cleartext);
  }

  return {
    ciphertext: cleartext, // ciphertext == cleartext with fake crypto
    IV: "irrelevant",
    hmac: fakeSHA256HMAC(cleartext, CryptoUtils.makeHMACKey("")),
  };
};

this.sumHistogram = function(name, options = {}) {
  let histogram = options.key ? Services.telemetry.getKeyedHistogramById(name) :
                  Services.telemetry.getHistogramById(name);
  let snapshot = histogram.snapshot(options.key);
  let sum = -Infinity;
  if (snapshot) {
    sum = snapshot.sum;
  }
  histogram.clear();
  return sum;
};

this.getLoginTelemetryScalar = function() {
  let dataset = Services.telemetry.DATASET_RELEASE_CHANNEL_OPTOUT;
  let snapshot = Services.telemetry.snapshotKeyedScalars(dataset, true);
  return snapshot.parent ? snapshot.parent["services.sync.sync_login_state_transitions"] : {};
};
