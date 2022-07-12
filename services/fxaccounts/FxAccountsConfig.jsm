/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
var EXPORTED_SYMBOLS = ["FxAccountsConfig"];

const { RESTRequest } = ChromeUtils.import(
  "resource://services-common/rest.js"
);
const { log } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsCommon.js"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "fxAccounts", () => {
  return ChromeUtils.import(
    "resource://gre/modules/FxAccounts.jsm"
  ).getFxAccountsSingleton();
});

ChromeUtils.defineModuleGetter(
  lazy,
  "EnsureFxAccountsWebChannel",
  "resource://gre/modules/FxAccountsWebChannel.jsm"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "ROOT_URL",
  "identity.fxaccounts.remote.root"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "CONTEXT_PARAM",
  "identity.fxaccounts.contextParam"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "REQUIRES_HTTPS",
  "identity.fxaccounts.allowHttp",
  false,
  null,
  val => !val
);

const CONFIG_PREFS = [
  "identity.fxaccounts.remote.root",
  "identity.fxaccounts.auth.uri",
  "identity.fxaccounts.remote.oauth.uri",
  "identity.fxaccounts.remote.profile.uri",
  "identity.fxaccounts.remote.pairing.uri",
  "identity.sync.tokenserver.uri",
];
const SYNC_PARAM = "sync";

var FxAccountsConfig = {
  async promiseEmailURI(email, entrypoint, extraParams = {}) {
    return this._buildURL("", {
      extraParams: { entrypoint, email, service: SYNC_PARAM, ...extraParams },
    });
  },

  async promiseConnectAccountURI(entrypoint, extraParams = {}) {
    return this._buildURL("", {
      extraParams: {
        entrypoint,
        action: "email",
        service: SYNC_PARAM,
        ...extraParams,
      },
    });
  },

  async promiseForceSigninURI(entrypoint, extraParams = {}) {
    return this._buildURL("force_auth", {
      extraParams: { entrypoint, service: SYNC_PARAM, ...extraParams },
      addAccountIdentifiers: true,
    });
  },

  async promiseManageURI(entrypoint, extraParams = {}) {
    return this._buildURL("settings", {
      extraParams: { entrypoint, ...extraParams },
      addAccountIdentifiers: true,
    });
  },

  async promiseChangeAvatarURI(entrypoint, extraParams = {}) {
    return this._buildURL("settings/avatar/change", {
      extraParams: { entrypoint, ...extraParams },
      addAccountIdentifiers: true,
    });
  },

  async promiseManageDevicesURI(entrypoint, extraParams = {}) {
    return this._buildURL("settings/clients", {
      extraParams: { entrypoint, ...extraParams },
      addAccountIdentifiers: true,
    });
  },

  async promiseConnectDeviceURI(entrypoint, extraParams = {}) {
    return this._buildURL("connect_another_device", {
      extraParams: { entrypoint, service: SYNC_PARAM, ...extraParams },
      addAccountIdentifiers: true,
    });
  },

  async promisePairingURI(extraParams = {}) {
    return this._buildURL("pair", {
      extraParams,
      includeDefaultParams: false,
    });
  },

  async promiseOAuthURI(extraParams = {}) {
    return this._buildURL("oauth", {
      extraParams,
      includeDefaultParams: false,
    });
  },

  async promiseMetricsFlowURI(entrypoint, extraParams = {}) {
    return this._buildURL("metrics-flow", {
      extraParams: { entrypoint, ...extraParams },
      includeDefaultParams: false,
    });
  },

  get defaultParams() {
    return { context: lazy.CONTEXT_PARAM };
  },

  /**
   * @param path should be parsable by the URL constructor first parameter.
   * @param {bool} [options.includeDefaultParams] If true include the default search params.
   * @param {Object.<string, string>} [options.extraParams] Additionnal search params.
   * @param {bool} [options.addAccountIdentifiers] if true we add the current logged-in user uid and email to the search params.
   */
  async _buildURL(
    path,
    {
      includeDefaultParams = true,
      extraParams = {},
      addAccountIdentifiers = false,
    }
  ) {
    await this.ensureConfigured();
    const url = new URL(path, lazy.ROOT_URL);
    if (lazy.REQUIRES_HTTPS && url.protocol != "https:") {
      throw new Error("Firefox Accounts server must use HTTPS");
    }
    const params = {
      ...(includeDefaultParams ? this.defaultParams : null),
      ...extraParams,
    };
    for (let [k, v] of Object.entries(params)) {
      url.searchParams.append(k, v);
    }
    if (addAccountIdentifiers) {
      const accountData = await this.getSignedInUser();
      if (!accountData) {
        return null;
      }
      url.searchParams.append("uid", accountData.uid);
      url.searchParams.append("email", accountData.email);
    }
    return url.href;
  },

  async _buildURLFromString(href, extraParams = {}) {
    const url = new URL(href);
    for (let [k, v] of Object.entries(extraParams)) {
      url.searchParams.append(k, v);
    }
    return url.href;
  },

  resetConfigURLs() {
    let autoconfigURL = this.getAutoConfigURL();
    if (!autoconfigURL) {
      return;
    }
    // They have the autoconfig uri pref set, so we clear all the prefs that we
    // will have initialized, which will leave them pointing at production.
    for (let pref of CONFIG_PREFS) {
      Services.prefs.clearUserPref(pref);
    }
    // Reset the webchannel.
    lazy.EnsureFxAccountsWebChannel();
  },

  getAutoConfigURL() {
    let pref = Services.prefs.getCharPref(
      "identity.fxaccounts.autoconfig.uri",
      ""
    );
    if (!pref) {
      // no pref / empty pref means we don't bother here.
      return "";
    }
    let rootURL = Services.urlFormatter.formatURL(pref);
    if (rootURL.endsWith("/")) {
      rootURL = rootURL.slice(0, -1);
    }
    return rootURL;
  },

  async ensureConfigured() {
    let isSignedIn = !!(await this.getSignedInUser());
    if (!isSignedIn) {
      await this.updateConfigURLs();
    }
  },

  // Returns true if this user is using the FxA "production" systems, false
  // if using any other configuration, including self-hosting or the FxA
  // non-production systems such as "dev" or "staging".
  // It's typically used as a proxy for "is this likely to be a self-hosted
  // user?", but it's named this way to make the implementation less
  // surprising. As a result, it's fairly conservative and would prefer to have
  // a false-negative than a false-position as it determines things which users
  // might consider sensitive (notably, telemetry).
  // Note also that while it's possible to self-host just sync and not FxA, we
  // don't make that distinction - that's a self-hoster from the POV of this
  // function.
  isProductionConfig() {
    // Specifically, if the autoconfig URLs, or *any* of the URLs that
    // we consider configurable are modified, we assume self-hosted.
    if (this.getAutoConfigURL()) {
      return false;
    }
    for (let pref of CONFIG_PREFS) {
      if (Services.prefs.prefHasUserValue(pref)) {
        return false;
      }
    }
    return true;
  },

  // Read expected client configuration from the fxa auth server
  // (from `identity.fxaccounts.autoconfig.uri`/.well-known/fxa-client-configuration)
  // and replace all the relevant our prefs with the information found there.
  // This is only done before sign-in and sign-up, and even then only if the
  // `identity.fxaccounts.autoconfig.uri` preference is set.
  async updateConfigURLs() {
    let rootURL = this.getAutoConfigURL();
    if (!rootURL) {
      return;
    }
    const config = await this.fetchConfigDocument(rootURL);
    try {
      // Update the prefs directly specified by the config.
      let authServerBase = config.auth_server_base_url;
      if (!authServerBase.endsWith("/v1")) {
        authServerBase += "/v1";
      }
      Services.prefs.setCharPref(
        "identity.fxaccounts.auth.uri",
        authServerBase
      );
      Services.prefs.setCharPref(
        "identity.fxaccounts.remote.oauth.uri",
        config.oauth_server_base_url + "/v1"
      );
      // At the time of landing this, our servers didn't yet answer with pairing_server_base_uri.
      // Remove this condition check once Firefox 68 is stable.
      if (config.pairing_server_base_uri) {
        Services.prefs.setCharPref(
          "identity.fxaccounts.remote.pairing.uri",
          config.pairing_server_base_uri
        );
      }
      Services.prefs.setCharPref(
        "identity.fxaccounts.remote.profile.uri",
        config.profile_server_base_url + "/v1"
      );
      Services.prefs.setCharPref(
        "identity.sync.tokenserver.uri",
        config.sync_tokenserver_base_url + "/1.0/sync/1.5"
      );
      Services.prefs.setCharPref("identity.fxaccounts.remote.root", rootURL);

      // Ensure the webchannel is pointed at the correct uri
      lazy.EnsureFxAccountsWebChannel();
    } catch (e) {
      log.error(
        "Failed to initialize configuration preferences from autoconfig object",
        e
      );
      throw e;
    }
  },

  // Read expected client configuration from the fxa auth server
  // (or from the provided rootURL, if present) and return it as an object.
  async fetchConfigDocument(rootURL = null) {
    if (!rootURL) {
      rootURL = lazy.ROOT_URL;
    }
    let configURL = rootURL + "/.well-known/fxa-client-configuration";
    let request = new RESTRequest(configURL);
    request.setHeader("Accept", "application/json");

    // Catch and rethrow the error inline.
    let resp = await request.get().catch(e => {
      log.error(`Failed to get configuration object from "${configURL}"`, e);
      throw e;
    });
    if (!resp.success) {
      // Note: 'resp.body' is included with the error log below as we are not concerned
      // that the body will contain PII, but if that changes it should be excluded.
      log.error(
        `Received HTTP response code ${resp.status} from configuration object request:
        ${resp.body}`
      );
      throw new Error(
        `HTTP status ${resp.status} from configuration object request`
      );
    }
    log.debug("Got successful configuration response", resp.body);
    try {
      return JSON.parse(resp.body);
    } catch (e) {
      log.error(
        `Failed to parse configuration preferences from ${configURL}`,
        e
      );
      throw e;
    }
  },

  // For test purposes, returns a Promise.
  getSignedInUser() {
    return lazy.fxAccounts.getSignedInUser();
  },
};
