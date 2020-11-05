/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["RustFxAccount"];

/**
 * This class is a low-level JS wrapper around the `mozIFirefoxAccountsBridge`
 * interface.
 * A `RustFxAccount` instance can be associated to 0 or 1 Firefox Account depending
 * on its login state.
 * This class responsibilities are to:
 * - Expose an async JS interface to the methods in `mozIFirefoxAccountsBridge` by
 *   converting the callbacks-driven routines into proper JS promises.
 * - Serialize and deserialize the input and outputs of `mozIFirefoxAccountsBridge`.
 *   Complex objects are generally returned through JSON strings.
 */
class RustFxAccount {
  /**
   * Create a new `RustFxAccount` instance, depending on the argument passed it could be:
   * - From scratch (object passed).
   * - Restore a previously serialized account (string passed).
   * @param {(Object)|string} options Object type creates a new instance, string type restores an instance from a serialized state obtained with `stateJSON`.
   * @param {string} options.fxaServer Content URL of the remote Firefox Accounts server.
   * @param {string} options.clientId OAuth client_id of the application.
   * @param {string} options.redirectUri Redirection URL to be navigated to at the end of the OAuth login flow.
   * @param {string} [options.tokenServerUrlOverride] Override the token server URL: used by self-hosters of Sync.
   */
  constructor(options) {
    // This initializes the network stack for all the Rust components.
    let viaduct = Cc["@mozilla.org/toolkit/viaduct;1"].createInstance(
      Ci.mozIViaduct
    );
    viaduct.EnsureInitialized();

    this.bridge = Cc[
      "@mozilla.org/services/firefox-accounts-bridge;1"
    ].createInstance(Ci.mozIFirefoxAccountsBridge);

    if (typeof options == "string") {
      // Restore from JSON case.
      this.bridge.initFromJSON(options);
    } else {
      // New instance case.
      let props = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
        Ci.nsIWritablePropertyBag
      );
      props.setProperty("content_url", options.fxaServer);
      props.setProperty("client_id", options.clientId);
      props.setProperty("redirect_uri", options.redirectUri);
      props.setProperty(
        "token_server_url_override",
        options.tokenServerUrlOverride || ""
      );
      this.bridge.init(props);
    }
  }
  /**
   * Serialize the state of a `RustFxAccount` instance. It can be restored
   * later by passing the resulting String back to the `RustFxAccount` constructor.
   * It is the responsability of the caller to
   * persist that serialized state regularly (after operations that mutate
   * `RustFxAccount`) in a **secure** location.
   * @returns {Promise<string>} The JSON representation of the state.
   */
  async stateJSON() {
    return promisify(this.bridge.stateJSON);
  }
  /**
   * Request a OAuth token by starting a new OAuth flow.
   *
   * Once the user has confirmed the authorization grant, they will get redirected to `redirect_url`:
   * the caller must intercept that redirection; extract the `code` and `state` query parameters and call
   * `completeOAuthFlow(...)` to complete the flow.
   *
   * @param {[string]} scopes
   * @param {string} entryPoint - a string for metrics.
   * @returns {Promise<string>}  a URL string that the caller should navigate to.
   */
  async beginOAuthFlow(scopes, entryPoint = "desktop") {
    return promisify(this.bridge.beginOAuthFlow, scopes, entryPoint);
  }
  /**
   * Complete an OAuth flow initiated by `beginOAuthFlow(...)`.
   *
   * @param {string} code
   * @param {string} state
   * @throws if there was an error during the login flow.
   */
  async completeOAuthFlow(code, state) {
    return promisify(this.bridge.completeOAuthFlow, code, state);
  }
  /**
   * Try to get an OAuth access token.
   *
   * @typedef {Object} AccessTokenInfo
   * @property {string} scope
   * @property {string} token
   * @property {ScopedKey} [key]
   * @property {Date} expires_at
   *
   * @typedef {Object} ScopedKey
   * @property {string} kty
   * @property {string} scope
   * @property {string} k
   * @property {string} kid
   *
   * @param {string} scope Single OAuth scope
   * @param {Number} [ttl] Time in seconds for which the token will be used.
   * @returns {Promise<AccessTokenInfo>}
   * @throws if we couldn't provide an access token
   * for this scope. The caller should then start the OAuth Flow again with
   * the desired scope.
   */
  async getAccessToken(scope, ttl) {
    return JSON.parse(await promisify(this.bridge.getAccessToken, scope, ttl));
  }
  /**
   * Get the session token if held.
   *
   * @returns {Promise<string>}
   * @throws if a session token is not being held.
   */
  async getSessionToken() {
    return promisify(this.bridge.getSessionToken);
  }
  /**
   * Returns the list of OAuth attached clients.
   *
   * @typedef {Object} AttachedClient
   * @property {string} [clientId]
   * @property {string} [sessionTokenId]
   * @property {string} [refreshTokenId]
   * @property {string} [deviceId]
   * @property {DeviceType} [deviceType]
   * @property {boolean} isCurrentSession
   * @property {string} [name]
   * @property {Number} [createdTime]
   * @property {Number} [lastAccessTime]
   * @property {string[]} [scope]
   * @property {string} userAgent
   * @property {string} [os]
   *
   * @returns {Promise<[AttachedClient]>}
   * @throws if a session token is not being held.
   */
  async getAttachedClients() {
    return JSON.parse(await promisify(this.bridge.getAttachedClients));
  }
  /**
   * Check whether the currently held refresh token is active.
   *
   * @typedef {Object} IntrospectInfo
   * @property {boolean} active
   *
   * @returns {Promise<IntrospectInfo>}
   */
  async checkAuthorizationStatus() {
    return JSON.parse(await promisify(this.bridge.checkAuthorizationStatus));
  }
  /*
   * This method should be called when a request made with
   * an OAuth token failed with an authentication error.
   * It clears the internal cache of OAuth access tokens,
   * so the caller can try to call `getAccessToken` or `getProfile`
   * again.
   */
  async clearAccessTokenCache() {
    return promisify(this.bridge.clearAccessTokenCache);
  }
  /*
   * Disconnect from the account and optionaly destroy our device record.
   * `beginOAuthFlow(...)` will need to be called to reconnect.
   */
  async disconnect() {
    return promisify(this.bridge.disconnect);
  }
  /**
   * Gets the logged-in user profile.
   *
   * @typedef {Object} Profile
   * @property {string} uid
   * @property {string} email
   * @property {string} avatar
   * @property {boolean} avatarDefault
   * @property {string} [displayName]
   *
   * @param {boolean} [ignoreCache=false] Ignore the profile freshness threshold.
   * @returns {Promise<Profile>}
   * @throws if no suitable access token was found to make this call.
   * The caller should then start the OAuth login flow again with
   * at least the `profile` scope.
   */
  async getProfile(ignoreCache) {
    return JSON.parse(await promisify(this.bridge.getProfile, ignoreCache));
  }
  /**
   * Start a migration process from a session-token-based authenticated account.
   *
   * @typedef {Object} MigrationResult
   * @property {Number} total_duration
   *
   * @param {string} sessionToken
   * @param {string} kSync
   * @param {string} kXCS
   * @param {Boolean} copySessionToken
   * @returns {Promise<MigrationResult>}
   */
  async migrateFromSessionToken(
    sessionToken,
    kSync,
    kXCS,
    copySessionToken = false
  ) {
    return JSON.parse(
      await promisify(
        this.bridge.migrateFromSessionToken,
        sessionToken,
        kSync,
        kXCS,
        copySessionToken
      )
    );
  }
  /**
   * Retry a migration that failed earlier because of transient reasons.
   *
   * @returns {Promise<MigrationResult>}
   */
  async retryMigrateFromSessionToken() {
    return JSON.parse(
      await promisify(this.bridge.retryMigrateFromSessionToken)
    );
  }
  /**
   * Call this function after migrateFromSessionToken is un-successful
   * (or after app startup) to figure out if we can call `retryMigrateFromSessionToken`.
   *
   * @returns {Promise<boolean>} true if a migration flow can be resumed.
   */
  async isInMigrationState() {
    return promisify(this.bridge.isInMigrationState);
  }
  /**
   * Called after a password change was done through webchannel.
   *
   * @param {string} sessionToken
   */
  async handleSessionTokenChange(sessionToken) {
    return promisify(this.bridge.handleSessionTokenChange, sessionToken);
  }
  /**
   * Get the token server URL with `1.0/sync/1.5` appended at the end.
   *
   * @returns {Promise<string>}
   */
  async getTokenServerEndpointURL() {
    let url = await promisify(this.bridge.getTokenServerEndpointURL);
    return `${url}${url.endsWith("/") ? "" : "/"}1.0/sync/1.5`;
  }
  /**
   * @returns {Promise<string>}
   */
  async getConnectionSuccessURL() {
    return promisify(this.bridge.getConnectionSuccessURL);
  }
  /**
   * @param {string} entrypoint
   * @returns {Promise<string>}
   */
  async getManageAccountURL(entrypoint) {
    return promisify(this.bridge.getManageAccountURL, entrypoint);
  }
  /**
   * @param {string} entrypoint
   * @returns {Promise<string>}
   */
  async getManageDevicesURL(entrypoint) {
    return promisify(this.bridge.getManageDevicesURL, entrypoint);
  }
  /**
   * Fetch the devices in the account.
   * @typedef {Object} Device
   * @property {string} id
   * @property {string} name
   * @property {DeviceType} type
   * @property {boolean} isCurrentDevice
   * @property {Number} [lastAccessTime]
   * @property {String} [pushAuthKey]
   * @property {String} [pushCallback]
   * @property {String} [pushPublicKey]
   * @property {boolean} pushEndpointExpired
   * @property {Object} availableCommands
   * @property {Object} location
   *
   * @typedef {Object} DevicePushSubscription
   * @property {string} endpoint
   * @property {string} publicKey
   * @property {string} authKey
   *
   * @param {boolean} [ignoreCache=false] Ignore the devices freshness threshold.
   *
   * @returns {Promise<[Device]>}
   */
  async fetchDevices(ignoreCache) {
    return JSON.parse(await promisify(this.bridge.fetchDevices, ignoreCache));
  }
  /**
   * Rename the local device
   *
   * @param {string} name
   */
  async setDeviceDisplayName(name) {
    return promisify(this.bridge.setDeviceDisplayName, name);
  }
  /**
   * Handle an incoming Push message payload.
   *
   * @typedef {Object} DeviceConnectedEvent
   * @property {string} deviceName
   *
   * @typedef {Object} DeviceDisconnectedEvent
   * @property {string} deviceId
   * @property {boolean} isLocalDevice
   *
   * @param {string} payload
   * @return {Promise<[TabReceivedCommand|DeviceConnectedEvent|DeviceDisconnectedEvent]>}
   */
  async handlePushMessage(payload) {
    return JSON.parse(await promisify(this.bridge.handlePushMessage, payload));
  }
  /**
   * Fetch for device commands we didn't receive through Push.
   *
   * @typedef {Object} TabReceivedCommand
   * @property {Device} [from]
   * @property {TabData} tabData
   *
   * @typedef {Object} TabData
   * @property {string} title
   * @property {string} url
   *
   * @returns {Promise<[TabReceivedCommand]>}
   */
  async pollDeviceCommands() {
    return JSON.parse(await promisify(this.bridge.pollDeviceCommands));
  }
  /**
   * Send a tab to a device identified by its ID.
   *
   * @param {string} targetId
   * @param {string} title
   * @param {string} url
   */
  async sendSingleTab(targetId, title, url) {
    return promisify(this.bridge.sendSingleTab, targetId, title, url);
  }
  /**
   * Update our FxA push subscription.
   *
   * @param {string} endpoint
   * @param {string} publicKey
   * @param {string} authKey
   */
  async setDevicePushSubscription(endpoint, publicKey, authKey) {
    return promisify(
      this.bridge.setDevicePushSubscription,
      endpoint,
      publicKey,
      authKey
    );
  }
  /**
   * Initialize the local device (should be done only once after log-in).
   *
   * @param {string} name
   * @param {DeviceType} deviceType
   * @param {[DeviceCapability]} supportedCapabilities
   */
  async initializeDevice(name, deviceType, supportedCapabilities) {
    return promisify(
      this.bridge.initializeDevice,
      name,
      deviceType,
      supportedCapabilities
    );
  }
  /**
   * Update the device capabilities if needed.
   *
   * @param {[DeviceCapability]} supportedCapabilities
   */
  async ensureCapabilities(supportedCapabilities) {
    return promisify(this.bridge.ensureCapabilities, supportedCapabilities);
  }
}

function promisify(func, ...params) {
  return new Promise((resolve, reject) => {
    func(...params, {
      // This object implicitly implements
      // `mozIFirefoxAccountsBridgeCallback`.
      handleSuccess: resolve,
      handleError(code, message) {
        let error = new Error(message);
        error.result = code;
        reject(error);
      },
    });
  });
}

/**
 * @enum
 */
const DeviceType = Object.freeze({
  desktop: "desktop",
  mobile: "mobile",
  tablet: "tablet",
  tv: "tv",
  vr: "vr",
});

/**
 * @enum
 */
const DeviceCapability = Object.freeze({
  sendTab: "sendTab",
  fromCommandName(str) {
    switch (str) {
      case "https://identity.mozilla.com/cmd/open-uri":
        return DeviceCapability.sendTab;
    }
    throw new Error("Unknown device capability.");
  },
});
