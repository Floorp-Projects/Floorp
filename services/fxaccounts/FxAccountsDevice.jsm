/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const {
  log,
  ERRNO_DEVICE_SESSION_CONFLICT,
  ERRNO_UNKNOWN_DEVICE,
  ON_NEW_DEVICE_ID,
  ON_DEVICE_CONNECTED_NOTIFICATION,
  ON_DEVICE_DISCONNECTED_NOTIFICATION,
} = ChromeUtils.import("resource://gre/modules/FxAccountsCommon.js");

const { DEVICE_TYPE_DESKTOP } = ChromeUtils.import(
  "resource://services-sync/constants.js"
);

const { PREF_ACCOUNT_ROOT } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsCommon.js"
);

ChromeUtils.defineModuleGetter(
  this,
  "CommonUtils",
  "resource://services-common/utils.js"
);

const PREF_LOCAL_DEVICE_NAME = PREF_ACCOUNT_ROOT + "device.name";
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "pref_localDeviceName",
  PREF_LOCAL_DEVICE_NAME,
  ""
);

const PREF_DEPRECATED_DEVICE_NAME = "services.sync.client.name";

// Everything to do with FxA devices.
class FxAccountsDevice {
  constructor(fxai) {
    this._fxai = fxai;
    this._deviceListCache = null;

    // The generation avoids a race where we'll cache a stale device list if the
    // user signs out during a background refresh. It works like this: during a
    // refresh, we store the current generation, fetch the new list from the
    // server, and compare the stored generation to the current one. Since we
    // increment the generation on reset, we know that the fetched list isn't
    // valid if the generations are different.
    this._generation = 0;

    // The current version of the device registration, we use this to re-register
    // devices after we update what we send on device registration.
    this.DEVICE_REGISTRATION_VERSION = 2;

    // This is to avoid multiple sequential syncs ending up calling
    // this expensive endpoint multiple times in a row.
    this.TIME_BETWEEN_FXA_DEVICES_FETCH_MS = 1 * 60 * 1000; // 1 minute

    // Invalidate our cached device list when a device is connected or disconnected.
    Services.obs.addObserver(this, ON_DEVICE_CONNECTED_NOTIFICATION, true);
    Services.obs.addObserver(this, ON_DEVICE_DISCONNECTED_NOTIFICATION, true);
  }

  async getLocalId() {
    let data = await this._fxai.currentAccountState.getUserAccountData();
    if (!data) {
      // Without a signed-in user, there can be no device id.
      return null;
    }
    const { device } = data;
    if (await this.checkDeviceUpdateNeeded(device)) {
      return this._registerOrUpdateDevice(data);
    }
    // Return the device id that we already registered with the server.
    return device.id;
  }

  // Generate a client name if we don't have a useful one yet
  getDefaultLocalName() {
    let env = Cc["@mozilla.org/process/environment;1"].getService(
      Ci.nsIEnvironment
    );
    let user = env.get("USER") || env.get("USERNAME");
    // Note that we used to fall back to the "services.sync.username" pref here,
    // but that's no longer suitable in a world where sync might not be
    // configured. However, we almost never *actually* fell back to that, and
    // doing so sanely here would mean making this function async, which we don't
    // really want to do yet.

    // A little hack for people using the the moz-build environment on Windows
    // which sets USER to the literal "%USERNAME%" (yes, really)
    if (user == "%USERNAME%" && env.get("USERNAME")) {
      user = env.get("USERNAME");
    }

    let brand = Services.strings.createBundle(
      "chrome://branding/locale/brand.properties"
    );
    let brandName;
    try {
      brandName = brand.GetStringFromName("brandShortName");
    } catch (O_o) {
      // this only fails in tests and markh can't work out why :(
      brandName = Services.appinfo.name;
    }

    // The DNS service may fail to provide a hostname in edge-cases we don't
    // fully understand - bug 1391488.
    let hostname;
    try {
      // hostname of the system, usually assigned by the user or admin
      hostname = Cc["@mozilla.org/network/dns-service;1"].getService(
        Ci.nsIDNSService
      ).myHostName;
    } catch (ex) {
      Cu.reportError(ex);
    }
    let system =
      // 'device' is defined on unix systems
      Services.sysinfo.get("device") ||
      hostname ||
      // fall back on ua info string
      Cc["@mozilla.org/network/protocol;1?name=http"].getService(
        Ci.nsIHttpProtocolHandler
      ).oscpu;

    // It's a little unfortunate that this string is defined as being weave/sync,
    // but it's not worth moving it.
    let syncStrings = Services.strings.createBundle(
      "chrome://weave/locale/sync.properties"
    );
    return syncStrings.formatStringFromName("client.name2", [
      user,
      brandName,
      system,
    ]);
  }

  getLocalName() {
    // We used to store this in services.sync.client.name, but now store it
    // under an fxa-specific location.
    let deprecated_value = Services.prefs.getStringPref(
      PREF_DEPRECATED_DEVICE_NAME,
      ""
    );
    if (deprecated_value) {
      Services.prefs.setStringPref(PREF_LOCAL_DEVICE_NAME, deprecated_value);
      Services.prefs.clearUserPref(PREF_DEPRECATED_DEVICE_NAME);
    }
    let name = pref_localDeviceName;
    if (!name) {
      name = this.getDefaultLocalName();
      Services.prefs.setStringPref(PREF_LOCAL_DEVICE_NAME, name);
    }
    return name;
  }

  setLocalName(newName) {
    Services.prefs.clearUserPref(PREF_DEPRECATED_DEVICE_NAME);
    Services.prefs.setStringPref(PREF_LOCAL_DEVICE_NAME, newName);
    // Update the registration in the background.
    this.updateDeviceRegistration().catch(error => {
      log.warn("failed to update fxa device registration", error);
    });
  }

  getLocalType() {
    return DEVICE_TYPE_DESKTOP;
  }

  async checkDeviceUpdateNeeded(device) {
    // There is no device registered or the device registration is outdated.
    // Either way, we should register the device with FxA
    // before returning the id to the caller.
    const availableCommandsKeys = Object.keys(
      await this._fxai.commands.availableCommands()
    ).sort();
    return (
      !device ||
      !device.registrationVersion ||
      device.registrationVersion < this.DEVICE_REGISTRATION_VERSION ||
      !device.registeredCommandsKeys ||
      !CommonUtils.arrayEqual(
        device.registeredCommandsKeys,
        availableCommandsKeys
      )
    );
  }

  /**
   * Returns the most recently fetched device list, or `null` if the list
   * hasn't been fetched yet. This is synchronous, so that consumers like
   * Send Tab can render the device list right away, without waiting for
   * it to refresh.
   *
   * @type {?Array}
   */
  get recentDeviceList() {
    return this._deviceListCache ? this._deviceListCache.devices : null;
  }

  /**
   * Refreshes the device list. After this function returns, consumers can
   * access the new list using the `recentDeviceList` getter. Note that
   * multiple concurrent calls to `refreshDeviceList` will only refresh the
   * list once.
   *
   * @param  {Boolean} [options.ignoreCached]
   *         If `true`, forces a refresh, even if the cached device list is
   *         still fresh. Defaults to `false`.
   * @return {Promise<Boolean>}
   *         `true` if the list was refreshed, `false` if the cached list is
   *         fresh. Rejects if an error occurs refreshing the list or device
   *         push registration.
   */
  async refreshDeviceList({ ignoreCached = false } = {}) {
    if (this._fetchAndCacheDeviceListPromise) {
      // If we're already refreshing the list in the background, let that
      // finish.
      return this._fetchAndCacheDeviceListPromise;
    }
    if (ignoreCached || !this._deviceListCache) {
      return this._fetchAndCacheDeviceList();
    }
    if (
      this._fxai.now() - this._deviceListCache.lastFetch <
      this.TIME_BETWEEN_FXA_DEVICES_FETCH_MS
    ) {
      // If our recent device list is still fresh, skip the request to
      // refresh it.
      return false;
    }
    return this._fetchAndCacheDeviceList();
  }

  async _fetchAndCacheDeviceList() {
    if (this._fetchAndCacheDeviceListPromise) {
      return this._fetchAndCacheDeviceListPromise;
    }
    let generation = this._generation;
    return (this._fetchAndCacheDeviceListPromise = this._fxai
      .withVerifiedAccountState(async state => {
        let accountData = await state.getUserAccountData([
          "sessionToken",
          "device",
        ]);

        let devices = await this._fxai.fxAccountsClient.getDeviceList(
          accountData.sessionToken
        );
        if (generation != this._generation) {
          throw new Error("Another user has signed in");
        }
        this._deviceListCache = {
          lastFetch: this._fxai.now(),
          devices,
        };

        // Check if our push registration is still good.
        const ourDevice = devices.find(device => device.isCurrentDevice);
        if (ourDevice.pushEndpointExpired) {
          await this._fxai.fxaPushService.unsubscribe();
          await this._registerOrUpdateDevice(accountData);
        }

        return true;
      })
      .finally(_ => {
        this._fetchAndCacheDeviceListPromise = null;
      }));
  }

  async updateDeviceRegistration() {
    try {
      const signedInUser = await this._fxai.currentAccountState.getUserAccountData();
      if (signedInUser) {
        await this._registerOrUpdateDevice(signedInUser);
      }
    } catch (error) {
      await this._logErrorAndResetDeviceRegistrationVersion(error);
    }
  }

  // If you change what we send to the FxA servers during device registration,
  // you'll have to bump the DEVICE_REGISTRATION_VERSION number to force older
  // devices to re-register when Firefox updates
  async _registerOrUpdateDevice(signedInUser) {
    const { sessionToken, device: currentDevice } = signedInUser;
    if (!sessionToken) {
      throw new Error("_registerOrUpdateDevice called without a session token");
    }

    try {
      const subscription = await this._fxai.fxaPushService.registerPushEndpoint();
      const deviceName = this.getLocalName();
      let deviceOptions = {};

      // if we were able to obtain a subscription
      if (subscription && subscription.endpoint) {
        deviceOptions.pushCallback = subscription.endpoint;
        let publicKey = subscription.getKey("p256dh");
        let authKey = subscription.getKey("auth");
        if (publicKey && authKey) {
          deviceOptions.pushPublicKey = urlsafeBase64Encode(publicKey);
          deviceOptions.pushAuthKey = urlsafeBase64Encode(authKey);
        }
      }
      deviceOptions.availableCommands = await this._fxai.commands.availableCommands();
      const availableCommandsKeys = Object.keys(
        deviceOptions.availableCommands
      ).sort();

      let device;
      if (currentDevice && currentDevice.id) {
        log.debug("updating existing device details");
        device = await this._fxai.fxAccountsClient.updateDevice(
          sessionToken,
          currentDevice.id,
          deviceName,
          deviceOptions
        );
      } else {
        log.debug("registering new device details");
        device = await this._fxai.fxAccountsClient.registerDevice(
          sessionToken,
          deviceName,
          this.getLocalType(),
          deviceOptions
        );
        Services.obs.notifyObservers(null, ON_NEW_DEVICE_ID);
      }

      // Get the freshest device props before updating them.
      let {
        device: deviceProps,
      } = await this._fxai.currentAccountState.getUserAccountData();
      await this._fxai.currentAccountState.updateUserAccountData({
        device: {
          ...deviceProps, // Copy the other properties (e.g. handledCommands).
          id: device.id,
          registrationVersion: this.DEVICE_REGISTRATION_VERSION,
          registeredCommandsKeys: availableCommandsKeys,
        },
      });
      return device.id;
    } catch (error) {
      return this._handleDeviceError(error, sessionToken);
    }
  }

  _handleDeviceError(error, sessionToken) {
    return Promise.resolve()
      .then(() => {
        if (error.code === 400) {
          if (error.errno === ERRNO_UNKNOWN_DEVICE) {
            return this._recoverFromUnknownDevice();
          }

          if (error.errno === ERRNO_DEVICE_SESSION_CONFLICT) {
            return this._recoverFromDeviceSessionConflict(error, sessionToken);
          }
        }

        // `_handleTokenError` re-throws the error.
        return this._fxai._handleTokenError(error);
      })
      .catch(error => this._logErrorAndResetDeviceRegistrationVersion(error))
      .catch(() => {});
  }

  async _recoverFromUnknownDevice() {
    // FxA did not recognise the device id. Handle it by clearing the device
    // id on the account data. At next sync or next sign-in, registration is
    // retried and should succeed.
    log.warn("unknown device id, clearing the local device data");
    try {
      await this._fxai.currentAccountState.updateUserAccountData({
        device: null,
      });
    } catch (error) {
      await this._logErrorAndResetDeviceRegistrationVersion(error);
    }
  }

  async _recoverFromDeviceSessionConflict(error, sessionToken) {
    // FxA has already associated this session with a different device id.
    // Perhaps we were beaten in a race to register. Handle the conflict:
    //   1. Fetch the list of devices for the current user from FxA.
    //   2. Look for ourselves in the list.
    //   3. If we find a match, set the correct device id and device registration
    //      version on the account data and return the correct device id. At next
    //      sync or next sign-in, registration is retried and should succeed.
    //   4. If we don't find a match, log the original error.
    log.warn(
      "device session conflict, attempting to ascertain the correct device id"
    );
    try {
      const devices = await this._fxai.fxAccountsClient.getDeviceList(
        sessionToken
      );
      const matchingDevices = devices.filter(device => device.isCurrentDevice);
      const length = matchingDevices.length;
      if (length === 1) {
        const deviceId = matchingDevices[0].id;
        await this._fxai.currentAccountState.updateUserAccountData({
          device: {
            id: deviceId,
            registrationVersion: null,
          },
        });
        return deviceId;
      }
      if (length > 1) {
        log.error(
          "insane server state, " + length + " devices for this session"
        );
      }
      await this._logErrorAndResetDeviceRegistrationVersion(error);
    } catch (secondError) {
      log.error("failed to recover from device-session conflict", secondError);
      await this._logErrorAndResetDeviceRegistrationVersion(error);
    }
    return null;
  }

  async _logErrorAndResetDeviceRegistrationVersion(error) {
    // Device registration should never cause other operations to fail.
    // If we've reached this point, just log the error and reset the device
    // on the account data. At next sync or next sign-in,
    // registration will be retried.
    log.error("device registration failed", error);
    try {
      this._fxai.currentAccountState.updateUserAccountData({
        device: null,
      });
    } catch (secondError) {
      log.error(
        "failed to reset the device registration version, device registration won't be retried",
        secondError
      );
    }
  }

  reset() {
    this._deviceListCache = null;
    this._generation++;
    this._fetchAndCacheDeviceListPromise = null;
  }

  // Kick off a background refresh when a device is connected or disconnected.
  observe(subject, topic, data) {
    switch (topic) {
      case ON_DEVICE_CONNECTED_NOTIFICATION:
        this._fetchAndCacheDeviceList().catch(error => {
          log.warn(
            "failed to refresh devices after connecting a new device",
            error
          );
        });
        break;
      case ON_DEVICE_DISCONNECTED_NOTIFICATION:
        let json = JSON.parse(data);
        if (!json.isLocalDevice) {
          // If we're the device being disconnected, don't bother fetching a new
          // list, since our session token is now invalid.
          this._fetchAndCacheDeviceList().catch(error => {
            log.warn(
              "failed to refresh devices after disconnecting a device",
              error
            );
          });
        }
        break;
    }
  }
}

FxAccountsDevice.prototype.QueryInterface = ChromeUtils.generateQI([
  Ci.nsIObserver,
  Ci.nsISupportsWeakReference,
]);

function urlsafeBase64Encode(buffer) {
  return ChromeUtils.base64URLEncode(new Uint8Array(buffer), { pad: false });
}

var EXPORTED_SYMBOLS = ["FxAccountsDevice"];
