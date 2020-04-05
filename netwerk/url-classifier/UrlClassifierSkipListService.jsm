/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.UrlClassifierSkipListService = function() {};

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "RemoteSettings",
  "resource://services-settings/remote-settings.js"
);

const COLLECTION_NAME = "url-classifier-skip-urls";

class Feature {
  constructor(name, prefName) {
    this.name = name;
    this.prefName = prefName;
    this.observers = new Set();
    this.prefValue = null;
    this.remoteEntries = null;

    if (prefName) {
      this.prefValue = Services.prefs.getStringPref(this.prefName, null);
      Services.prefs.addObserver(prefName, this);
    }
  }

  async addObserver(observer) {
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

    this.prefValue = Services.prefs.getStringPref(this.prefName, null);
    this.notifyObservers();
  }

  onRemoteSettingsUpdate(entries) {
    this.remoteEntries = [];

    for (let entry of entries) {
      if (entry.feature == this.name) {
        this.remoteEntries.push(entry.pattern.toLowerCase());
      }
    }
  }

  notifyObservers(observer = null) {
    // Don't notify until we have the initial data.
    if (!this.remoteEntries) {
      return;
    }

    let entries = [];
    if (this.prefValue) {
      entries = this.prefValue.split(",");
    }

    for (let entry of this.remoteEntries) {
      entries.push(entry);
    }

    let entriesAsString = entries.join(",").toLowerCase();
    if (observer) {
      observer.onSkipListUpdate(entriesAsString);
    } else {
      for (let obs of this.observers) {
        obs.onSkipListUpdate(entriesAsString);
      }
    }
  }
}

UrlClassifierSkipListService.prototype = {
  classID: Components.ID("{b9f4fd03-9d87-4bfd-9958-85a821750ddc}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIUrlClassifierSkipListService]),

  features: {},
  _initialized: false,

  async lazyInit() {
    if (this._initialized) {
      return;
    }

    let rs = RemoteSettings(COLLECTION_NAME);
    rs.on("sync", event => {
      let {
        data: { current },
      } = event;
      this.entries = current || [];
      this.onUpdateEntries(current);
    });

    this._initialized = true;

    // If the remote settings list hasn't been populated yet we have to make sure
    // to do it before firing the first notification.
    // This has to be run after _initialized is set because we'll be
    // blocked while getting entries from RemoteSetting, and we don't want
    // LazyInit is executed again.
    try {
      // The data will be initially available from the local DB (via a
      // resource:// URI).
      this.entries = await rs.get();
    } catch (e) {}

    // RemoteSettings.get() could return null, ensure passing a list to
    // onUpdateEntries.
    if (!this.entries) {
      this.entries = [];
    }

    this.onUpdateEntries(this.entries);
  },

  onUpdateEntries(entries) {
    for (let key of Object.keys(this.features)) {
      let feature = this.features[key];
      feature.onRemoteSettingsUpdate(entries);
      feature.notifyObservers();
    }
  },

  registerAndRunSkipListObserver(feature, prefName, observer) {
    // We don't await this; the caller is C++ and won't await this function,
    // and because we prevent re-entering into this method, once it's been
    // called once any subsequent calls will early-return anyway - so
    // awaiting that would be meaningless. Instead, `Feature` implementations
    // make sure not to call into observers until they have data, and we
    // make sure to let feature instances know whether we have data
    // immediately.
    this.lazyInit();

    if (!this.features[feature]) {
      let featureObj = new Feature(feature, prefName);
      this.features[feature] = featureObj;
      // If we've previously initialized, we need to pass the entries
      // we already have to the new feature.
      if (this.entries) {
        featureObj.onRemoteSettingsUpdate(this.entries);
      }
    }
    this.features[feature].addObserver(observer);
  },

  unregisterSkipListObserver(feature, observer) {
    if (!this.features[feature]) {
      return;
    }
    this.features[feature].removeObserver(observer);
  },
};

var EXPORTED_SYMBOLS = ["UrlClassifierSkipListService"];
