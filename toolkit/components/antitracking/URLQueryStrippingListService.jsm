/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ComponentUtils } = ChromeUtils.import(
  "resource://gre/modules/ComponentUtils.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
});

const COLLECTION_NAME = "query-stripping";

const PREF_STRIP_LIST_NAME = "privacy.query_stripping.strip_list";
const PREF_ALLOW_LIST_NAME = "privacy.query_stripping.allow_list";

class URLQueryStrippingListService {
  constructor() {
    this.classID = Components.ID("{afff16f0-3fd2-4153-9ccd-c6d9abd879e4}");
    this.QueryInterface = ChromeUtils.generateQI([
      "nsIURLQueryStrippingListService",
    ]);
    this._xpcom_factory = ComponentUtils.generateSingletonFactory(
      URLQueryStrippingListService
    );
    this.observers = new Set();
    this.prefStripList = [];
    this.prefAllowList = [];
    this.remoteStripList = [];
    this.remoteAllowList = [];

    this._initPromise = this._init();
  }

  async _init() {
    let rs = RemoteSettings(COLLECTION_NAME);

    rs.on("sync", event => {
      let {
        data: { current },
      } = event;
      this._onRemoteSettingsUpdate(current);
    });

    // Get the initially available entries for remote settings.
    let entries;
    try {
      entries = await rs.get();
    } catch (e) {}
    this._onRemoteSettingsUpdate(entries || []);

    // Get the list from pref.
    this._onPrefUpdate(
      PREF_STRIP_LIST_NAME,
      Services.prefs.getStringPref(PREF_STRIP_LIST_NAME, "")
    );
    this._onPrefUpdate(
      PREF_ALLOW_LIST_NAME,
      Services.prefs.getStringPref(PREF_ALLOW_LIST_NAME, "")
    );

    Services.prefs.addObserver(PREF_STRIP_LIST_NAME, this);
    Services.prefs.addObserver(PREF_ALLOW_LIST_NAME, this);

    AsyncShutdown.profileBeforeChange.addBlocker(
      "URLQueryStrippingListService",
      () => this._shutdown()
    );
  }

  async _shutdown() {
    Services.prefs.removeObserver(PREF_STRIP_LIST_NAME, this);
    Services.prefs.removeObserver(PREF_ALLOW_LIST_NAME, this);
  }

  _onRemoteSettingsUpdate(entries) {
    this.remoteStripList = [];
    this.remoteAllowList = [];

    for (let entry of entries) {
      for (let item of entry.stripList) {
        this.remoteStripList.push(item);
      }

      for (let item of entry.allowList) {
        this.remoteAllowList.push(item);
      }
    }

    this._notifyObservers();
  }

  _onPrefUpdate(pref, value) {
    switch (pref) {
      case PREF_STRIP_LIST_NAME:
        this.prefStripList = value.split(" ");
        break;

      case PREF_ALLOW_LIST_NAME:
        this.prefAllowList = value.split(",");
        break;

      default:
        Cu.reportError(`Unexpected pref name ${pref}`);
        return;
    }

    this._notifyObservers();
  }

  async _ensureInit() {
    await this._initPromise;
  }

  _notifyObservers(observer) {
    let stripEntries = this.prefStripList.concat(this.remoteStripList);
    let allowEntries = this.prefAllowList.concat(this.remoteAllowList);
    let stripEntriesAsString = stripEntries.join(" ").toLowerCase();
    let allowEntriesAsString = allowEntries.join(",").toLowerCase();

    let observers = observer ? [observer] : this.observers;

    for (let obs of observers) {
      obs.onQueryStrippingListUpdate(
        stripEntriesAsString,
        allowEntriesAsString
      );
    }
  }

  registerAndRunObserver(observer) {
    this._ensureInit().then(_ => {
      this.observers.add(observer);
      this._notifyObservers(observer);
    });
  }

  unregisterObserver(observer) {
    this.observers.delete(observer);
  }

  observe(subject, topic, data) {
    if (topic != "nsPref:changed") {
      Cu.reportError(`Unexpected event ${topic}`);
      return;
    }

    let prefValue = Services.prefs.getStringPref(data, "");
    this._onPrefUpdate(data, prefValue);
  }
}

var EXPORTED_SYMBOLS = ["URLQueryStrippingListService"];
