/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
this.EXPORTED_SYMBOLS = ["FxAccountsConfig"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://services-common/rest.js");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
                                  "resource://gre/modules/FxAccounts.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "EnsureFxAccountsWebChannel",
                                  "resource://gre/modules/FxAccountsWebChannel.jsm");

const CONFIG_PREFS = [
  "identity.fxaccounts.auth.uri",
  "identity.fxaccounts.remote.oauth.uri",
  "identity.fxaccounts.remote.profile.uri",
  "identity.sync.tokenserver.uri",
  "identity.fxaccounts.remote.webchannel.uri",
  "identity.fxaccounts.settings.uri",
  "identity.fxaccounts.settings.devices.uri",
  "identity.fxaccounts.remote.signup.uri",
  "identity.fxaccounts.remote.signin.uri",
  "identity.fxaccounts.remote.force_auth.uri",
];

this.FxAccountsConfig = {

  // Returns a promise that resolves with the URI of the remote UI flows.
  async promiseAccountsSignUpURI() {
    await this.ensureConfigured();
    let url = Services.urlFormatter.formatURLPref("identity.fxaccounts.remote.signup.uri");
    if (fxAccounts.requiresHttps() && !/^https:/.test(url)) { // Comment to un-break emacs js-mode highlighting
      throw new Error("Firefox Accounts server must use HTTPS");
    }
    return url;
  },

  // Returns a promise that resolves with the URI of the remote UI flows.
  async promiseAccountsSignInURI() {
    await this.ensureConfigured();
    let url = Services.urlFormatter.formatURLPref("identity.fxaccounts.remote.signin.uri");
    if (fxAccounts.requiresHttps() && !/^https:/.test(url)) { // Comment to un-break emacs js-mode highlighting
      throw new Error("Firefox Accounts server must use HTTPS");
    }
    return url;
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
    if (!Services.prefs.prefHasUserValue("webchannel.allowObject.urlWhitelist")) {
      return;
    }
    let whitelistValue = Services.prefs.getCharPref("webchannel.allowObject.urlWhitelist");
    if (whitelistValue.startsWith(autoconfigURL + " ")) {
      whitelistValue = whitelistValue.slice(autoconfigURL.length + 1);
      // Check and see if the value will be the default, and just clear the pref if it would
      // to avoid it showing up as changed in about:config.
      let defaultWhitelist = Services.prefs.getDefaultBranch("webchannel.allowObject.").getCharPref("urlWhitelist", "");

      if (defaultWhitelist === whitelistValue) {
        Services.prefs.clearUserPref("webchannel.allowObject.urlWhitelist");
      } else {
        Services.prefs.setCharPref("webchannel.allowObject.urlWhitelist", whitelistValue);
      }
    }
  },

  getAutoConfigURL() {
    let pref = Services.prefs.getCharPref("identity.fxaccounts.autoconfig.uri", "");
    if (!pref) {
      // no pref / empty pref means we don't bother here.
      return "";
    }
    let rootURL = Services.urlFormatter.formatURL(pref);
    if (rootURL.endsWith("/")) {
      rootURL.slice(0, -1);
    }
    return rootURL;
  },

  async ensureConfigured() {
    let isSignedIn = !!(await fxAccounts.getSignedInUser());
    if (!isSignedIn) {
      await this.fetchConfigURLs();
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
    let jsonStr = await new Promise((resolve, reject) => {
      let request = new RESTRequest(configURL);
      request.setHeader("Accept", "application/json");
      request.get(error => {
        if (error) {
          log.error(`Failed to get configuration object from "${configURL}"`, error);
          reject(error);
          return;
        }
        if (!request.response.success) {
          log.error(`Received HTTP response code ${request.response.status} from configuration object request`);
          if (request.response && request.response.body) {
            log.debug("Got error response", request.response.body);
          }
          reject(request.response.status);
          return;
        }
        resolve(request.response.body);
      });
    });

    log.debug("Got successful configuration response", jsonStr);
    try {
      // Update the prefs directly specified by the config.
      let config = JSON.parse(jsonStr);
      let authServerBase = config.auth_server_base_url;
      if (!authServerBase.endsWith("/v1")) {
        authServerBase += "/v1";
      }
      Services.prefs.setCharPref("identity.fxaccounts.auth.uri", authServerBase);
      Services.prefs.setCharPref("identity.fxaccounts.remote.oauth.uri", config.oauth_server_base_url + "/v1");
      Services.prefs.setCharPref("identity.fxaccounts.remote.profile.uri", config.profile_server_base_url + "/v1");
      Services.prefs.setCharPref("identity.sync.tokenserver.uri", config.sync_tokenserver_base_url + "/1.0/sync/1.5");
      // Update the prefs that are based off of the autoconfig url

      let contextParam = encodeURIComponent(
        Services.prefs.getCharPref("identity.fxaccounts.contextParam"));

      Services.prefs.setCharPref("identity.fxaccounts.remote.webchannel.uri", rootURL);
      Services.prefs.setCharPref("identity.fxaccounts.settings.uri", rootURL + "/settings?service=sync&context=" + contextParam);
      Services.prefs.setCharPref("identity.fxaccounts.settings.devices.uri", rootURL + "/settings/clients?service=sync&context=" + contextParam);
      Services.prefs.setCharPref("identity.fxaccounts.remote.signup.uri", rootURL + "/signup?service=sync&context=" + contextParam);
      Services.prefs.setCharPref("identity.fxaccounts.remote.signin.uri", rootURL + "/signin?service=sync&context=" + contextParam);
      Services.prefs.setCharPref("identity.fxaccounts.remote.force_auth.uri", rootURL + "/force_auth?service=sync&context=" + contextParam);

      let whitelistValue = Services.prefs.getCharPref("webchannel.allowObject.urlWhitelist");
      if (!whitelistValue.includes(rootURL)) {
        whitelistValue = `${rootURL} ${whitelistValue}`;
        Services.prefs.setCharPref("webchannel.allowObject.urlWhitelist", whitelistValue);
      }
      // Ensure the webchannel is pointed at the correct uri
      EnsureFxAccountsWebChannel();
    } catch (e) {
      log.error("Failed to initialize configuration preferences from autoconfig object", e);
      throw e;
    }
  },

};
