/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FxAccounts } = ChromeUtils.import(
  "resource://gre/modules/FxAccounts.jsm"
);
const { FxAccountsClient } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsClient.jsm"
);
const { FxAccountsDevice } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsDevice.jsm"
);
const {
  ERRNO_DEVICE_SESSION_CONFLICT,
  ERRNO_TOO_MANY_CLIENT_REQUESTS,
  ERRNO_UNKNOWN_DEVICE,
  ON_DEVICE_CONNECTED_NOTIFICATION,
  ON_DEVICE_DISCONNECTED_NOTIFICATION,
} = ChromeUtils.import("resource://gre/modules/FxAccountsCommon.js");
var { AccountState } = ChromeUtils.import(
  "resource://gre/modules/FxAccounts.jsm"
);

initTestLogging("Trace");

var log = Log.repository.getLogger("Services.FxAccounts.test");
log.level = Log.Level.Debug;

const BOGUS_PUBLICKEY =
  "BBXOKjUb84pzws1wionFpfCBjDuCh4-s_1b52WA46K5wYL2gCWEOmFKWn_NkS5nmJwTBuO8qxxdjAIDtNeklvQc";
const BOGUS_AUTHKEY = "GSsIiaD2Mr83iPqwFNK4rw";

Services.prefs.setCharPref("identity.fxaccounts.loglevel", "Trace");

const DEVICE_REGISTRATION_VERSION = 42;

function MockStorageManager() {}

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
  },
};

function MockFxAccountsClient(device) {
  this._email = "nobody@example.com";
  // Be careful relying on `this._verified` as it doesn't change if the user's
  // state does via setting the `verified` flag in the user data.
  this._verified = false;
  this._deletedOnServer = false; // for testing accountStatus

  // mock calls up to the auth server to determine whether the
  // user account has been verified
  this.recoveryEmailStatus = function(sessionToken) {
    // simulate a call to /recovery_email/status
    return Promise.resolve({
      email: this._email,
      verified: this._verified,
    });
  };

  this.accountKeys = function(keyFetchToken) {
    Assert.ok(keyFetchToken, "must be called with a key-fetch-token");
    // ideally we'd check the verification status here to more closely simulate
    // the server, but `this._verified` is a test-only construct and doesn't
    // update when the user changes verification status.
    Assert.ok(!this._deletedOnServer, "this test thinks the acct is deleted!");
    return {
      kA: "test-ka",
      wrapKB: "X".repeat(32),
    };
  };

  this.accountStatus = function(uid) {
    return Promise.resolve(!!uid && !this._deletedOnServer);
  };

  const {
    id: deviceId,
    name: deviceName,
    type: deviceType,
    sessionToken,
  } = device;

  this.registerDevice = (st, name, type) =>
    Promise.resolve({ id: deviceId, name });
  this.updateDevice = (st, id, name) => Promise.resolve({ id, name });
  this.signOut = () => Promise.resolve({});
  this.getDeviceList = st =>
    Promise.resolve([
      {
        id: deviceId,
        name: deviceName,
        type: deviceType,
        isCurrentDevice: st === sessionToken,
      },
    ]);

  FxAccountsClient.apply(this);
}
MockFxAccountsClient.prototype = {
  __proto__: FxAccountsClient.prototype,
};

async function MockFxAccounts(credentials, device = {}) {
  let fxa = new FxAccounts({
    newAccountState(creds) {
      // we use a real accountState but mocked storage.
      let storage = new MockStorageManager();
      storage.initialize(creds);
      return new AccountState(storage);
    },
    fxAccountsClient: new MockFxAccountsClient(device, credentials),
    fxaPushService: {
      registerPushEndpoint() {
        return new Promise(resolve => {
          resolve({
            endpoint: "http://mochi.test:8888",
            getKey(type) {
              return ChromeUtils.base64URLDecode(
                type === "auth" ? BOGUS_AUTHKEY : BOGUS_PUBLICKEY,
                { padding: "ignore" }
              );
            },
          });
        });
      },
      unsubscribe() {
        return Promise.resolve();
      },
    },
    commands: {
      async availableCommands() {
        return {};
      },
    },
    device: {
      DEVICE_REGISTRATION_VERSION,
      _checkRemoteCommandsUpdateNeeded: async () => false,
    },
    VERIFICATION_POLL_TIMEOUT_INITIAL: 1,
  });
  await fxa._internal.setSignedInUser(credentials);
  Services.prefs.setStringPref(
    "identity.fxaccounts.account.device.name",
    device.name || "mock device name"
  );
  return fxa;
}

function updateUserAccountData(fxa, data) {
  return fxa._internal.updateUserAccountData(data);
}

add_task(async function test_updateDeviceRegistration_with_new_device() {
  const deviceName = "foo";
  const deviceType = "bar";

  const credentials = getTestUser("baz");
  const fxa = await MockFxAccounts(credentials, { name: deviceName });
  // Remove the current device registration (setSignedInUser does one!).
  await updateUserAccountData(fxa, { uid: credentials.uid, device: null });

  const spy = {
    registerDevice: { count: 0, args: [] },
    updateDevice: { count: 0, args: [] },
    getDeviceList: { count: 0, args: [] },
  };
  const client = fxa._internal.fxAccountsClient;
  client.registerDevice = function() {
    spy.registerDevice.count += 1;
    spy.registerDevice.args.push(arguments);
    return Promise.resolve({
      id: "newly-generated device id",
      createdAt: Date.now(),
      name: deviceName,
      type: deviceType,
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

  await fxa.updateDeviceRegistration();

  Assert.equal(spy.updateDevice.count, 0);
  Assert.equal(spy.getDeviceList.count, 0);
  Assert.equal(spy.registerDevice.count, 1);
  Assert.equal(spy.registerDevice.args[0].length, 4);
  Assert.equal(spy.registerDevice.args[0][0], credentials.sessionToken);
  Assert.equal(spy.registerDevice.args[0][1], deviceName);
  Assert.equal(spy.registerDevice.args[0][2], "desktop");
  Assert.equal(
    spy.registerDevice.args[0][3].pushCallback,
    "http://mochi.test:8888"
  );
  Assert.equal(spy.registerDevice.args[0][3].pushPublicKey, BOGUS_PUBLICKEY);
  Assert.equal(spy.registerDevice.args[0][3].pushAuthKey, BOGUS_AUTHKEY);

  const state = fxa._internal.currentAccountState;
  const data = await state.getUserAccountData();

  Assert.equal(data.device.id, "newly-generated device id");
  Assert.equal(data.device.registrationVersion, DEVICE_REGISTRATION_VERSION);
  await fxa.signOut(true);
});

add_task(async function test_updateDeviceRegistration_with_existing_device() {
  const deviceId = "my device id";
  const deviceName = "phil's device";

  const credentials = getTestUser("pb");
  const fxa = await MockFxAccounts(credentials, { name: deviceName });
  await updateUserAccountData(fxa, {
    uid: credentials.uid,
    device: {
      id: deviceId,
      registeredCommandsKeys: [],
      registrationVersion: 1, // < 42
    },
  });

  const spy = {
    registerDevice: { count: 0, args: [] },
    updateDevice: { count: 0, args: [] },
    getDeviceList: { count: 0, args: [] },
  };
  const client = fxa._internal.fxAccountsClient;
  client.registerDevice = function() {
    spy.registerDevice.count += 1;
    spy.registerDevice.args.push(arguments);
    return Promise.resolve({});
  };
  client.updateDevice = function() {
    spy.updateDevice.count += 1;
    spy.updateDevice.args.push(arguments);
    return Promise.resolve({
      id: deviceId,
      name: deviceName,
    });
  };
  client.getDeviceList = function() {
    spy.getDeviceList.count += 1;
    spy.getDeviceList.args.push(arguments);
    return Promise.resolve([]);
  };
  await fxa.updateDeviceRegistration();

  Assert.equal(spy.registerDevice.count, 0);
  Assert.equal(spy.getDeviceList.count, 0);
  Assert.equal(spy.updateDevice.count, 1);
  Assert.equal(spy.updateDevice.args[0].length, 4);
  Assert.equal(spy.updateDevice.args[0][0], credentials.sessionToken);
  Assert.equal(spy.updateDevice.args[0][1], deviceId);
  Assert.equal(spy.updateDevice.args[0][2], deviceName);
  Assert.equal(
    spy.updateDevice.args[0][3].pushCallback,
    "http://mochi.test:8888"
  );
  Assert.equal(spy.updateDevice.args[0][3].pushPublicKey, BOGUS_PUBLICKEY);
  Assert.equal(spy.updateDevice.args[0][3].pushAuthKey, BOGUS_AUTHKEY);

  const state = fxa._internal.currentAccountState;
  const data = await state.getUserAccountData();

  Assert.equal(data.device.id, deviceId);
  Assert.equal(data.device.registrationVersion, DEVICE_REGISTRATION_VERSION);
  await fxa.signOut(true);
});

add_task(
  async function test_updateDeviceRegistration_with_unknown_device_error() {
    const deviceName = "foo";
    const deviceType = "bar";
    const currentDeviceId = "my device id";

    const credentials = getTestUser("baz");
    const fxa = await MockFxAccounts(credentials, { name: deviceName });
    await updateUserAccountData(fxa, {
      uid: credentials.uid,
      device: {
        id: currentDeviceId,
        registeredCommandsKeys: [],
        registrationVersion: 1, // < 42
      },
    });

    const spy = {
      registerDevice: { count: 0, args: [] },
      updateDevice: { count: 0, args: [] },
      getDeviceList: { count: 0, args: [] },
    };
    const client = fxa._internal.fxAccountsClient;
    client.registerDevice = function() {
      spy.registerDevice.count += 1;
      spy.registerDevice.args.push(arguments);
      return Promise.resolve({
        id: "a different newly-generated device id",
        createdAt: Date.now(),
        name: deviceName,
        type: deviceType,
      });
    };
    client.updateDevice = function() {
      spy.updateDevice.count += 1;
      spy.updateDevice.args.push(arguments);
      return Promise.reject({
        code: 400,
        errno: ERRNO_UNKNOWN_DEVICE,
      });
    };
    client.getDeviceList = function() {
      spy.getDeviceList.count += 1;
      spy.getDeviceList.args.push(arguments);
      return Promise.resolve([]);
    };

    await fxa.updateDeviceRegistration();

    Assert.equal(spy.getDeviceList.count, 0);
    Assert.equal(spy.registerDevice.count, 0);
    Assert.equal(spy.updateDevice.count, 1);
    Assert.equal(spy.updateDevice.args[0].length, 4);
    Assert.equal(spy.updateDevice.args[0][0], credentials.sessionToken);
    Assert.equal(spy.updateDevice.args[0][1], currentDeviceId);
    Assert.equal(spy.updateDevice.args[0][2], deviceName);
    Assert.equal(
      spy.updateDevice.args[0][3].pushCallback,
      "http://mochi.test:8888"
    );
    Assert.equal(spy.updateDevice.args[0][3].pushPublicKey, BOGUS_PUBLICKEY);
    Assert.equal(spy.updateDevice.args[0][3].pushAuthKey, BOGUS_AUTHKEY);

    const state = fxa._internal.currentAccountState;
    const data = await state.getUserAccountData();

    Assert.equal(null, data.device);
    await fxa.signOut(true);
  }
);

add_task(
  async function test_updateDeviceRegistration_with_device_session_conflict_error() {
    const deviceName = "foo";
    const deviceType = "bar";
    const currentDeviceId = "my device id";
    const conflictingDeviceId = "conflicting device id";

    const credentials = getTestUser("baz");
    const fxa = await MockFxAccounts(credentials, { name: deviceName });
    await updateUserAccountData(fxa, {
      uid: credentials.uid,
      device: {
        id: currentDeviceId,
        registeredCommandsKeys: [],
        registrationVersion: 1, // < 42
      },
    });

    const spy = {
      registerDevice: { count: 0, args: [] },
      updateDevice: { count: 0, args: [], times: [] },
      getDeviceList: { count: 0, args: [] },
    };
    const client = fxa._internal.fxAccountsClient;
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
          errno: ERRNO_DEVICE_SESSION_CONFLICT,
        });
      }
      return Promise.resolve({
        id: conflictingDeviceId,
        name: deviceName,
      });
    };
    client.getDeviceList = function() {
      spy.getDeviceList.count += 1;
      spy.getDeviceList.args.push(arguments);
      spy.getDeviceList.time = Date.now();
      return Promise.resolve([
        {
          id: "ignore",
          name: "ignore",
          type: "ignore",
          isCurrentDevice: false,
        },
        {
          id: conflictingDeviceId,
          name: deviceName,
          type: deviceType,
          isCurrentDevice: true,
        },
      ]);
    };

    await fxa.updateDeviceRegistration();

    Assert.equal(spy.registerDevice.count, 0);
    Assert.equal(spy.updateDevice.count, 1);
    Assert.equal(spy.updateDevice.args[0].length, 4);
    Assert.equal(spy.updateDevice.args[0][0], credentials.sessionToken);
    Assert.equal(spy.updateDevice.args[0][1], currentDeviceId);
    Assert.equal(spy.updateDevice.args[0][2], deviceName);
    Assert.equal(
      spy.updateDevice.args[0][3].pushCallback,
      "http://mochi.test:8888"
    );
    Assert.equal(spy.updateDevice.args[0][3].pushPublicKey, BOGUS_PUBLICKEY);
    Assert.equal(spy.updateDevice.args[0][3].pushAuthKey, BOGUS_AUTHKEY);
    Assert.equal(spy.getDeviceList.count, 1);
    Assert.equal(spy.getDeviceList.args[0].length, 1);
    Assert.equal(spy.getDeviceList.args[0][0], credentials.sessionToken);
    Assert.ok(spy.getDeviceList.time >= spy.updateDevice.time);

    const state = fxa._internal.currentAccountState;
    const data = await state.getUserAccountData();

    Assert.equal(data.device.id, conflictingDeviceId);
    Assert.equal(data.device.registrationVersion, null);
    await fxa.signOut(true);
  }
);

add_task(
  async function test_updateDeviceRegistration_with_unrecoverable_error() {
    const deviceName = "foo";

    const credentials = getTestUser("baz");
    const fxa = await MockFxAccounts(credentials, { name: deviceName });
    await updateUserAccountData(fxa, { uid: credentials.uid, device: null });

    const spy = {
      registerDevice: { count: 0, args: [] },
      updateDevice: { count: 0, args: [] },
      getDeviceList: { count: 0, args: [] },
    };
    const client = fxa._internal.fxAccountsClient;
    client.registerDevice = function() {
      spy.registerDevice.count += 1;
      spy.registerDevice.args.push(arguments);
      return Promise.reject({
        code: 400,
        errno: ERRNO_TOO_MANY_CLIENT_REQUESTS,
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

    await fxa.updateDeviceRegistration();

    Assert.equal(spy.getDeviceList.count, 0);
    Assert.equal(spy.updateDevice.count, 0);
    Assert.equal(spy.registerDevice.count, 1);
    Assert.equal(spy.registerDevice.args[0].length, 4);

    const state = fxa._internal.currentAccountState;
    const data = await state.getUserAccountData();

    Assert.equal(null, data.device);
    await fxa.signOut(true);
  }
);

add_task(
  async function test_getDeviceId_with_no_device_id_invokes_device_registration() {
    const credentials = getTestUser("foo");
    credentials.verified = true;
    const fxa = await MockFxAccounts(credentials);
    await updateUserAccountData(fxa, { uid: credentials.uid, device: null });

    const spy = { count: 0, args: [] };
    fxa._internal.currentAccountState.getUserAccountData = () =>
      Promise.resolve({
        email: credentials.email,
        registrationVersion: DEVICE_REGISTRATION_VERSION,
      });
    fxa._internal.device._registerOrUpdateDevice = function() {
      spy.count += 1;
      spy.args.push(arguments);
      return Promise.resolve("bar");
    };

    const result = await fxa.device.getLocalId();

    Assert.equal(spy.count, 1);
    Assert.equal(spy.args[0].length, 2);
    Assert.equal(spy.args[0][1].email, credentials.email);
    Assert.equal(null, spy.args[0][1].device);
    Assert.equal(result, "bar");
    await fxa.signOut(true);
  }
);

add_task(
  async function test_getDeviceId_with_registration_version_outdated_invokes_device_registration() {
    const credentials = getTestUser("foo");
    credentials.verified = true;
    const fxa = await MockFxAccounts(credentials);

    const spy = { count: 0, args: [] };
    fxa._internal.currentAccountState.getUserAccountData = () =>
      Promise.resolve({
        device: {
          id: "my id",
          registrationVersion: 0,
          registeredCommandsKeys: [],
        },
      });
    fxa._internal.device._registerOrUpdateDevice = function() {
      spy.count += 1;
      spy.args.push(arguments);
      return Promise.resolve("wibble");
    };

    const result = await fxa.device.getLocalId();

    Assert.equal(spy.count, 1);
    Assert.equal(spy.args[0].length, 2);
    Assert.equal(spy.args[0][1].device.id, "my id");
    Assert.equal(result, "wibble");
    await fxa.signOut(true);
  }
);

add_task(
  async function test_getDeviceId_with_device_id_and_uptodate_registration_version_doesnt_invoke_device_registration() {
    const credentials = getTestUser("foo");
    credentials.verified = true;
    const fxa = await MockFxAccounts(credentials);

    const spy = { count: 0 };
    fxa._internal.currentAccountState.getUserAccountData = async () => ({
      device: {
        id: "foo's device id",
        registrationVersion: DEVICE_REGISTRATION_VERSION,
        registeredCommandsKeys: [],
      },
    });
    fxa._internal.device._registerOrUpdateDevice = function() {
      spy.count += 1;
      return Promise.resolve("bar");
    };

    const result = await fxa.device.getLocalId();

    Assert.equal(spy.count, 0);
    Assert.equal(result, "foo's device id");
    await fxa.signOut(true);
  }
);

add_task(
  async function test_getDeviceId_with_device_id_and_with_no_registration_version_invokes_device_registration() {
    const credentials = getTestUser("foo");
    credentials.verified = true;
    const fxa = await MockFxAccounts(credentials);

    const spy = { count: 0, args: [] };
    fxa._internal.currentAccountState.getUserAccountData = () =>
      Promise.resolve({ device: { id: "wibble" } });
    fxa._internal.device._registerOrUpdateDevice = function() {
      spy.count += 1;
      spy.args.push(arguments);
      return Promise.resolve("wibble");
    };

    const result = await fxa.device.getLocalId();

    Assert.equal(spy.count, 1);
    Assert.equal(spy.args[0].length, 2);
    Assert.equal(spy.args[0][1].device.id, "wibble");
    Assert.equal(result, "wibble");
    await fxa.signOut(true);
  }
);

add_task(async function test_verification_updates_registration() {
  const deviceName = "foo";

  const credentials = getTestUser("baz");
  const fxa = await MockFxAccounts(credentials, {
    id: "device-id",
    name: deviceName,
  });

  // We should already have a device registration, but without send-tab due to
  // our inability to fetch keys for an unverified users.
  const state = fxa._internal.currentAccountState;
  const { device } = await state.getUserAccountData();
  Assert.equal(device.registeredCommandsKeys.length, 0);

  let updatePromise = new Promise(resolve => {
    const old_registerOrUpdateDevice = fxa.device._registerOrUpdateDevice.bind(
      fxa.device
    );
    fxa.device._registerOrUpdateDevice = async function(
      currentState,
      signedInUser
    ) {
      await old_registerOrUpdateDevice(currentState, signedInUser);
      fxa.device._registerOrUpdateDevice = old_registerOrUpdateDevice;
      resolve();
    };
  });

  fxa._internal.checkEmailStatus = async function(sessionToken) {
    credentials.verified = true;
    return credentials;
  };

  await updatePromise;

  const {
    device: newDevice,
    encryptedSendTabKeys,
  } = await state.getUserAccountData();
  Assert.equal(newDevice.registeredCommandsKeys.length, 1);
  Assert.notEqual(encryptedSendTabKeys, null);
  await fxa.signOut(true);
});

add_task(async function test_devicelist_pushendpointexpired() {
  const deviceId = "mydeviceid";
  const credentials = getTestUser("baz");
  credentials.verified = true;
  const fxa = await MockFxAccounts(credentials);
  await updateUserAccountData(fxa, {
    uid: credentials.uid,
    device: {
      id: deviceId,
      registeredCommandsKeys: [],
      registrationVersion: 1, // < 42
    },
  });

  const spy = {
    updateDevice: { count: 0, args: [] },
    getDeviceList: { count: 0, args: [] },
  };
  const client = fxa._internal.fxAccountsClient;
  client.updateDevice = function() {
    spy.updateDevice.count += 1;
    spy.updateDevice.args.push(arguments);
    return Promise.resolve({});
  };
  client.getDeviceList = function() {
    spy.getDeviceList.count += 1;
    spy.getDeviceList.args.push(arguments);
    return Promise.resolve([
      {
        id: "mydeviceid",
        name: "foo",
        type: "desktop",
        isCurrentDevice: true,
        pushEndpointExpired: true,
        pushCallback: "https://example.com",
      },
    ]);
  };
  let polledForMissedCommands = false;
  fxa._internal.commands.pollDeviceCommands = () => {
    polledForMissedCommands = true;
  };

  await fxa.device.refreshDeviceList();

  Assert.equal(spy.getDeviceList.count, 1);
  Assert.equal(spy.updateDevice.count, 1);
  Assert.ok(polledForMissedCommands);
  await fxa.signOut(true);
});

add_task(async function test_devicelist_nopushcallback() {
  const deviceId = "mydeviceid";
  const credentials = getTestUser("baz");
  credentials.verified = true;
  const fxa = await MockFxAccounts(credentials);
  await updateUserAccountData(fxa, {
    uid: credentials.uid,
    device: {
      id: deviceId,
      registeredCommandsKeys: [],
      registrationVersion: 1,
    },
  });

  const spy = {
    updateDevice: { count: 0, args: [] },
    getDeviceList: { count: 0, args: [] },
  };
  const client = fxa._internal.fxAccountsClient;
  client.updateDevice = function() {
    spy.updateDevice.count += 1;
    spy.updateDevice.args.push(arguments);
    return Promise.resolve({});
  };
  client.getDeviceList = function() {
    spy.getDeviceList.count += 1;
    spy.getDeviceList.args.push(arguments);
    return Promise.resolve([
      {
        id: "mydeviceid",
        name: "foo",
        type: "desktop",
        isCurrentDevice: true,
        pushEndpointExpired: false,
        pushCallback: null,
      },
    ]);
  };

  let polledForMissedCommands = false;
  fxa._internal.commands.pollDeviceCommands = () => {
    polledForMissedCommands = true;
  };

  await fxa.device.refreshDeviceList();

  Assert.equal(spy.getDeviceList.count, 1);
  Assert.equal(spy.updateDevice.count, 1);
  Assert.ok(polledForMissedCommands);
  await fxa.signOut(true);
});

add_task(async function test_refreshDeviceList() {
  let credentials = getTestUser("baz");

  let storage = new MockStorageManager();
  storage.initialize(credentials);
  let state = new AccountState(storage);

  let fxAccountsClient = new MockFxAccountsClient({
    id: "deviceAAAAAA",
    name: "iPhone",
    type: "phone",
    sessionToken: credentials.sessionToken,
  });
  let spy = {
    getDeviceList: { count: 0 },
  };
  fxAccountsClient.getDeviceList = (function(old) {
    return function getDeviceList() {
      spy.getDeviceList.count += 1;
      return old.apply(this, arguments);
    };
  })(fxAccountsClient.getDeviceList);
  let fxai = {
    _now: Date.now(),
    _generation: 0,
    fxAccountsClient,
    now() {
      return this._now;
    },
    withVerifiedAccountState(func) {
      // Ensure `func` is called asynchronously, and simulate the possibility
      // of a different user signng in while the promise is in-flight.
      const currentGeneration = this._generation;
      return Promise.resolve()
        .then(_ => func(state))
        .then(result => {
          if (currentGeneration < this._generation) {
            throw new Error("Another user has signed in");
          }
          return result;
        });
    },
    fxaPushService: null,
  };
  let device = new FxAccountsDevice(fxai);
  device._checkRemoteCommandsUpdateNeeded = async () => false;

  Assert.equal(
    device.recentDeviceList,
    null,
    "Should not have device list initially"
  );
  Assert.ok(await device.refreshDeviceList(), "Should refresh list");
  Assert.deepEqual(
    device.recentDeviceList,
    [
      {
        id: "deviceAAAAAA",
        name: "iPhone",
        type: "phone",
        isCurrentDevice: true,
      },
    ],
    "Should fetch device list"
  );
  Assert.equal(
    spy.getDeviceList.count,
    1,
    "Should make request to refresh list"
  );
  Assert.ok(
    !(await device.refreshDeviceList()),
    "Should not refresh device list if fresh"
  );

  fxai._now += device.TIME_BETWEEN_FXA_DEVICES_FETCH_MS;

  let refreshPromise = device.refreshDeviceList();
  let secondRefreshPromise = device.refreshDeviceList();
  Assert.ok(
    await Promise.all([refreshPromise, secondRefreshPromise]),
    "Should refresh list if stale"
  );
  Assert.equal(
    spy.getDeviceList.count,
    2,
    "Should only make one request if called with pending request"
  );

  device.observe(null, ON_DEVICE_CONNECTED_NOTIFICATION);
  await device.refreshDeviceList();
  Assert.equal(
    spy.getDeviceList.count,
    3,
    "Should refresh device list after connecting new device"
  );
  device.observe(
    null,
    ON_DEVICE_DISCONNECTED_NOTIFICATION,
    JSON.stringify({ isLocalDevice: false })
  );
  await device.refreshDeviceList();
  Assert.equal(
    spy.getDeviceList.count,
    4,
    "Should refresh device list after disconnecting device"
  );
  device.observe(
    null,
    ON_DEVICE_DISCONNECTED_NOTIFICATION,
    JSON.stringify({ isLocalDevice: true })
  );
  await device.refreshDeviceList();
  Assert.equal(
    spy.getDeviceList.count,
    4,
    "Should not refresh device list after disconnecting this device"
  );

  let refreshBeforeResetPromise = device.refreshDeviceList({
    ignoreCached: true,
  });
  fxai._generation++;
  await Assert.rejects(refreshBeforeResetPromise, /Another user has signed in/);

  device.reset();
  Assert.equal(
    device.recentDeviceList,
    null,
    "Should clear device list after resetting"
  );
  Assert.ok(
    await device.refreshDeviceList(),
    "Should fetch new list after resetting"
  );
});

add_task(async function test_checking_remote_availableCommands_mismatch() {
  const credentials = getTestUser("baz");
  credentials.verified = true;
  const fxa = await MockFxAccounts(credentials);
  fxa.device._checkRemoteCommandsUpdateNeeded =
    FxAccountsDevice.prototype._checkRemoteCommandsUpdateNeeded;
  fxa.commands.availableCommands = async () => {
    return {
      "https://identity.mozilla.com/cmd/open-uri": "local-keys",
    };
  };

  const ourDevice = {
    isCurrentDevice: true,
    availableCommands: {
      "https://identity.mozilla.com/cmd/open-uri": "remote-keys",
    },
  };
  Assert.ok(
    await fxa.device._checkRemoteCommandsUpdateNeeded(
      ourDevice.availableCommands
    )
  );
});

add_task(async function test_checking_remote_availableCommands_match() {
  const credentials = getTestUser("baz");
  credentials.verified = true;
  const fxa = await MockFxAccounts(credentials);
  fxa.device._checkRemoteCommandsUpdateNeeded =
    FxAccountsDevice.prototype._checkRemoteCommandsUpdateNeeded;
  fxa.commands.availableCommands = async () => {
    return {
      "https://identity.mozilla.com/cmd/open-uri": "local-keys",
    };
  };

  const ourDevice = {
    isCurrentDevice: true,
    availableCommands: {
      "https://identity.mozilla.com/cmd/open-uri": "local-keys",
    },
  };
  Assert.ok(
    !(await fxa.device._checkRemoteCommandsUpdateNeeded(
      ourDevice.availableCommands
    ))
  );
});

function getTestUser(name) {
  return {
    email: name + "@example.com",
    uid: "1ad7f502-4cc7-4ec1-a209-071fd2fae348",
    sessionToken: name + "'s session token",
    verified: false,
    ...MOCK_ACCOUNT_KEYS,
  };
}
