/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Prefs which start with this prefix are our "control" prefs - they indicate
// which preferences should be synced.
const PREF_SYNC_PREFS_PREFIX = "services.sync.prefs.sync.";

// Prefs which have a default value are usually not synced - however, if the
// preference exists under this prefix and the value is:
// * `true`, then we do sync default values.
// * `false`, then as soon as we ever sync a non-default value out, or sync
//    any value in, then we toggle the value to `true`.
//
// We never explicitly set this pref back to false, so it's one-shot.
// Some preferences which are known to have a different default value on
// different platforms have this preference with a default value of `false`,
// so they don't sync until one device changes to the non-default value, then
// that value forever syncs, even if it gets reset back to the default.
// Note that preferences handled this way *must also* have the "normal"
// control pref set.
// A possible future enhancement would be to sync these prefs so that
// other distributions can flag them if they change the default, but that
// doesn't seem worthwhile until we can be confident they'd actually create
// this special control pref at the same time they flip the default.
const PREF_SYNC_SEEN_PREFIX = "services.sync.prefs.sync-seen.";

import {
  Store,
  SyncEngine,
  Tracker,
} from "resource://services-sync/engines.sys.mjs";
import { CryptoWrapper } from "resource://services-sync/record.sys.mjs";
import { Svc, Utils } from "resource://services-sync/util.sys.mjs";
import { SCORE_INCREMENT_XLARGE } from "resource://services-sync/constants.sys.mjs";
import { CommonUtils } from "resource://services-common/utils.sys.mjs";

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "PREFS_GUID", () =>
  CommonUtils.encodeBase64URL(Services.appinfo.ID)
);

ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
});

// In bug 1538015, we decided that it isn't always safe to allow all "incoming"
// preferences to be applied locally. So we introduced another preference to control
// this for backward compatibility. We removed that capability in bug 1854698, but in the
// interests of working well between different versions of Firefox, we still forever
// want to prevent this preference from syncing.
// This was the name of the "control" pref.
const PREF_SYNC_PREFS_ARBITRARY =
  "services.sync.prefs.dangerously_allow_arbitrary";

// Check for a local control pref or PREF_SYNC_PREFS_ARBITRARY
function isAllowedPrefName(prefName) {
  if (prefName == PREF_SYNC_PREFS_ARBITRARY) {
    return false; // never allow this.
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

export function PrefRec(collection, id) {
  CryptoWrapper.call(this, collection, id);
}

PrefRec.prototype = {
  _logName: "Sync.Record.Pref",
};
Object.setPrototypeOf(PrefRec.prototype, CryptoWrapper.prototype);

Utils.deferGetSet(PrefRec, "cleartext", ["value"]);

export function PrefsEngine(service) {
  SyncEngine.call(this, "Prefs", service);
}

PrefsEngine.prototype = {
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

  async _uploadOutgoing() {
    try {
      await SyncEngine.prototype._uploadOutgoing.call(this);
    } finally {
      this._store._incomingPrefs = null;
    }
  },

  async trackRemainingChanges() {
    if (this._modified.count() > 0) {
      this._tracker.modified = true;
    }
  },
};
Object.setPrototypeOf(PrefsEngine.prototype, SyncEngine.prototype);

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
    function () {
      this.__prefs = null;
    },
    this
  );
}
PrefStore.prototype = {
  __prefs: null,
  // used just for logging so we can work out why we chose to re-upload
  _incomingPrefs: null,
  get _prefs() {
    if (!this.__prefs) {
      this.__prefs = Services.prefs.getBranch("");
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
    if (!this._prefs.getBoolPref(PREF_SYNC_PREFS_PREFIX + pref, false)) {
      return false;
    }
    return isAllowedPrefName(pref);
  },

  // Given a preference name, returns either a string, bool, number or null.
  _getPrefValue(pref) {
    switch (this._prefs.getPrefType(pref)) {
      case Ci.nsIPrefBranch.PREF_STRING:
        return this._prefs.getStringPref(pref);
      case Ci.nsIPrefBranch.PREF_INT:
        return this._prefs.getIntPref(pref);
      case Ci.nsIPrefBranch.PREF_BOOL:
        return this._prefs.getBoolPref(pref);
      //  case Ci.nsIPrefBranch.PREF_INVALID: handled by the fallthrough
    }
    return null;
  },

  _getAllPrefs() {
    let values = {};
    for (let pref of this._getSyncPrefs()) {
      // Note: _isSynced doesn't call isUnsyncableURLPref since it would cause
      // us not to apply (syncable) changes to preferences that are set locally
      // which have unsyncable urls.
      if (this._isSynced(pref) && !isUnsyncableURLPref(pref)) {
        let isSet = this._prefs.prefHasUserValue(pref);
        // Missing and default prefs get the null value, unless that `seen`
        // pref is set, in which case it always gets the value.
        let forceValue = this._prefs.getBoolPref(
          PREF_SYNC_SEEN_PREFIX + pref,
          false
        );
        if (isSet || forceValue) {
          values[pref] = this._getPrefValue(pref);
        } else {
          values[pref] = null;
        }
        // If incoming and outgoing don't match then either the user toggled a
        // pref that doesn't match an incoming non-default value for that pref
        // during a sync (unlikely!) or it refused to stick and is behaving oddly.
        if (this._incomingPrefs) {
          let inValue = this._incomingPrefs[pref];
          let outValue = values[pref];
          if (inValue != null && outValue != null && inValue != outValue) {
            this._log.debug(`Incoming pref '${pref}' refused to stick?`);
            this._log.trace(`Incoming: '${inValue}', outgoing: '${outValue}'`);
          }
        }
        // If this is a special "sync-seen" pref, and it's not the default value,
        // set the seen pref to true.
        if (
          isSet &&
          this._prefs.getBoolPref(PREF_SYNC_SEEN_PREFIX + pref, false) === false
        ) {
          this._log.trace(`toggling sync-seen pref for '${pref}' to true`);
          this._prefs.setBoolPref(PREF_SYNC_SEEN_PREFIX + pref, true);
        }
      }
    }
    return values;
  },

  _maybeLogPrefChange(pref, incomingValue, existingValue) {
    if (incomingValue != existingValue) {
      this._log.debug(`Adjusting preference "${pref}" to the incoming value`);
      // values are PII, so must only be logged at trace.
      this._log.trace(`Existing: ${existingValue}. Incoming: ${incomingValue}`);
    }
  },

  _setAllPrefs(values) {
    const selectedThemeIDPref = "extensions.activeThemeID";
    let selectedThemeIDBefore = this._prefs.getCharPref(
      selectedThemeIDPref,
      null
    );
    let selectedThemeIDAfter = selectedThemeIDBefore;

    // Update 'services.sync.prefs.sync.foo.pref' before 'foo.pref', otherwise
    // _isSynced returns false when 'foo.pref' doesn't exist (e.g., on a new device).
    let prefs = Object.keys(values).sort(
      a => -a.indexOf(PREF_SYNC_PREFS_PREFIX)
    );
    for (let pref of prefs) {
      let value = values[pref];
      if (!this._isSynced(pref)) {
        // It's unusual for us to find an incoming preference (ie, a pref some other
        // instance thinks is syncable) which we don't think is syncable.
        this._log.trace(`Ignoring incoming unsyncable preference "${pref}"`);
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
            if (this._prefs.prefHasUserValue(pref)) {
              this._log.debug(`Clearing existing local preference "${pref}"`);
              this._log.trace(
                `Existing local value for preference: ${this._getPrefValue(
                  pref
                )}`
              );
            }
            this._prefs.clearUserPref(pref);
          } else {
            try {
              switch (typeof value) {
                case "string":
                  this._maybeLogPrefChange(
                    pref,
                    value,
                    this._prefs.getStringPref(pref, undefined)
                  );
                  this._prefs.setStringPref(pref, value);
                  break;
                case "number":
                  this._maybeLogPrefChange(
                    pref,
                    value,
                    this._prefs.getIntPref(pref, undefined)
                  );
                  this._prefs.setIntPref(pref, value);
                  break;
                case "boolean":
                  this._maybeLogPrefChange(
                    pref,
                    value,
                    this._prefs.getBoolPref(pref, undefined)
                  );
                  this._prefs.setBoolPref(pref, value);
                  break;
              }
            } catch (ex) {
              this._log.trace(`Failed to set pref: ${pref}`, ex);
            }
          }
          // If there's a "sync-seen" pref for this it gets toggled to true
          // regardless of the value.
          let seenPref = PREF_SYNC_SEEN_PREFIX + pref;
          if (
            this._prefs.getPrefType(seenPref) != Ci.nsIPrefBranch.PREF_INVALID
          ) {
            this._prefs.setBoolPref(PREF_SYNC_SEEN_PREFIX + pref, true);
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
    this._incomingPrefs = record.value;
    this._setAllPrefs(record.value);
  },

  async wipe() {
    this._log.trace("Ignoring wipe request");
  },
};
Object.setPrototypeOf(PrefStore.prototype, Store.prototype);

function PrefTracker(name, engine) {
  Tracker.call(this, name, engine);
  this._ignoreAll = false;
  Svc.Obs.add("profile-before-change", this.asyncObserver);
}
PrefTracker.prototype = {
  get ignoreAll() {
    return this._ignoreAll;
  },

  set ignoreAll(value) {
    this._ignoreAll = value;
  },

  get modified() {
    return Svc.PrefBranch.getBoolPref("engine.prefs.modified", false);
  },
  set modified(value) {
    Svc.PrefBranch.setBoolPref("engine.prefs.modified", value);
  },

  clearChangedIDs: function clearChangedIDs() {
    this.modified = false;
  },

  __prefs: null,
  get _prefs() {
    if (!this.__prefs) {
      this.__prefs = Services.prefs.getBranch("");
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
          this._prefs.getBoolPref(PREF_SYNC_PREFS_PREFIX + data, false)
        ) {
          this.score += SCORE_INCREMENT_XLARGE;
          this.modified = true;
          this._log.trace("Preference " + data + " changed");
        }
        break;
    }
  },
};
Object.setPrototypeOf(PrefTracker.prototype, Tracker.prototype);

export function getPrefsGUIDForTest() {
  return lazy.PREFS_GUID;
}
