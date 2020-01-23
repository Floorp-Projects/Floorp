/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["GeckoViewLoginStorage", "LoginEntry"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
});

XPCOMUtils.defineLazyGetter(this, "LoginInfo", () =>
  Components.Constructor(
    "@mozilla.org/login-manager/loginInfo;1",
    "nsILoginInfo",
    "init"
  )
);

class LoginEntry {
  constructor() {
    this.origin = null;
    this.formActionOrigin = null;
    this.httpRealm = null;
    this.username = null;
    this.password = null;

    // Metadata.
    this.guid = null;
    // TODO: Not supported by GV.
    this.timeCreated = null;
    this.timeLastUsed = null;
    this.timePasswordChanged = null;
    this.timesUsed = null;
  }

  toLoginInfo() {
    const info = new LoginInfo(
      this.origin,
      this.formActionOrigin,
      this.httpRealm,
      this.username,
      this.password
    );

    // Metadata.
    info.QueryInterface(Ci.nsILoginMetaInfo);
    info.guid = this.guid;
    info.timeCreated = this.timeCreated;
    info.timeLastUsed = this.timeLastUsed;
    info.timePasswordChanged = this.timePasswordChanged;
    info.timesUsed = this.timesUsed;

    return info;
  }

  static fromBundle(aObj) {
    const entry = new LoginEntry();
    Object.assign(entry, aObj);

    return entry;
  }

  static fromLoginInfo(aInfo) {
    const entry = new LoginEntry();
    entry.origin = aInfo.origin;
    entry.formActionOrigin = aInfo.formActionOrigin;
    entry.httpRealm = aInfo.httpRealm;
    entry.username = aInfo.username;
    entry.password = aInfo.password;

    // Metadata.
    aInfo.QueryInterface(Ci.nsILoginMetaInfo);
    entry.guid = aInfo.guid;
    entry.timeCreated = aInfo.timeCreated;
    entry.timeLastUsed = aInfo.timeLastUsed;
    entry.timePasswordChanged = aInfo.timePasswordChanged;
    entry.timesUsed = aInfo.timesUsed;

    return entry;
  }
}

// Sync with LoginStorage.Delegate.UsedField in LoginStorage.java.
const UsedField = { PASSWORD: 1 };

const GeckoViewLoginStorage = {
  /**
   * Delegates login entry fetching for the given domain to the attached
   * LoginStorage GeckoView delegate.
   *
   * @param aDomain
   *        The domain string to fetch login entries for.
   * @return {Promise}
   *         Resolves with an array of login objects or null.
   *         Rejected if no delegate is attached.
   *         Login object string properties:
   *         { guid, origin, formActionOrigin, httpRealm, username, password }
   */
  fetchLogins(aDomain) {
    debug`fetchLogins for ${aDomain}`;

    return EventDispatcher.instance.sendRequestForResult({
      type: "GeckoView:LoginStorage:Fetch",
      domain: aDomain,
    });
  },

  /**
   * Delegates login entry saving to the attached LoginStorage GeckoView delegate.
   * Call this when a new login entry or a new password for an existing login
   * entry has been submitted.
   *
   * @param aLogin The {LoginEntry} to be saved.
   */
  onLoginSave(aLogin) {
    debug`onLoginSave ${aLogin}`;

    EventDispatcher.instance.sendRequest({
      type: "GeckoView:LoginStorage:Save",
      login: aLogin,
    });
  },

  /**
   * Delegates login entry password usage to the attached LoginStorage GeckoView
   * delegate.
   * Call this when the password of an existing login entry, as returned by
   * fetchLogins, has been used for autofill.
   *
   * @param aLogin The {LoginEntry} whose password was used.
   */
  onLoginPasswordUsed(aLogin) {
    debug`onLoginUsed ${aLogin}`;

    EventDispatcher.instance.sendRequest({
      type: "GeckoView:LoginStorage:Used",
      usedFields: UsedField.PASSWORD,
      login: aLogin,
    });
  },
};

const { debug } = GeckoViewUtils.initLogging("GeckoViewLoginStorage");
