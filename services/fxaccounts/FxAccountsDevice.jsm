/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const {
  log,
  ERRNO_DEVICE_SESSION_CONFLICT,
  ERRNO_UNKNOWN_DEVICE,
  ON_NEW_DEVICE_ID,
  ON_DEVICELIST_UPDATED,
  ON_DEVICE_CONNECTED_NOTIFICATION,
  ON_DEVICE_DISCONNECTED_NOTIFICATION,
  ONVERIFIED_NOTIFICATION,
  PREF_ACCOUNT_ROOT,
} = ChromeUtils.import("resource://gre/modules/FxAccountsCommon.js");

const { DEVICE_TYPE_DESKTOP } = ChromeUtils.import(
  "resource://services-sync/constants.js"
);

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "CommonUtils",
  "resource://services-common/utils.js"
);

const PREF_LOCAL_DEVICE_NAME = PREF_ACCOUNT_ROOT + "device.name";
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "pref_localDeviceName",
  PREF_LOCAL_DEVICE_NAME,
  ""
);

const PREF_DEPRECATED_DEVICE_NAME = "services.sync.client.name";

// Sanitizes all characters which the FxA server considers invalid, replacing
// them with the unicode replacement character.
// At time of writing, FxA has a regex DISPLAY_SAFE_UNICODE_WITH_NON_BMP, which
// the regex below is based on.
// eslint-disable-next-line no-control-regex
const INVALID_NAME_CHARS = /[\u0000-\u001F\u007F\u0080-\u009F\u2028-\u2029\uE000-\uF8FF\uFFF9-\uFFFC\uFFFE-\uFFFF]/g;
const MAX_NAME_LEN = 255;
const REPLACEMENT_CHAR = "\uFFFD";

function sanitizeDeviceName(name) {
  return name
    .substr(0, MAX_NAME_LEN)
    .replace(INVALID_NAME_CHARS, REPLACEMENT_CHAR);
}

// Everything to do with FxA devices.
class FxAccountsDevice {
  constructor(fxai) {
    this._fxai = fxai;
    this._deviceListCache = null;
    this._fetchAndCacheDeviceListPromise = null;

    // The current version of the device registration, we use this to re-register
    // devices after we update what we send on device registration.
    this.DEVICE_REGISTRATION_VERSION = 2;

    // This is to avoid multiple sequential syncs ending up calling
    // this expensive endpoint multiple times in a row.
    this.TIME_BETWEEN_FXA_DEVICES_FETCH_MS = 1 * 60 * 1000; // 1 minute

    // Invalidate our cached device list when a device is connected or disconnected.
    Services.obs.addObserver(this, ON_DEVICE_CONNECTED_NOTIFICATION, true);
    Services.obs.addObserver(this, ON_DEVICE_DISCONNECTED_NOTIFICATION, true);
    // A user becoming verified probably means we need to re-register the device
    // because we are now able to get the sendtab keys.
    Services.obs.addObserver(this, ONVERIFIED_NOTIFICATION, true);
  }

  async getLocalId() {
    return this._withCurrentAccountState(currentState => {
      // It turns out _updateDeviceRegistrationIfNecessary() does exactly what we
      // need.
      return this._updateDeviceRegistrationIfNecessary(currentState);
    });
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
    return sanitizeDeviceName(
      syncStrings.formatStringFromName("client.name2", [
        user,
        brandName,
        system,
      ])
    );
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
    let name = lazy.pref_localDeviceName;
    if (!name) {
      name = this.getDefaultLocalName();
      Services.prefs.setStringPref(PREF_LOCAL_DEVICE_NAME, name);
    }
    // We need to sanitize here because some names were generated before we
    // started sanitizing.
    return sanitizeDeviceName(name);
  }

  setLocalName(newName) {
    Services.prefs.clearUserPref(PREF_DEPRECATED_DEVICE_NAME);
    Services.prefs.setStringPref(
      PREF_LOCAL_DEVICE_NAME,
      sanitizeDeviceName(newName)
    );
    // Update the registration in the background.
    this.updateDeviceRegistration().catch(error => {
      log.warn("failed to update fxa device registration", error);
    });
  }

  getLocalType() {
    return DEVICE_TYPE_DESKTOP;
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
    // If we're already refreshing the list in the background, let that finish.
    if (this._fetchAndCacheDeviceListPromise) {
      log.info("Already fetching device list, return existing promise");
      return this._fetchAndCacheDeviceListPromise;
    }

    // If the cache is fresh enough, don't refresh it again.
    if (!ignoreCached && this._deviceListCache) {
      const ageOfCache = this._fxai.now() - this._deviceListCache.lastFetch;
      if (ageOfCache < this.TIME_BETWEEN_FXA_DEVICES_FETCH_MS) {
        log.info("Device list cache is fresh, re-using it");
        return false;
      }
    }

    log.info("fetching updated device list");
    this._fetchAndCacheDeviceListPromise = (async () => {
      try {
        const devices = await this._withVerifiedAccountState(
          async currentState => {
            const accountData = await currentState.getUserAccountData([
              "sessionToken",
              "device",
            ]);
            const devices = await this._fxai.fxAccountsClient.getDeviceList(
              accountData.sessionToken
            );
            log.info(
              `Got new device list: ${devices.map(d => d.id).join(", ")}`
            );

            await this._refreshRemoteDevice(currentState, accountData, devices);
            return devices;
          }
        );
        log.info("updating the cache");
        // Be careful to only update the cache once the above has resolved, so
        // we know that the current account state didn't change underneath us.
        this._deviceListCache = {
          lastFetch: this._fxai.now(),
          devices,
        };
        Services.obs.notifyObservers(null, ON_DEVICELIST_UPDATED);
        return true;
      } finally {
        this._fetchAndCacheDeviceListPromise = null;
      }
    })();
    return this._fetchAndCacheDeviceListPromise;
  }

  async _refreshRemoteDevice(currentState, accountData, remoteDevices) {
    // Check if our push registration previously succeeded and is still
    // good (although background device registration means it's possible
    // we'll be fetching the device list before we've actually
    // registered ourself!)
    // (For a missing subscription we check for an explicit 'null' -
    // both to help tests and as a safety valve - missing might mean
    // "no push available" for self-hosters or similar?)
    const ourDevice = remoteDevices.find(device => device.isCurrentDevice);
    if (
      ourDevice &&
      (ourDevice.pushCallback === null || ourDevice.pushEndpointExpired)
    ) {
      log.warn(`Our push endpoint needs resubscription`);
      await this._fxai.fxaPushService.unsubscribe();
      await this._registerOrUpdateDevice(currentState, accountData);
      // and there's a reasonable chance there are commands waiting.
      await this._fxai.commands.pollDeviceCommands();
    } else if (
      ourDevice &&
      (await this._checkRemoteCommandsUpdateNeeded(ourDevice.availableCommands))
    ) {
      log.warn(`Our commands need to be updated on the server`);
      await this._registerOrUpdateDevice(currentState, accountData);
    }
  }

  async updateDeviceRegistration() {
    return this._withCurrentAccountState(async currentState => {
      const signedInUser = await currentState.getUserAccountData([
        "sessionToken",
        "device",
      ]);
      if (signedInUser) {
        await this._registerOrUpdateDevice(currentState, signedInUser);
      }
    });
  }

  async updateDeviceRegistrationIfNecessary() {
    return this._withCurrentAccountState(currentState => {
      return this._updateDeviceRegistrationIfNecessary(currentState);
    });
  }

  reset() {
    this._deviceListCache = null;
    this._fetchAndCacheDeviceListPromise = null;
  }

  /**
   * Here begin our internal helper methods.
   *
   * Many of these methods take the current account state as first argument,
   * in order to avoid racing our state updates with e.g. the uer signing
   * out while we're in the middle of an update. If this does happen, the
   * resulting promise will be rejected rather than persisting stale state.
   *
   */

  _withCurrentAccountState(func) {
    return this._fxai.withCurrentAccountState(async currentState => {
      try {
        return await func(currentState);
      } catch (err) {
        // `_handleTokenError` always throws, this syntax keeps the linter happy.
        // TODO: probably `_handleTokenError` could be done by `_fxai.withCurrentAccountState`
        // internally rather than us having to remember to do it here.
        throw await this._fxai._handleTokenError(err);
      }
    });
  }

  _withVerifiedAccountState(func) {
    return this._fxai.withVerifiedAccountState(async currentState => {
      try {
        return await func(currentState);
      } catch (err) {
        // `_handleTokenError` always throws, this syntax keeps the linter happy.
        throw await this._fxai._handleTokenError(err);
      }
    });
  }

  async _checkDeviceUpdateNeeded(device) {
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
      !lazy.CommonUtils.arrayEqual(
        device.registeredCommandsKeys,
        availableCommandsKeys
      )
    );
  }

  async _checkRemoteCommandsUpdateNeeded(remoteAvailableCommands) {
    if (!remoteAvailableCommands) {
      return true;
    }
    const remoteAvailableCommandsKeys = Object.keys(
      remoteAvailableCommands
    ).sort();
    const localAvailableCommands = await this._fxai.commands.availableCommands();
    const localAvailableCommandsKeys = Object.keys(
      localAvailableCommands
    ).sort();

    if (
      !lazy.CommonUtils.arrayEqual(
        localAvailableCommandsKeys,
        remoteAvailableCommandsKeys
      )
    ) {
      return true;
    }

    for (const key of localAvailableCommandsKeys) {
      if (remoteAvailableCommands[key] !== localAvailableCommands[key]) {
        return true;
      }
    }
    return false;
  }

  async _updateDeviceRegistrationIfNecessary(currentState) {
    let data = await currentState.getUserAccountData([
      "sessionToken",
      "device",
    ]);
    if (!data) {
      // Can't register a device without a signed-in user.
      return null;
    }
    const { device } = data;
    if (await this._checkDeviceUpdateNeeded(device)) {
      return this._registerOrUpdateDevice(currentState, data);
    }
    // Return the device ID we already had.
    return device.id;
  }

  // If you change what we send to the FxA servers during device registration,
  // you'll have to bump the DEVICE_REGISTRATION_VERSION number to force older
  // devices to re-register when Firefox updates.
  async _registerOrUpdateDevice(currentState, signedInUser) {
    // This method has the side-effect of setting some account-related prefs
    // (e.g. for caching the device name) so it's important we don't execute it
    // if the signed-in state has changed.
    if (!currentState.isCurrent) {
      throw new Error(
        "_registerOrUpdateDevice called after a different user has signed in"
      );
    }

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
      log.info("registering with available commands", availableCommandsKeys);

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
      let { device: deviceProps } = await currentState.getUserAccountData([
        "device",
      ]);
      await currentState.updateUserAccountData({
        device: {
          ...deviceProps, // Copy the other properties (e.g. handledCommands).
          id: device.id,
          registrationVersion: this.DEVICE_REGISTRATION_VERSION,
          registeredCommandsKeys: availableCommandsKeys,
        },
      });
      return device.id;
    } catch (error) {
      return this._handleDeviceError(currentState, error, sessionToken);
    }
  }

  async _handleDeviceError(currentState, error, sessionToken) {
    try {
      if (error.code === 400) {
        if (error.errno === ERRNO_UNKNOWN_DEVICE) {
          return this._recoverFromUnknownDevice(currentState);
        }

        if (error.errno === ERRNO_DEVICE_SESSION_CONFLICT) {
          return this._recoverFromDeviceSessionConflict(
            currentState,
            error,
            sessionToken
          );
        }
      }

      // `_handleTokenError` always throws, this syntax keeps the linter happy.
      // Note that the re-thrown error is immediately caught, logged and ignored
      // by the containing scope here, which is why we have to `_handleTokenError`
      // ourselves rather than letting it bubble up for handling by the caller.
      throw await this._fxai._handleTokenError(error);
    } catch (error) {
      await this._logErrorAndResetDeviceRegistrationVersion(
        currentState,
        error
      );
      return null;
    }
  }

  async _recoverFromUnknownDevice(currentState) {
    // FxA did not recognise the device id. Handle it by clearing the device
    // id on the account data. At next sync or next sign-in, registration is
    // retried and should succeed.
    log.warn("unknown device id, clearing the local device data");
    try {
      await currentState.updateUserAccountData({
        device: null,
        encryptedSendTabKeys: null,
      });
    } catch (error) {
      await this._logErrorAndResetDeviceRegistrationVersion(
        currentState,
        error
      );
    }
    return null;
  }

  async _recoverFromDeviceSessionConflict(currentState, error, sessionToken) {
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
        await currentState.updateUserAccountData({
          device: {
            id: deviceId,
            registrationVersion: null,
          },
          encryptedSendTabKeys: null,
        });
        return deviceId;
      }
      if (length > 1) {
        log.error(
          "insane server state, " + length + " devices for this session"
        );
      }
      await this._logErrorAndResetDeviceRegistrationVersion(
        currentState,
        error
      );
    } catch (secondError) {
      log.error("failed to recover from device-session conflict", secondError);
      await this._logErrorAndResetDeviceRegistrationVersion(
        currentState,
        error
      );
    }
    return null;
  }

  async _logErrorAndResetDeviceRegistrationVersion(currentState, error) {
    // Device registration should never cause other operations to fail.
    // If we've reached this point, just log the error and reset the device
    // on the account data. At next sync or next sign-in,
    // registration will be retried.
    log.error("device registration failed", error);
    try {
      await currentState.updateUserAccountData({
        device: null,
        encryptedSendTabKeys: null,
      });
    } catch (secondError) {
      log.error(
        "failed to reset the device registration version, device registration won't be retried",
        secondError
      );
    }
  }

  // Kick off a background refresh when a device is connected or disconnected.
  observe(subject, topic, data) {
    switch (topic) {
      case ON_DEVICE_CONNECTED_NOTIFICATION:
        this.refreshDeviceList({ ignoreCached: true }).catch(error => {
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
          this.refreshDeviceList({ ignoreCached: true }).catch(error => {
            log.warn(
              "failed to refresh devices after disconnecting a device",
              error
            );
          });
        }
        break;
      case ONVERIFIED_NOTIFICATION:
        this.updateDeviceRegistrationIfNecessary().catch(error => {
          log.warn(
            "updateDeviceRegistrationIfNecessary failed after verification",
            error
          );
        });
        break;
    }
  }
}

FxAccountsDevice.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIObserver",
  "nsISupportsWeakReference",
]);

function urlsafeBase64Encode(buffer) {
  return ChromeUtils.base64URLEncode(new Uint8Array(buffer), { pad: false });
}

var EXPORTED_SYMBOLS = ["FxAccountsDevice"];
