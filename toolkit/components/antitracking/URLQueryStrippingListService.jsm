/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  RemoteSettings: "resource://services-settings/remote-settings.js",
});

const COLLECTION_NAME = "query-stripping";
const SHARED_DATA_KEY = "URLQueryStripping";

const PREF_STRIP_LIST_NAME = "privacy.query_stripping.strip_list";
const PREF_ALLOW_LIST_NAME = "privacy.query_stripping.allow_list";
const PREF_TESTING_ENABLED = "privacy.query_stripping.testing";

class URLQueryStrippingListService {
  constructor() {
    this.classID = Components.ID("{afff16f0-3fd2-4153-9ccd-c6d9abd879e4}");
    this.QueryInterface = ChromeUtils.generateQI([
      "nsIURLQueryStrippingListService",
    ]);
    this.observers = new Set();
    this.prefStripList = new Set();
    this.prefAllowList = new Set();
    this.remoteStripList = new Set();
    this.remoteAllowList = new Set();
    this.isParentProcess =
      Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_DEFAULT;
  }

  async _init() {
    // We can only access the remote settings in the parent process. For content
    // processes, we will use sharedData to sync the list to content processes.
    if (this.isParentProcess) {
      let rs = lazy.RemoteSettings(COLLECTION_NAME);

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
    } else {
      // Register the message listener for the remote settings update from the
      // sharedData.
      Services.cpmm.sharedData.addEventListener("change", this);

      // Get the remote settings data from the shared data.
      let data = this._getListFromSharedData();

      this._onRemoteSettingsUpdate(data);
    }

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

    Services.obs.addObserver(this, "xpcom-shutdown");
  }

  async _shutdown() {
    Services.obs.removeObserver(this, "xpcom-shutdown");
    Services.prefs.removeObserver(PREF_STRIP_LIST_NAME, this);
    Services.prefs.removeObserver(PREF_ALLOW_LIST_NAME, this);
  }

  _onRemoteSettingsUpdate(entries) {
    this.remoteStripList.clear();
    this.remoteAllowList.clear();

    for (let entry of entries) {
      for (let item of entry.stripList) {
        this.remoteStripList.add(item);
      }

      for (let item of entry.allowList) {
        this.remoteAllowList.add(item);
      }
    }

    // Because only the parent process will get the remote settings update, so
    // we will sync the list to the shared data so that content processes can
    // get the list.
    if (this.isParentProcess) {
      Services.ppmm.sharedData.set(SHARED_DATA_KEY, {
        stripList: this.remoteStripList,
        allowList: this.remoteAllowList,
      });

      if (Services.prefs.getBoolPref(PREF_TESTING_ENABLED, false)) {
        Services.ppmm.sharedData.flush();
      }
    }

    this._notifyObservers();
  }

  _onPrefUpdate(pref, value) {
    switch (pref) {
      case PREF_STRIP_LIST_NAME:
        this.prefStripList = new Set(value ? value.split(" ") : []);
        break;

      case PREF_ALLOW_LIST_NAME:
        this.prefAllowList = new Set(value ? value.split(",") : []);
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

  _getListFromSharedData() {
    let data = Services.cpmm.sharedData.get(SHARED_DATA_KEY);

    return data ? [data] : [];
  }

  _notifyObservers(observer) {
    let stripEntries = new Set([
      ...this.prefStripList,
      ...this.remoteStripList,
    ]);
    let allowEntries = new Set([
      ...this.prefAllowList,
      ...this.remoteAllowList,
    ]);
    let stripEntriesAsString = Array.from(stripEntries)
      .join(" ")
      .toLowerCase();
    let allowEntriesAsString = Array.from(allowEntries)
      .join(",")
      .toLowerCase();

    let observers = observer ? [observer] : this.observers;

    for (let obs of observers) {
      obs.onQueryStrippingListUpdate(
        stripEntriesAsString,
        allowEntriesAsString
      );
    }
  }

  init() {
    if (this.initialized) {
      return;
    }

    this.initialized = true;
    this._initPromise = this._init();
  }

  registerAndRunObserver(observer) {
    let addAndRunObserver = _ => {
      this.observers.add(observer);
      this._notifyObservers(observer);
    };

    if (this.initialized) {
      addAndRunObserver();
      return;
    }

    this._ensureInit().then(addAndRunObserver);
  }

  unregisterObserver(observer) {
    this.observers.delete(observer);
  }

  clearLists() {
    if (!this.isParentProcess) {
      return;
    }

    // Clear the lists of remote settings.
    this._onRemoteSettingsUpdate([]);

    // Clear the user pref for the strip list. The pref change observer will
    // handle the rest of the work.
    Services.prefs.clearUserPref(PREF_STRIP_LIST_NAME);
    Services.prefs.clearUserPref(PREF_ALLOW_LIST_NAME);
  }

  observe(subject, topic, data) {
    switch (topic) {
      case "xpcom-shutdown":
        this._shutdown();
        break;
      case "nsPref:changed":
        let prefValue = Services.prefs.getStringPref(data, "");
        this._onPrefUpdate(data, prefValue);
        break;
      default:
        Cu.reportError(`Unexpected event ${topic}`);
    }
  }

  handleEvent(event) {
    if (event.type != "change") {
      return;
    }

    if (!event.changedKeys.includes(SHARED_DATA_KEY)) {
      return;
    }

    let data = this._getListFromSharedData();
    this._onRemoteSettingsUpdate(data);
    this._notifyObservers();
  }
}

var EXPORTED_SYMBOLS = ["URLQueryStrippingListService"];
