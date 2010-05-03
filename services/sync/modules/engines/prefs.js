/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Bookmarks Sync.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Anant Narayanan <anant@kix.in>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const EXPORTED_SYMBOLS = ['PrefsEngine'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const WEAVE_SYNC_PREFS = "extensions.weave.prefs.sync.";
const WEAVE_PREFS_GUID = "preferences";

Cu.import("resource://weave/util.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/stores.js");
Cu.import("resource://weave/trackers.js");
Cu.import("resource://weave/type_records/prefs.js");

function PrefsEngine() {
  SyncEngine.call(this, "Prefs");
}
PrefsEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _storeObj: PrefStore,
  _trackerObj: PrefTracker,
  _recordObj: PrefRec,

  _wipeClient: function _wipeClient() {
    SyncEngine.prototype._wipeClient.call(this);
    this.justWiped = true;
  },

  _reconcile: function _reconcile(item) {
    // Apply the incoming item if we don't care about the local data
    if (this.justWiped) {
      this.justWiped = false;
      return true;
    }
    return SyncEngine.prototype._reconcile.call(this, item);
  }
};


function PrefStore(name) {
  Store.call(this, name);
}
PrefStore.prototype = {
  __proto__: Store.prototype,

  get _prefs() {
    let prefs = Cc["@mozilla.org/preferences-service;1"].
      getService(Ci.nsIPrefBranch);

    this.__defineGetter__("_prefs", function() prefs);
    return prefs;
  },

  get _syncPrefs() {
    let service = Cc["@mozilla.org/preferences-service;1"].
      getService(Ci.nsIPrefService);
    let syncPrefs = service.getBranch(WEAVE_SYNC_PREFS).getChildList("", {});

    this.__defineGetter__("_syncPrefs", function() syncPrefs);
    return syncPrefs;
  },

  _getAllPrefs: function PrefStore__getAllPrefs() {
    let values = [];
    let toSync = this._syncPrefs;

    let pref;
    for (let i = 0; i < toSync.length; i++) {
      if (!this._prefs.getBoolPref(WEAVE_SYNC_PREFS + toSync[i]))
        continue;

      pref = {};
      pref["name"] = toSync[i];

      switch (this._prefs.getPrefType(toSync[i])) {
        case Ci.nsIPrefBranch.PREF_INT:
          pref["type"] = "int";
          pref["value"] = this._prefs.getIntPref(toSync[i]);
          break;
        case Ci.nsIPrefBranch.PREF_STRING:
          pref["type"] = "string";
          pref["value"] = this._prefs.getCharPref(toSync[i]);
          break;
        case Ci.nsIPrefBranch.PREF_BOOL:
          pref["type"] = "boolean";
          pref["value"] = this._prefs.getBoolPref(toSync[i]);
          break;
        default:
          this._log.trace("Unsupported pref type for " + toSync[i]);
      }
      if ("value" in pref)
        values[values.length] = pref;
    }

    return values;
  },

  _setAllPrefs: function PrefStore__setAllPrefs(values) {
    // cache 
    let ltmExists = true;
    let ltm = {};
    let enabledBefore = false;
    let prevTheme = "";
    try {
      Cu.import("resource://gre/modules/LightweightThemeManager.jsm", ltm);
      ltm = ltm.LightweightThemeManager;
      enabledBefore = this._prefs.getBoolPref("lightweightThemes.isThemeSelected");
      prevTheme = ltm.currentTheme;
    } catch(ex) {
      ltmExists = false;
    } // LightweightThemeManager only exists in Firefox 3.6+
    
    for (let i = 0; i < values.length; i++) {
      switch (values[i]["type"]) {
        case "int":
          this._prefs.setIntPref(values[i]["name"], values[i]["value"]);
          break;
        case "string":
          this._prefs.setCharPref(values[i]["name"], values[i]["value"]);
          break;
        case "boolean":
          this._prefs.setBoolPref(values[i]["name"], values[i]["value"]);
          break;
        default:
          this._log.trace("Unexpected preference type: " + values[i]["type"]);
      }
    }

    // Notify the lightweight theme manager of all the new values
    if (ltmExists) {
      let enabledNow = this._prefs.getBoolPref("lightweightThemes.isThemeSelected");    
      if (enabledBefore && !enabledNow)
        ltm.currentTheme = null;
      else if (enabledNow && ltm.usedThemes[0] != prevTheme) {
        ltm.currentTheme = null;
        ltm.currentTheme = ltm.usedThemes[0];
      }
    }
  },

  getAllIDs: function PrefStore_getAllIDs() {
    /* We store all prefs in just one WBO, with just one GUID */
    let allprefs = {};
    allprefs[WEAVE_PREFS_GUID] = this._getAllPrefs();
    return allprefs;
  },

  changeItemID: function PrefStore_changeItemID(oldID, newID) {
    this._log.trace("PrefStore GUID is constant!");
  },

  itemExists: function FormStore_itemExists(id) {
    return (id === WEAVE_PREFS_GUID);
  },

  createRecord: function createRecord(guid) {
    let record = new PrefRec();

    if (guid == WEAVE_PREFS_GUID) {
      record.value = this._getAllPrefs();
    } else {
      record.deleted = true;
    }

    return record;
  },

  create: function PrefStore_create(record) {
    this._log.trace("Ignoring create request");
  },

  remove: function PrefStore_remove(record) {
    this._log.trace("Ignoring remove request")
  },

  update: function PrefStore_update(record) {
    this._log.trace("Received pref updates, applying...");
    this._setAllPrefs(record.value);
  },

  wipe: function PrefStore_wipe() {
    this._log.trace("Ignoring wipe request");
  }
};

function PrefTracker(name) {
  Tracker.call(this, name);
  this._prefs.addObserver("", this, false);
}
PrefTracker.prototype = {
  __proto__: Tracker.prototype,

  get _prefs() {
    let prefs = Cc["@mozilla.org/preferences-service;1"].
      getService(Ci.nsIPrefBranch2);

    this.__defineGetter__("_prefs", function() prefs);
    return prefs;
  },

  get _syncPrefs() {
    let service = Cc["@mozilla.org/preferences-service;1"].
      getService(Ci.nsIPrefService);
    let syncPrefs = service.getBranch(WEAVE_SYNC_PREFS).getChildList("", {});

    this.__defineGetter__("_syncPrefs", function() syncPrefs);
    return syncPrefs;
  },

  /* 25 points per pref change */
  observe: function(aSubject, aTopic, aData) {
    if (aTopic != "nsPref:changed")
      return;

    if (this._syncPrefs.indexOf(aData) != -1) {
      this.score += 1;
      this.addChangedID(WEAVE_PREFS_GUID);
      this._log.trace("Preference " + aData + " changed");
    }
  }
};
