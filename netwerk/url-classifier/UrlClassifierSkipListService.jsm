/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.UrlClassifierSkipListService = function() {};

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "RemoteSettings",
                               "resource://services-settings/remote-settings.js");

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

    RemoteSettings(COLLECTION_NAME).on("sync", event => {
      let { data: { current } } = event;
      this.remoteEntries = current;
      this.notifyObservers();
    });
  }

  async addObserver(observer) {
    // If the remote settings list hasn't been populated yet we have to make sure
    // to do it before firing the first notification.
    if (!this.remoteEntries) {
      this.remoteEntries = await RemoteSettings(COLLECTION_NAME).get({ syncIfEmpty: false });
    }

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

  notifyObservers(observer = null) {
    let entries = [];
    if (this.prefValue) {
      entries = this.prefValue.split(",");
    }

    for (let entry of this.remoteEntries) {
      if (entry.feature == this.name) {
        entries.push(entry.pattern.toLowerCase());
      }
    }

    let entriesAsString = entries.join(",");
    if (observer) {
      observer.onSkipListUpdate(entriesAsString);
    } else {
      for (let obs of this.observers) {
        obs.onSkipListUpdate(entriesAsString);
      }
    }
  }
}

UrlClassifierSkipListService.prototype = Object.freeze({
  classID: Components.ID("{b9f4fd03-9d87-4bfd-9958-85a821750ddc}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIUrlClassifierSkipListService]),

  features: {},

  registerAndRunSkipListObserver(feature, prefName, observer) {
    if (!this.features[feature]) {
      this.features[feature] = new Feature(feature, prefName);
    }
    this.features[feature].addObserver(observer);
  },

  unregisterSkipListObserver(feature, observer) {
    if (!this.features[feature]) {
      return;
    }
    this.features[feature].removeObserver(observer);
  },
});

var EXPORTED_SYMBOLS = ["UrlClassifierSkipListService"];
