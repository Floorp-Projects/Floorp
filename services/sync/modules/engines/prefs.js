/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["PrefsEngine", "PrefRec"];

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

const PREF_SYNC_PREFS_PREFIX = "services.sync.prefs.sync.";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-common/utils.js");

XPCOMUtils.defineLazyModuleGetter(this, "LightweightThemeManager",
                          "resource://gre/modules/LightweightThemeManager.jsm");

const PREFS_GUID = CommonUtils.encodeBase64URL(Services.appinfo.ID);

this.PrefRec = function PrefRec(collection, id) {
  CryptoWrapper.call(this, collection, id);
}
PrefRec.prototype = {
  __proto__: CryptoWrapper.prototype,
  _logName: "Sync.Record.Pref",
};

Utils.deferGetSet(PrefRec, "cleartext", ["value"]);


this.PrefsEngine = function PrefsEngine(service) {
  SyncEngine.call(this, "Prefs", service);
}
PrefsEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _storeObj: PrefStore,
  _trackerObj: PrefTracker,
  _recordObj: PrefRec,
  version: 2,

  syncPriority: 1,
  allowSkippedRecord: false,

  async getChangedIDs() {
    // No need for a proper timestamp (no conflict resolution needed).
    let changedIDs = {};
    if (this._tracker.modified)
      changedIDs[PREFS_GUID] = 0;
    return changedIDs;
  },

  async _wipeClient() {
    await SyncEngine.prototype._wipeClient.call(this);
    this.justWiped = true;
  },

  async _reconcile(item) {
    // Apply the incoming item if we don't care about the local data
    if (this.justWiped) {
      this.justWiped = false;
      return true;
    }
    return SyncEngine.prototype._reconcile.call(this, item);
  }
};


function PrefStore(name, engine) {
  Store.call(this, name, engine);
  Svc.Obs.add("profile-before-change", function() {
    this.__prefs = null;
  }, this);
}
PrefStore.prototype = {
  __proto__: Store.prototype,

 __prefs: null,
  get _prefs() {
    if (!this.__prefs) {
      this.__prefs = new Preferences();
    }
    return this.__prefs;
  },

  _getSyncPrefs() {
    let syncPrefs = Cc["@mozilla.org/preferences-service;1"]
                      .getService(Ci.nsIPrefService)
                      .getBranch(PREF_SYNC_PREFS_PREFIX)
                      .getChildList("", {});
    // Also sync preferences that determine which prefs get synced.
    let controlPrefs = syncPrefs.map(pref => PREF_SYNC_PREFS_PREFIX + pref);
    return controlPrefs.concat(syncPrefs);
  },

  _isSynced(pref) {
    return pref.startsWith(PREF_SYNC_PREFS_PREFIX) ||
           this._prefs.get(PREF_SYNC_PREFS_PREFIX + pref, false);
  },

  _getAllPrefs() {
    let values = {};
    for (let pref of this._getSyncPrefs()) {
      if (this._isSynced(pref)) {
        // Missing and default prefs get the null value.
        values[pref] = this._prefs.isSet(pref) ? this._prefs.get(pref, null) : null;
      }
    }
    return values;
  },

  _updateLightWeightTheme(themeID) {
    let themeObject = null;
    if (themeID) {
      themeObject = LightweightThemeManager.getUsedTheme(themeID);
    }
    LightweightThemeManager.currentTheme = themeObject;
  },

  _setAllPrefs(values) {
    let selectedThemeIDPref = "lightweightThemes.selectedThemeID";
    let selectedThemeIDBefore = this._prefs.get(selectedThemeIDPref, null);
    let selectedThemeIDAfter = selectedThemeIDBefore;

    // Update 'services.sync.prefs.sync.foo.pref' before 'foo.pref', otherwise
    // _isSynced returns false when 'foo.pref' doesn't exist (e.g., on a new device).
    let prefs = Object.keys(values).sort(a => -a.indexOf(PREF_SYNC_PREFS_PREFIX));
    for (let pref of prefs) {
      if (!this._isSynced(pref)) {
        continue;
      }

      let value = values[pref];

      switch (pref) {
        // Some special prefs we don't want to set directly.
        case selectedThemeIDPref:
          selectedThemeIDAfter = value;
          break;

        // default is to just set the pref
        default:
          if (value == null) {
            // Pref has gone missing. The best we can do is reset it.
            this._prefs.reset(pref);
          } else {
            try {
              this._prefs.set(pref, value);
            } catch (ex) {
              this._log.trace(`Failed to set pref: ${pref}`, ex);
            }
          }
      }
    }

    // Notify the lightweight theme manager if the selected theme has changed.
    if (selectedThemeIDBefore != selectedThemeIDAfter) {
      this._updateLightWeightTheme(selectedThemeIDAfter);
    }
  },

  async getAllIDs() {
    /* We store all prefs in just one WBO, with just one GUID */
    let allprefs = {};
    allprefs[PREFS_GUID] = true;
    return allprefs;
  },

  async changeItemID(oldID, newID) {
    this._log.trace("PrefStore GUID is constant!");
  },

  async itemExists(id) {
    return (id === PREFS_GUID);
  },

  async createRecord(id, collection) {
    let record = new PrefRec(collection, id);

    if (id == PREFS_GUID) {
      record.value = this._getAllPrefs();
    } else {
      record.deleted = true;
    }

    return record;
  },

  async create(record) {
    this._log.trace("Ignoring create request");
  },

  async remove(record) {
    this._log.trace("Ignoring remove request");
  },

  async update(record) {
    // Silently ignore pref updates that are for other apps.
    if (record.id != PREFS_GUID)
      return;

    this._log.trace("Received pref updates, applying...");
    this._setAllPrefs(record.value);
  },

  async wipe() {
    this._log.trace("Ignoring wipe request");
  }
};

function PrefTracker(name, engine) {
  Tracker.call(this, name, engine);
  Svc.Obs.add("profile-before-change", this);
  Svc.Obs.add("weave:engine:start-tracking", this);
  Svc.Obs.add("weave:engine:stop-tracking", this);
}
PrefTracker.prototype = {
  __proto__: Tracker.prototype,

  get modified() {
    return Svc.Prefs.get("engine.prefs.modified", false);
  },
  set modified(value) {
    Svc.Prefs.set("engine.prefs.modified", value);
  },

  clearChangedIDs: function clearChangedIDs() {
    this.modified = false;
  },

 __prefs: null,
  get _prefs() {
    if (!this.__prefs) {
      this.__prefs = new Preferences();
    }
    return this.__prefs;
  },

  startTracking() {
    Services.prefs.addObserver("", this);
  },

  stopTracking() {
    this.__prefs = null;
    Services.prefs.removeObserver("", this);
  },

  observe(subject, topic, data) {
    Tracker.prototype.observe.call(this, subject, topic, data);

    switch (topic) {
      case "profile-before-change":
        this.stopTracking();
        break;
      case "nsPref:changed":
        if (this.ignoreAll) {
          break;
        }
        // Trigger a sync for MULTI-DEVICE for a change that determines
        // which prefs are synced or a regular pref change.
        if (data.indexOf(PREF_SYNC_PREFS_PREFIX) == 0 ||
            this._prefs.get(PREF_SYNC_PREFS_PREFIX + data, false)) {
          this.score += SCORE_INCREMENT_XLARGE;
          this.modified = true;
          this._log.trace("Preference " + data + " changed");
        }
        break;
    }
  }
};
