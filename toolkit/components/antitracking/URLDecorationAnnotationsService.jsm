/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.URLDecorationAnnotationsService = function() {};

ChromeUtils.defineModuleGetter(
  this,
  "RemoteSettings",
  "resource://services-settings/remote-settings.js"
);

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const COLLECTION_NAME = "anti-tracking-url-decoration";
const PREF_NAME = "privacy.restrict3rdpartystorage.url_decorations";

URLDecorationAnnotationsService.prototype = {
  classID: Components.ID("{5874af6d-5719-4e1b-b155-ef4eae7fcb32}"),
  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsIURLDecorationAnnotationsService",
  ]),

  _initialized: false,
  _prefBranch: null,

  onDataAvailable(entries) {
    // Use this technique in order to ensure the pref cannot be changed by the
    // user e.g. through about:config.  This preferences is only intended as a
    // mechanism for reflecting this data to content processes.
    if (this._prefBranch === null) {
      this._prefBranch = Services.prefs.getDefaultBranch("");
    }

    const branch = this._prefBranch;
    branch.unlockPref(PREF_NAME);
    branch.setStringPref(
      PREF_NAME,
      entries.map(x => x.token.replace(/ /, "%20")).join(" ")
    );
    branch.lockPref(PREF_NAME);
  },

  observe(aSubject, aTopic, aData) {
    if (aTopic == "profile-after-change") {
      this.ensureUpdated();
    }
  },

  ensureUpdated() {
    if (this._initialized) {
      return Promise.resolve();
    }
    this._initialized = true;

    const client = RemoteSettings(COLLECTION_NAME);
    client.on("sync", event => {
      let {
        data: { current },
      } = event;
      this.onDataAvailable(current);
    });

    // Now trigger an update from the server if necessary to get a fresh copy
    // of the data
    return client.get({}).then(entries => {
      this.onDataAvailable(entries);
      return undefined;
    });
  },
};

var EXPORTED_SYMBOLS = ["URLDecorationAnnotationsService"];
