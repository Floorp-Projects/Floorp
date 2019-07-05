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
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "fxAccounts",
  "resource://gre/modules/FxAccounts.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "EnsureFxAccountsWebChannel",
  "resource://gre/modules/FxAccountsWebChannel.jsm"
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "ROOT_URL",
  "identity.fxaccounts.remote.root"
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "CONTEXT_PARAM",
  "identity.fxaccounts.contextParam"
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "REQUIRES_HTTPS",
  // Also used in FxAccountsOAuthGrantClient.jsm.
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

var FxAccountsConfig = {
  async promiseSignUpURI(entrypoint) {
    return this._buildURL("signup", {
      extraParams: { entrypoint },
    });
  },

  async promiseSignInURI(entrypoint) {
    return this._buildURL("signin", {
      extraParams: { entrypoint },
    });
  },

  async promiseEmailURI(email, entrypoint) {
    return this._buildURL("", {
      extraParams: { entrypoint, email },
    });
  },

  async promiseEmailFirstURI(entrypoint) {
    return this._buildURL("", {
      extraParams: { entrypoint, action: "email" },
    });
  },

  async promiseForceSigninURI(entrypoint) {
    return this._buildURL("force_auth", {
      extraParams: { entrypoint },
      addAccountIdentifiers: true,
    });
  },

  async promiseManageURI(entrypoint) {
    return this._buildURL("settings", {
      extraParams: { entrypoint },
      addAccountIdentifiers: true,
    });
  },

  async promiseChangeAvatarURI(entrypoint) {
    return this._buildURL("settings/avatar/change", {
      extraParams: { entrypoint },
      addAccountIdentifiers: true,
    });
  },

  async promiseManageDevicesURI(entrypoint) {
    return this._buildURL("settings/clients", {
      extraParams: { entrypoint },
      addAccountIdentifiers: true,
    });
  },

  async promiseConnectDeviceURI(entrypoint) {
    return this._buildURL("connect_another_device", {
      extraParams: { entrypoint },
      addAccountIdentifiers: true,
    });
  },

  async promisePairingURI() {
    return this._buildURL("pair", {
      includeDefaultParams: false,
    });
  },

  async promiseOAuthURI() {
    return this._buildURL("oauth", {
      includeDefaultParams: false,
    });
  },

  get defaultParams() {
    return { service: "sync", context: CONTEXT_PARAM };
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
    const url = new URL(path, ROOT_URL);
    if (REQUIRES_HTTPS && url.protocol != "https:") {
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
    EnsureFxAccountsWebChannel();
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
    await this.tryPrefsMigration();
    let isSignedIn = !!(await this.getSignedInUser());
    if (!isSignedIn) {
      await this.fetchConfigURLs();
    }
  },

  // In bug 1427674 we migrated a set of preferences with a shared origin
  // to a single preference (identity.fxaccounts.remote.root).
  // This whole function should be removed in version 65 or later once
  // everyone had a chance to migrate.
  async tryPrefsMigration() {
    // If this pref is set, there is a very good chance the user is running
    // a custom FxA content server.
    if (
      !Services.prefs.prefHasUserValue("identity.fxaccounts.remote.signin.uri")
    ) {
      return;
    }

    if (Services.prefs.prefHasUserValue("identity.fxaccounts.autoconfig.uri")) {
      await this.fetchConfigURLs();
    } else {
      // Best effort.
      const signinURI = Services.prefs.getCharPref(
        "identity.fxaccounts.remote.signin.uri"
      );
      Services.prefs.setCharPref(
        "identity.fxaccounts.remote.root",
        signinURI.slice(0, signinURI.lastIndexOf("/signin")) + "/"
      );
    }

    const migratedPrefs = [
      "identity.fxaccounts.remote.webchannel.uri",
      "identity.fxaccounts.settings.uri",
      "identity.fxaccounts.settings.devices.uri",
      "identity.fxaccounts.remote.signup.uri",
      "identity.fxaccounts.remote.signin.uri",
      "identity.fxaccounts.remote.email.uri",
      "identity.fxaccounts.remote.connectdevice.uri",
      "identity.fxaccounts.remote.force_auth.uri",
    ];
    for (const pref of migratedPrefs) {
      Services.prefs.clearUserPref(pref);
    }
  },

  // Read expected client configuration from the fxa auth server
  // (from `identity.fxaccounts.autoconfig.uri`/.well-known/fxa-client-configuration)
  // and replace all the relevant our prefs with the information found there.
  // This is only done before sign-in and sign-up, and even then only if the
  // `identity.fxaccounts.autoconfig.uri` preference is set.
  async fetchConfigURLs() {
    let rootURL = this.getAutoConfigURL();
    if (!rootURL) {
      return;
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
      log.error(
        `Received HTTP response code ${
          resp.status
        } from configuration object request`
      );
      if (resp.body) {
        log.debug("Got error response", resp.body);
      }
      throw new Error(
        `HTTP status ${resp.status} from configuration object request`
      );
    }

    log.debug("Got successful configuration response", resp.body);
    try {
      // Update the prefs directly specified by the config.
      let config = JSON.parse(resp.body);
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
      EnsureFxAccountsWebChannel();
    } catch (e) {
      log.error(
        "Failed to initialize configuration preferences from autoconfig object",
        e
      );
      throw e;
    }
  },

  // For test purposes, returns a Promise.
  getSignedInUser() {
    return fxAccounts.getSignedInUser();
  },
};
