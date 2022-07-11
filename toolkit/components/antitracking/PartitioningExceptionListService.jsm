/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "RemoteSettings",
  "resource://services-settings/remote-settings.js"
);

const COLLECTION_NAME = "partitioning-exempt-urls";
const PREF_NAME = "privacy.restrict3rdpartystorage.skip_list";

class Feature {
  constructor() {
    this.prefName = PREF_NAME;
    this.observers = new Set();
    this.prefValue = [];
    this.remoteEntries = [];

    if (this.prefName) {
      let prefValue = Services.prefs.getStringPref(this.prefName, null);
      this.prefValue = prefValue ? prefValue.split(";") : [];
      Services.prefs.addObserver(this.prefName, this);
    }
  }

  async addAndRunObserver(observer) {
    this.observers.add(observer);
    this.notifyObservers(observer);
  }

  removeObserver(observer) {
    this.observers.delete(observer);
  }

  observe(subject, topic, data) {
    if (topic != "nsPref:changed" || data != this.prefName) {
      Cu.reportError(`Unexpected event ${topic} with ${data}`);
      return;
    }

    let prefValue = Services.prefs.getStringPref(this.prefName, null);
    this.prefValue = prefValue ? prefValue.split(";") : [];
    this.notifyObservers();
  }

  onRemoteSettingsUpdate(entries) {
    this.remoteEntries = [];

    for (let entry of entries) {
      this.remoteEntries.push(
        `${entry.firstPartyOrigin},${entry.thirdPartyOrigin}`
      );
    }
  }

  notifyObservers(observer = null) {
    let entries = this.prefValue.concat(this.remoteEntries);
    let entriesAsString = entries.join(";").toLowerCase();
    if (observer) {
      observer.onExceptionListUpdate(entriesAsString);
    } else {
      for (let obs of this.observers) {
        obs.onExceptionListUpdate(entriesAsString);
      }
    }
  }
}

function PartitioningExceptionListService() {}

PartitioningExceptionListService.prototype = {
  classID: Components.ID("{ab94809d-33f0-4f28-af38-01efbd3baf22}"),
  QueryInterface: ChromeUtils.generateQI([
    "nsIPartitioningExceptionListService",
  ]),

  _initialized: false,

  async lazyInit() {
    if (this._initialized) {
      return;
    }

    this.feature = new Feature();

    let rs = lazy.RemoteSettings(COLLECTION_NAME);
    rs.on("sync", event => {
      let {
        data: { current },
      } = event;
      this.onUpdateEntries(current);
    });

    this._initialized = true;

    let entries;
    // If the remote settings list hasn't been populated yet we have to make sure
    // to do it before firing the first notification.
    // This has to be run after _initialized is set because we'll be
    // blocked while getting entries from RemoteSetting, and we don't want
    // LazyInit is executed again.
    try {
      // The data will be initially available from the local DB (via a
      // resource:// URI).
      entries = await rs.get();
    } catch (e) {}

    // RemoteSettings.get() could return null, ensure passing a list to
    // onUpdateEntries.
    this.onUpdateEntries(entries || []);
  },

  onUpdateEntries(entries) {
    if (!this.feature) {
      return;
    }
    this.feature.onRemoteSettingsUpdate(entries);
    this.feature.notifyObservers();
  },

  registerAndRunExceptionListObserver(observer) {
    // We don't await this; the caller is C++ and won't await this function,
    // and because we prevent re-entering into this method, once it's been
    // called once any subsequent calls will early-return anyway - so
    // awaiting that would be meaningless. Instead, `Feature` implementations
    // make sure not to call into observers until they have data, and we
    // make sure to let feature instances know whether we have data
    // immediately.
    this.lazyInit();

    this.feature.addAndRunObserver(observer);
  },

  unregisterExceptionListObserver(observer) {
    if (!this.feature) {
      return;
    }
    this.feature.removeObserver(observer);
  },
};

var EXPORTED_SYMBOLS = ["PartitioningExceptionListService"];
