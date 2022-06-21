/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["PrefsEngine", "PrefRec", "getPrefsGUIDForTest"];

const PREF_SYNC_PREFS_PREFIX = "services.sync.prefs.sync.";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);
const { Store, SyncEngine, Tracker } = ChromeUtils.import(
  "resource://services-sync/engines.js"
);
const { CryptoWrapper } = ChromeUtils.import(
  "resource://services-sync/record.js"
);
const { Svc, Utils } = ChromeUtils.import("resource://services-sync/util.js");
const { SCORE_INCREMENT_XLARGE } = ChromeUtils.import(
  "resource://services-sync/constants.js"
);
const { CommonUtils } = ChromeUtils.import(
  "resource://services-common/utils.js"
);

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "PREFS_GUID", () =>
  CommonUtils.encodeBase64URL(Services.appinfo.ID)
);

ChromeUtils.defineModuleGetter(
  lazy,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);

// In bug 1538015, we decided that it isn't always safe to allow all "incoming"
// preferences to be applied locally. So we have introduced another preference,
// which if false (the default) will ignore all incoming preferences which don't
// already have the "control" preference locally set. If this new
// preference is set to true, then we continue our old behavior of allowing all
// preferences to be updated, even those which don't already have a local
// "control" pref.
const PREF_SYNC_PREFS_ARBITRARY =
  "services.sync.prefs.dangerously_allow_arbitrary";

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "ALLOW_ARBITRARY",
  PREF_SYNC_PREFS_ARBITRARY
);

// The SUMO supplied URL we log with more information about how custom prefs can
// continue to be synced. SUMO have told us that this URL will remain "stable".
const PREFS_DOC_URL_TEMPLATE =
  "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/sync-custom-preferences";
XPCOMUtils.defineLazyGetter(lazy, "PREFS_DOC_URL", () =>
  Services.urlFormatter.formatURL(PREFS_DOC_URL_TEMPLATE)
);

// Check for a local control pref or PREF_SYNC_PREFS_ARBITRARY
function isAllowedPrefName(prefName) {
  if (prefName == PREF_SYNC_PREFS_ARBITRARY) {
    return false; // never allow this.
  }
  if (lazy.ALLOW_ARBITRARY) {
    // user has set the "dangerous" pref, so everything is allowed.
    return true;
  }
  // The pref must already have a control pref set, although it doesn't matter
  // here whether that value is true or false. We can't use prefHasUserValue
  // here because we also want to check prefs still with default values.
  try {
    Services.prefs.getBoolPref(PREF_SYNC_PREFS_PREFIX + prefName);
    // pref exists!
    return true;
  } catch (_) {
    return false;
  }
}

function PrefRec(collection, id) {
  CryptoWrapper.call(this, collection, id);
}
PrefRec.prototype = {
  __proto__: CryptoWrapper.prototype,
  _logName: "Sync.Record.Pref",
};

Utils.deferGetSet(PrefRec, "cleartext", ["value"]);

function PrefsEngine(service) {
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
    if (this._tracker.modified) {
      changedIDs[lazy.PREFS_GUID] = 0;
    }
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
  },

  async trackRemainingChanges() {
    if (this._modified.count() > 0) {
      this._tracker.modified = true;
    }
  },
};

// We don't use services.sync.engine.tabs.filteredSchemes since it includes
// about: pages and the like, which we want to be syncable in preferences.
// Blob, moz-extension, data and file uris are never safe to sync,
// so we limit our check to those.
const UNSYNCABLE_URL_REGEXP = /^(moz-extension|blob|data|file):/i;
function isUnsyncableURLPref(prefName) {
  if (Services.prefs.getPrefType(prefName) != Ci.nsIPrefBranch.PREF_STRING) {
    return false;
  }
  const prefValue = Services.prefs.getStringPref(prefName, "");
  return UNSYNCABLE_URL_REGEXP.test(prefValue);
}

function PrefStore(name, engine) {
  Store.call(this, name, engine);
  Svc.Obs.add(
    "profile-before-change",
    function() {
      this.__prefs = null;
    },
    this
  );
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
    let syncPrefs = Services.prefs
      .getBranch(PREF_SYNC_PREFS_PREFIX)
      .getChildList("")
      .filter(pref => isAllowedPrefName(pref) && !isUnsyncableURLPref(pref));
    // Also sync preferences that determine which prefs get synced.
    let controlPrefs = syncPrefs.map(pref => PREF_SYNC_PREFS_PREFIX + pref);
    return controlPrefs.concat(syncPrefs);
  },

  _isSynced(pref) {
    if (pref.startsWith(PREF_SYNC_PREFS_PREFIX)) {
      // this is an incoming control pref, which is ignored if there's not already
      // a local control pref for the preference.
      let controlledPref = pref.slice(PREF_SYNC_PREFS_PREFIX.length);
      return isAllowedPrefName(controlledPref);
    }

    // This is the pref itself - it must be both allowed, and have a control
    // pref which is true.
    if (!this._prefs.get(PREF_SYNC_PREFS_PREFIX + pref, false)) {
      return false;
    }
    return isAllowedPrefName(pref);
  },

  _getAllPrefs() {
    let values = {};
    for (let pref of this._getSyncPrefs()) {
      // Note: _isSynced doesn't call isUnsyncableURLPref since it would cause
      // us not to apply (syncable) changes to preferences that are set locally
      // which have unsyncable urls.
      if (this._isSynced(pref) && !isUnsyncableURLPref(pref)) {
        // Missing and default prefs get the null value.
        values[pref] = this._prefs.isSet(pref)
          ? this._prefs.get(pref, null)
          : null;
      }
    }
    return values;
  },

  _setAllPrefs(values) {
    const selectedThemeIDPref = "extensions.activeThemeID";
    let selectedThemeIDBefore = this._prefs.get(selectedThemeIDPref, null);
    let selectedThemeIDAfter = selectedThemeIDBefore;

    // Update 'services.sync.prefs.sync.foo.pref' before 'foo.pref', otherwise
    // _isSynced returns false when 'foo.pref' doesn't exist (e.g., on a new device).
    let prefs = Object.keys(values).sort(
      a => -a.indexOf(PREF_SYNC_PREFS_PREFIX)
    );
    for (let pref of prefs) {
      let value = values[pref];
      if (!this._isSynced(pref)) {
        // An extra complication just so we can warn when we decline to sync a
        // preference due to no local control pref.
        if (!pref.startsWith(PREF_SYNC_PREFS_PREFIX)) {
          // this is an incoming pref - if the incoming value is not null and
          // there's no local control pref, then it means we would have previously
          // applied a value, but now will decline to.
          // We need to check this here rather than in _isSynced because the
          // default list of prefs we sync has changed, so we don't want to report
          // this message when we wouldn't have actually applied a value.
          // We should probably remove all of this in ~ Firefox 80.
          if (value !== null) {
            // null means "use the default value"
            let controlPref = PREF_SYNC_PREFS_PREFIX + pref;
            let controlPrefExists;
            try {
              Services.prefs.getBoolPref(controlPref);
              controlPrefExists = true;
            } catch (ex) {
              controlPrefExists = false;
            }
            if (!controlPrefExists) {
              // This is a long message and written to both the sync log and the
              // console, but note that users who have not customized the control
              // prefs will never see this.
              let msg =
                `Not syncing the preference '${pref}' because it has no local ` +
                `control preference (${PREF_SYNC_PREFS_PREFIX}${pref}) and ` +
                `the preference ${PREF_SYNC_PREFS_ARBITRARY} isn't true. ` +
                `See ${lazy.PREFS_DOC_URL} for more information`;
              console.warn(msg);
              this._log.warn(msg);
            }
          }
        }
        continue;
      }

      if (typeof value == "string" && UNSYNCABLE_URL_REGEXP.test(value)) {
        this._log.trace(`Skipping incoming unsyncable url for pref: ${pref}`);
        continue;
      }

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
    // Themes are a little messy. Themes which have been installed are handled
    // by the addons engine - but default themes aren't seen by that engine.
    // So if there's a new default theme ID and that ID corresponds to a
    // system addon, then we arrange to enable that addon here.
    if (selectedThemeIDBefore != selectedThemeIDAfter) {
      this._maybeEnableBuiltinTheme(selectedThemeIDAfter).catch(e => {
        this._log.error("Failed to maybe update the default theme", e);
      });
    }
  },

  async _maybeEnableBuiltinTheme(themeId) {
    let addon = null;
    try {
      addon = await lazy.AddonManager.getAddonByID(themeId);
    } catch (ex) {
      this._log.trace(
        `There's no addon with ID '${themeId} - it can't be a builtin theme`
      );
      return;
    }
    if (addon && addon.isBuiltin && addon.type == "theme") {
      this._log.trace(`Enabling builtin theme '${themeId}'`);
      await addon.enable();
    } else {
      this._log.trace(
        `Have incoming theme ID of '${themeId}' but it's not a builtin theme`
      );
    }
  },

  async getAllIDs() {
    /* We store all prefs in just one WBO, with just one GUID */
    let allprefs = {};
    allprefs[lazy.PREFS_GUID] = true;
    return allprefs;
  },

  async changeItemID(oldID, newID) {
    this._log.trace("PrefStore GUID is constant!");
  },

  async itemExists(id) {
    return id === lazy.PREFS_GUID;
  },

  async createRecord(id, collection) {
    let record = new PrefRec(collection, id);

    if (id == lazy.PREFS_GUID) {
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
    if (record.id != lazy.PREFS_GUID) {
      return;
    }

    this._log.trace("Received pref updates, applying...");
    this._setAllPrefs(record.value);
  },

  async wipe() {
    this._log.trace("Ignoring wipe request");
  },
};

function PrefTracker(name, engine) {
  Tracker.call(this, name, engine);
  this._ignoreAll = false;
  Svc.Obs.add("profile-before-change", this.asyncObserver);
}
PrefTracker.prototype = {
  __proto__: Tracker.prototype,

  get ignoreAll() {
    return this._ignoreAll;
  },

  set ignoreAll(value) {
    this._ignoreAll = value;
  },

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

  onStart() {
    Services.prefs.addObserver("", this.asyncObserver);
  },

  onStop() {
    this.__prefs = null;
    Services.prefs.removeObserver("", this.asyncObserver);
  },

  async observe(subject, topic, data) {
    switch (topic) {
      case "profile-before-change":
        await this.stop();
        break;
      case "nsPref:changed":
        if (this.ignoreAll) {
          break;
        }
        // Trigger a sync for MULTI-DEVICE for a change that determines
        // which prefs are synced or a regular pref change.
        if (
          data.indexOf(PREF_SYNC_PREFS_PREFIX) == 0 ||
          this._prefs.get(PREF_SYNC_PREFS_PREFIX + data, false)
        ) {
          this.score += SCORE_INCREMENT_XLARGE;
          this.modified = true;
          this._log.trace("Preference " + data + " changed");
        }
        break;
    }
  },
};

function getPrefsGUIDForTest() {
  return lazy.PREFS_GUID;
}
