/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  RemoteSettings: "resource://services-settings/remote-settings.js",
  RemoteSettingsClient: "resource://services-settings/RemoteSettingsClient.jsm",
});

var EXPORTED_SYMBOLS = ["IgnoreLists"];

const SETTINGS_IGNORELIST_KEY = "hijack-blocklists";

class IgnoreListsManager {
  async init() {
    if (!this._ignoreListSettings) {
      this._ignoreListSettings = lazy.RemoteSettings(SETTINGS_IGNORELIST_KEY);
    }
  }

  async getAndSubscribe(listener) {
    await this.init();

    // Trigger a get of the initial value.
    const settings = await this._getIgnoreList();

    // Listen for future updates after we first get the values.
    this._ignoreListSettings.on("sync", listener);

    return settings;
  }

  unsubscribe(listener) {
    if (!this._ignoreListSettings) {
      return;
    }

    this._ignoreListSettings.off("sync", listener);
  }

  async _getIgnoreList() {
    if (this._getSettingsPromise) {
      return this._getSettingsPromise;
    }

    const settings = await (this._getSettingsPromise = this._getIgnoreListSettings());
    delete this._getSettingsPromise;
    return settings;
  }

  /**
   * Obtains the current ignore list from remote settings. This includes
   * verifying the signature of the ignore list within the database.
   *
   * If the signature in the database is invalid, the database will be wiped
   * and the stored dump will be used, until the settings next update.
   *
   * Note that this may cause a network check of the certificate, but that
   * should generally be quick.
   *
   * @param {boolean} [firstTime]
   *   Internal boolean to indicate if this is the first time check or not.
   * @returns {array}
   *   An array of objects in the database, or an empty array if none
   *   could be obtained.
   */
  async _getIgnoreListSettings(firstTime = true) {
    let result = [];
    try {
      result = await this._ignoreListSettings.get({
        verifySignature: true,
      });
    } catch (ex) {
      if (
        ex instanceof lazy.RemoteSettingsClient.InvalidSignatureError &&
        firstTime
      ) {
        // The local database is invalid, try and reset it.
        await this._ignoreListSettings.db.clear();
        // Now call this again.
        return this._getIgnoreListSettings(false);
      }
      // Don't throw an error just log it, just continue with no data, and hopefully
      // a sync will fix things later on.
      Cu.reportError(ex);
    }
    return result;
  }
}

const IgnoreLists = new IgnoreListsManager();
