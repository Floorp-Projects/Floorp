/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FxAccounts.jsm");
Cu.import("resource://gre/modules/FxAccountsClient.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Log.jsm");

initTestLogging("Trace");

var log = Log.repository.getLogger("Services.FxAccounts.test");
log.level = Log.Level.Debug;

const BOGUS_PUBLICKEY = "BBXOKjUb84pzws1wionFpfCBjDuCh4-s_1b52WA46K5wYL2gCWEOmFKWn_NkS5nmJwTBuO8qxxdjAIDtNeklvQc";
const BOGUS_AUTHKEY = "GSsIiaD2Mr83iPqwFNK4rw";

Services.prefs.setCharPref("identity.fxaccounts.loglevel", "Trace");
Log.repository.getLogger("FirefoxAccounts").level = Log.Level.Trace;

Services.prefs.setCharPref("identity.fxaccounts.remote.oauth.uri", "https://example.com/v1");
Services.prefs.setCharPref("identity.fxaccounts.oauth.client_id", "abc123");
Services.prefs.setCharPref("identity.fxaccounts.remote.profile.uri", "http://example.com/v1");
Services.prefs.setCharPref("identity.fxaccounts.settings.uri", "http://accounts.example.com/");

const DEVICE_REGISTRATION_VERSION = 42;

function MockStorageManager() {
}

MockStorageManager.prototype = {
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

function MockFxAccountsClient(device) {
  this._email = "nobody@example.com";
  this._verified = false;
  this._deletedOnServer = false; // for testing accountStatus

  // mock calls up to the auth server to determine whether the
  // user account has been verified
  this.recoveryEmailStatus = function(sessionToken) {
    // simulate a call to /recovery_email/status
    return Promise.resolve({
      email: this._email,
      verified: this._verified
    });
  };

  this.accountStatus = function(uid) {
    let deferred = Promise.defer();
    deferred.resolve(!!uid && (!this._deletedOnServer));
    return deferred.promise;
  };

  const { id: deviceId, name: deviceName, type: deviceType, sessionToken } = device;

  this.registerDevice = (st, name, type) => Promise.resolve({ id: deviceId, name });
  this.updateDevice = (st, id, name) => Promise.resolve({ id, name });
  this.signOutAndDestroyDevice = () => Promise.resolve({});
  this.getDeviceList = (st) =>
    Promise.resolve([
      { id: deviceId, name: deviceName, type: deviceType, isCurrentDevice: st === sessionToken }
    ]);

  FxAccountsClient.apply(this);
}
MockFxAccountsClient.prototype = {
  __proto__: FxAccountsClient.prototype
}

function MockFxAccounts(device = {}) {
  return new FxAccounts({
    _getDeviceName() {
      return device.name || "mock device name";
    },
    fxAccountsClient: new MockFxAccountsClient(device),
    fxaPushService: {
      registerPushEndpoint() {
        return new Promise((resolve) => {
          resolve({
            endpoint: "http://mochi.test:8888",
            getKey(type) {
              return ChromeUtils.base64URLDecode(
                type === "auth" ? BOGUS_AUTHKEY : BOGUS_PUBLICKEY,
                { padding: "ignore" });
            }
          });
        });
      },
    },
    DEVICE_REGISTRATION_VERSION
  });
}

add_task(function* test_updateDeviceRegistration_with_new_device() {
  const deviceName = "foo";
  const deviceType = "bar";

  const credentials = getTestUser("baz");
  delete credentials.deviceId;
  const fxa = new MockFxAccounts({ name: deviceName });
  yield fxa.internal.setSignedInUser(credentials);

  const spy = {
    registerDevice: { count: 0, args: [] },
    updateDevice: { count: 0, args: [] },
    getDeviceList: { count: 0, args: [] }
  };
  const client = fxa.internal.fxAccountsClient;
  client.registerDevice = function() {
    spy.registerDevice.count += 1;
    spy.registerDevice.args.push(arguments);
    return Promise.resolve({
      id: "newly-generated device id",
      createdAt: Date.now(),
      name: deviceName,
      type: deviceType
    });
  };
  client.updateDevice = function() {
    spy.updateDevice.count += 1;
    spy.updateDevice.args.push(arguments);
    return Promise.resolve({});
  };
  client.getDeviceList = function() {
    spy.getDeviceList.count += 1;
    spy.getDeviceList.args.push(arguments);
    return Promise.resolve([]);
  };

  const result = yield fxa.updateDeviceRegistration();

  do_check_eq(result, "newly-generated device id");
  do_check_eq(spy.updateDevice.count, 0);
  do_check_eq(spy.getDeviceList.count, 0);
  do_check_eq(spy.registerDevice.count, 1);
  do_check_eq(spy.registerDevice.args[0].length, 4);
  do_check_eq(spy.registerDevice.args[0][0], credentials.sessionToken);
  do_check_eq(spy.registerDevice.args[0][1], deviceName);
  do_check_eq(spy.registerDevice.args[0][2], "desktop");
  do_check_eq(spy.registerDevice.args[0][3].pushCallback, "http://mochi.test:8888");
  do_check_eq(spy.registerDevice.args[0][3].pushPublicKey, BOGUS_PUBLICKEY);
  do_check_eq(spy.registerDevice.args[0][3].pushAuthKey, BOGUS_AUTHKEY);

  const state = fxa.internal.currentAccountState;
  const data = yield state.getUserAccountData();

  do_check_eq(data.deviceId, "newly-generated device id");
  do_check_eq(data.deviceRegistrationVersion, DEVICE_REGISTRATION_VERSION);
});

add_task(function* test_updateDeviceRegistration_with_existing_device() {
  const deviceName = "phil's device";
  const deviceType = "desktop";

  const credentials = getTestUser("pb");
  const fxa = new MockFxAccounts({ name: deviceName });
  yield fxa.internal.setSignedInUser(credentials);

  const spy = {
    registerDevice: { count: 0, args: [] },
    updateDevice: { count: 0, args: [] },
    getDeviceList: { count: 0, args: [] }
  };
  const client = fxa.internal.fxAccountsClient;
  client.registerDevice = function() {
    spy.registerDevice.count += 1;
    spy.registerDevice.args.push(arguments);
    return Promise.resolve({});
  };
  client.updateDevice = function() {
    spy.updateDevice.count += 1;
    spy.updateDevice.args.push(arguments);
    return Promise.resolve({
      id: credentials.deviceId,
      name: deviceName
    });
  };
  client.getDeviceList = function() {
    spy.getDeviceList.count += 1;
    spy.getDeviceList.args.push(arguments);
    return Promise.resolve([]);
  };
  const result = yield fxa.updateDeviceRegistration();

  do_check_eq(result, credentials.deviceId);
  do_check_eq(spy.registerDevice.count, 0);
  do_check_eq(spy.getDeviceList.count, 0);
  do_check_eq(spy.updateDevice.count, 1);
  do_check_eq(spy.updateDevice.args[0].length, 4);
  do_check_eq(spy.updateDevice.args[0][0], credentials.sessionToken);
  do_check_eq(spy.updateDevice.args[0][1], credentials.deviceId);
  do_check_eq(spy.updateDevice.args[0][2], deviceName);
  do_check_eq(spy.updateDevice.args[0][3].pushCallback, "http://mochi.test:8888");
  do_check_eq(spy.updateDevice.args[0][3].pushPublicKey, BOGUS_PUBLICKEY);
  do_check_eq(spy.updateDevice.args[0][3].pushAuthKey, BOGUS_AUTHKEY);

  const state = fxa.internal.currentAccountState;
  const data = yield state.getUserAccountData();

  do_check_eq(data.deviceId, credentials.deviceId);
  do_check_eq(data.deviceRegistrationVersion, DEVICE_REGISTRATION_VERSION);
});

add_task(function* test_updateDeviceRegistration_with_unknown_device_error() {
  const deviceName = "foo";
  const deviceType = "bar";

  const credentials = getTestUser("baz");
  const fxa = new MockFxAccounts({ name: deviceName });
  yield fxa.internal.setSignedInUser(credentials);

  const spy = {
    registerDevice: { count: 0, args: [] },
    updateDevice: { count: 0, args: [] },
    getDeviceList: { count: 0, args: [] }
  };
  const client = fxa.internal.fxAccountsClient;
  client.registerDevice = function() {
    spy.registerDevice.count += 1;
    spy.registerDevice.args.push(arguments);
    return Promise.resolve({
      id: "a different newly-generated device id",
      createdAt: Date.now(),
      name: deviceName,
      type: deviceType
    });
  };
  client.updateDevice = function() {
    spy.updateDevice.count += 1;
    spy.updateDevice.args.push(arguments);
    return Promise.reject({
      code: 400,
      errno: ERRNO_UNKNOWN_DEVICE
    });
  };
  client.getDeviceList = function() {
    spy.getDeviceList.count += 1;
    spy.getDeviceList.args.push(arguments);
    return Promise.resolve([]);
  };

  const result = yield fxa.updateDeviceRegistration();

  do_check_null(result);
  do_check_eq(spy.getDeviceList.count, 0);
  do_check_eq(spy.registerDevice.count, 0);
  do_check_eq(spy.updateDevice.count, 1);
  do_check_eq(spy.updateDevice.args[0].length, 4);
  do_check_eq(spy.updateDevice.args[0][0], credentials.sessionToken);
  do_check_eq(spy.updateDevice.args[0][1], credentials.deviceId);
  do_check_eq(spy.updateDevice.args[0][2], deviceName);
  do_check_eq(spy.updateDevice.args[0][3].pushCallback, "http://mochi.test:8888");
  do_check_eq(spy.updateDevice.args[0][3].pushPublicKey, BOGUS_PUBLICKEY);
  do_check_eq(spy.updateDevice.args[0][3].pushAuthKey, BOGUS_AUTHKEY);


  const state = fxa.internal.currentAccountState;
  const data = yield state.getUserAccountData();

  do_check_null(data.deviceId);
  do_check_eq(data.deviceRegistrationVersion, DEVICE_REGISTRATION_VERSION);
});

add_task(function* test_updateDeviceRegistration_with_device_session_conflict_error() {
  const deviceName = "foo";
  const deviceType = "bar";

  const credentials = getTestUser("baz");
  const fxa = new MockFxAccounts({ name: deviceName });
  yield fxa.internal.setSignedInUser(credentials);

  const spy = {
    registerDevice: { count: 0, args: [] },
    updateDevice: { count: 0, args: [], times: [] },
    getDeviceList: { count: 0, args: [] }
  };
  const client = fxa.internal.fxAccountsClient;
  client.registerDevice = function() {
    spy.registerDevice.count += 1;
    spy.registerDevice.args.push(arguments);
    return Promise.resolve({});
  };
  client.updateDevice = function() {
    spy.updateDevice.count += 1;
    spy.updateDevice.args.push(arguments);
    spy.updateDevice.time = Date.now();
    if (spy.updateDevice.count === 1) {
      return Promise.reject({
        code: 400,
        errno: ERRNO_DEVICE_SESSION_CONFLICT
      });
    }
    return Promise.resolve({
      id: credentials.deviceId,
      name: deviceName
    });
  };
  client.getDeviceList = function() {
    spy.getDeviceList.count += 1;
    spy.getDeviceList.args.push(arguments);
    spy.getDeviceList.time = Date.now();
    return Promise.resolve([
      { id: "ignore", name: "ignore", type: "ignore", isCurrentDevice: false },
      { id: credentials.deviceId, name: deviceName, type: deviceType, isCurrentDevice: true }
    ]);
  };

  const result = yield fxa.updateDeviceRegistration();

  do_check_eq(result, credentials.deviceId);
  do_check_eq(spy.registerDevice.count, 0);
  do_check_eq(spy.updateDevice.count, 1);
  do_check_eq(spy.updateDevice.args[0].length, 4);
  do_check_eq(spy.updateDevice.args[0][0], credentials.sessionToken);
  do_check_eq(spy.updateDevice.args[0][1], credentials.deviceId);
  do_check_eq(spy.updateDevice.args[0][2], deviceName);
  do_check_eq(spy.updateDevice.args[0][3].pushCallback, "http://mochi.test:8888");
  do_check_eq(spy.updateDevice.args[0][3].pushPublicKey, BOGUS_PUBLICKEY);
  do_check_eq(spy.updateDevice.args[0][3].pushAuthKey, BOGUS_AUTHKEY);
  do_check_eq(spy.getDeviceList.count, 1);
  do_check_eq(spy.getDeviceList.args[0].length, 1);
  do_check_eq(spy.getDeviceList.args[0][0], credentials.sessionToken);
  do_check_true(spy.getDeviceList.time >= spy.updateDevice.time);

  const state = fxa.internal.currentAccountState;
  const data = yield state.getUserAccountData();

  do_check_eq(data.deviceId, credentials.deviceId);
  do_check_eq(data.deviceRegistrationVersion, null);
});

add_task(function* test_updateDeviceRegistration_with_unrecoverable_error() {
  const deviceName = "foo";
  const deviceType = "bar";

  const credentials = getTestUser("baz");
  delete credentials.deviceId;
  const fxa = new MockFxAccounts({ name: deviceName });
  yield fxa.internal.setSignedInUser(credentials);

  const spy = {
    registerDevice: { count: 0, args: [] },
    updateDevice: { count: 0, args: [] },
    getDeviceList: { count: 0, args: [] }
  };
  const client = fxa.internal.fxAccountsClient;
  client.registerDevice = function() {
    spy.registerDevice.count += 1;
    spy.registerDevice.args.push(arguments);
    return Promise.reject({
      code: 400,
      errno: ERRNO_TOO_MANY_CLIENT_REQUESTS
    });
  };
  client.updateDevice = function() {
    spy.updateDevice.count += 1;
    spy.updateDevice.args.push(arguments);
    return Promise.resolve({});
  };
  client.getDeviceList = function() {
    spy.getDeviceList.count += 1;
    spy.getDeviceList.args.push(arguments);
    return Promise.resolve([]);
  };

  const result = yield fxa.updateDeviceRegistration();

  do_check_null(result);
  do_check_eq(spy.getDeviceList.count, 0);
  do_check_eq(spy.updateDevice.count, 0);
  do_check_eq(spy.registerDevice.count, 1);
  do_check_eq(spy.registerDevice.args[0].length, 4);

  const state = fxa.internal.currentAccountState;
  const data = yield state.getUserAccountData();

  do_check_null(data.deviceId);
});

add_task(function* test_getDeviceId_with_no_device_id_invokes_device_registration() {
  const credentials = getTestUser("foo");
  credentials.verified = true;
  delete credentials.deviceId;
  const fxa = new MockFxAccounts();
  yield fxa.internal.setSignedInUser(credentials);

  const spy = { count: 0, args: [] };
  fxa.internal.currentAccountState.getUserAccountData =
    () => Promise.resolve({ email: credentials.email,
                            deviceRegistrationVersion: DEVICE_REGISTRATION_VERSION });
  fxa.internal._registerOrUpdateDevice = function() {
    spy.count += 1;
    spy.args.push(arguments);
    return Promise.resolve("bar");
  };

  const result = yield fxa.internal.getDeviceId();

  do_check_eq(spy.count, 1);
  do_check_eq(spy.args[0].length, 1);
  do_check_eq(spy.args[0][0].email, credentials.email);
  do_check_null(spy.args[0][0].deviceId);
  do_check_eq(result, "bar");
});

add_task(function* test_getDeviceId_with_registration_version_outdated_invokes_device_registration() {
  const credentials = getTestUser("foo");
  credentials.verified = true;
  const fxa = new MockFxAccounts();
  yield fxa.internal.setSignedInUser(credentials);

  const spy = { count: 0, args: [] };
  fxa.internal.currentAccountState.getUserAccountData =
    () => Promise.resolve({ deviceId: credentials.deviceId, deviceRegistrationVersion: 0 });
  fxa.internal._registerOrUpdateDevice = function() {
    spy.count += 1;
    spy.args.push(arguments);
    return Promise.resolve("wibble");
  };

  const result = yield fxa.internal.getDeviceId();

  do_check_eq(spy.count, 1);
  do_check_eq(spy.args[0].length, 1);
  do_check_eq(spy.args[0][0].deviceId, credentials.deviceId);
  do_check_eq(result, "wibble");
});

add_task(function* test_getDeviceId_with_device_id_and_uptodate_registration_version_doesnt_invoke_device_registration() {
  const credentials = getTestUser("foo");
  credentials.verified = true;
  const fxa = new MockFxAccounts();
  yield fxa.internal.setSignedInUser(credentials);

  const spy = { count: 0 };
  fxa.internal.currentAccountState.getUserAccountData =
    () => Promise.resolve({ deviceId: credentials.deviceId, deviceRegistrationVersion: DEVICE_REGISTRATION_VERSION });
  fxa.internal._registerOrUpdateDevice = function() {
    spy.count += 1;
    return Promise.resolve("bar");
  };

  const result = yield fxa.internal.getDeviceId();

  do_check_eq(spy.count, 0);
  do_check_eq(result, "foo's device id");
});

add_task(function* test_getDeviceId_with_device_id_and_with_no_registration_version_invokes_device_registration() {
  const credentials = getTestUser("foo");
  credentials.verified = true;
  const fxa = new MockFxAccounts();
  yield fxa.internal.setSignedInUser(credentials);

  const spy = { count: 0, args: [] };
  fxa.internal.currentAccountState.getUserAccountData =
    () => Promise.resolve({ deviceId: credentials.deviceId });
  fxa.internal._registerOrUpdateDevice = function() {
    spy.count += 1;
    spy.args.push(arguments);
    return Promise.resolve("wibble");
  };

  const result = yield fxa.internal.getDeviceId();

  do_check_eq(spy.count, 1);
  do_check_eq(spy.args[0].length, 1);
  do_check_eq(spy.args[0][0].deviceId, credentials.deviceId);
  do_check_eq(result, "wibble");
});

function expandHex(two_hex) {
  // Return a 64-character hex string, encoding 32 identical bytes.
  let eight_hex = two_hex + two_hex + two_hex + two_hex;
  let thirtytwo_hex = eight_hex + eight_hex + eight_hex + eight_hex;
  return thirtytwo_hex + thirtytwo_hex;
}

function expandBytes(two_hex) {
  return CommonUtils.hexToBytes(expandHex(two_hex));
}

function getTestUser(name) {
  return {
    email: name + "@example.com",
    uid: "1ad7f502-4cc7-4ec1-a209-071fd2fae348",
    deviceId: name + "'s device id",
    sessionToken: name + "'s session token",
    keyFetchToken: name + "'s keyfetch token",
    unwrapBKey: expandHex("44"),
    verified: false
  };
}

