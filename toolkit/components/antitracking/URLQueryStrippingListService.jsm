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
  RemoteSettings: "resource://services-settings/remote-settings.js",
});

const COLLECTION_NAME = "query-stripping";

const PREF_STRIP_LIST_NAME = "privacy.query_stripping.strip_list";
const PREF_ALLOW_LIST_NAME = "privacy.query_stripping.allow_list";

const CONTENT_PROCESS_SCRIPT =
  "resource://gre/modules/URLQueryStrippingListProcessScript.js";

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
    this.isParentProcess =
      Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_DEFAULT;

    this._initPromise = this._init();
  }

  async _init() {
    // We can only access the remote settings in the parent process. For content
    // processes, we will broadcast the lists once the lists get updated in the
    // parent.
    if (this.isParentProcess) {
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

      Services.ppmm.addMessageListener("query-stripping:request-rs", this);

      // We need the services to be initialized early, at least before the
      // stripping happens. To achieve that, we will register the process script
      // which will be executed every time a content process been created. The
      // process script will try to get the service to init it so that the lists
      // will be ready when we do the stripping.
      //
      // Note that we need to do this for Fission because 'profile-after-change'
      // won't be triggered in content processes.
      Services.ppmm.loadProcessScript(CONTENT_PROCESS_SCRIPT, true);
    } else {
      // Register the message listener for the remote settings update from the
      // parent process. We also send a message to the parent to get remote
      // settings data.
      Services.cpmm.addMessageListener("query-stripping:rs-updated", this);
      Services.cpmm.addMessageListener("query-stripping:clear-lists", this);
      Services.cpmm.sendAsyncMessage("query-stripping:request-rs");
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

    this.initialized = true;
  }

  async _shutdown() {
    Services.obs.removeObserver(this, "xpcom-shutdown");
    Services.prefs.removeObserver(PREF_STRIP_LIST_NAME, this);
    Services.prefs.removeObserver(PREF_ALLOW_LIST_NAME, this);

    if (this.isParentProcess) {
      Services.ppmm.removeMessageListener("query-stripping:request-rs", this);
      Services.ppmm.removeDelayedProcessScript(CONTENT_PROCESS_SCRIPT);
    } else {
      Services.cpmm.removeMessageListener("query-stripping:rs-updated", this);
      Services.cpmm.removeMessageListener("query-stripping:clear-lists", this);
    }
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

    // Because only the parent process will get the remote settings update, so
    // we will broadcast the update to every content processes so that the
    // content processes will have the lists from remote settings.
    if (this.isParentProcess) {
      Services.ppmm.broadcastAsyncMessage(
        "query-stripping:rs-updated",
        entries
      );
    }

    this._notifyObservers();
  }

  _onPrefUpdate(pref, value) {
    switch (pref) {
      case PREF_STRIP_LIST_NAME:
        this.prefStripList = value ? value.split(" ") : [];
        break;

      case PREF_ALLOW_LIST_NAME:
        this.prefAllowList = value ? value.split(",") : [];
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
    this.remoteStripList = [];
    this.remoteAllowList = [];
    this.prefStripList = [];
    this.prefAllowList = [];

    if (this.isParentProcess) {
      Services.ppmm.broadcastAsyncMessage("query-stripping:clear-lists");
    }
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

  receiveMessage({ target, name, data }) {
    if (name == "query-stripping:rs-updated") {
      this._onRemoteSettingsUpdate(data);
    } else if (name == "query-stripping:request-rs") {
      target.sendAsyncMessage("query-stripping:rs-updated", [
        { stripList: this.remoteStripList, allowList: this.remoteAllowList },
      ]);
    } else if (name == "query-stripping:clear-lists") {
      this.clearLists();
    }
  }
}

var EXPORTED_SYMBOLS = ["URLQueryStrippingListService"];
