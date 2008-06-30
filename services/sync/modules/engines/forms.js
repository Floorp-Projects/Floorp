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

const EXPORTED_SYMBOLS = ['FormEngine'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/syncCores.js");
Cu.import("resource://weave/stores.js");
Cu.import("resource://weave/trackers.js");

Function.prototype.async = Async.sugar;

function FormEngine(pbeId) {
  this._init(pbeId);
}
FormEngine.prototype = {
  get name() { return "forms"; },
  get logName() { return "FormEngine"; },
  get serverPrefix() { return "user-data/forms/"; },

  __store: null,
  get _store() {
    if (!this.__store)
      this.__store = new FormStore();
    return this.__store;
  },

  __core: null,
  get _core() {
    if (!this.__core)
      this.__core = new FormSyncCore(this._store);
    return this.__core;
  },

  __tracker: null,
  get _tracker() {
    if (!this.__tracker)
      this.__tracker = new FormsTracker();
    return this.__tracker;
  }
};
FormEngine.prototype.__proto__ = new Engine();

function FormSyncCore(store) {
  this._store = store;
  this._init();
}
FormSyncCore.prototype = {
  _logName: "FormSync",
  _store: null,

  _commandLike: function FSC_commandLike(a, b) {
    /* Not required as GUIDs for similar data sets will be the same */
    return false;
  }
};
FormSyncCore.prototype.__proto__ = new SyncCore();

function FormStore() {
  this._init();
}
FormStore.prototype = {
  _logName: "FormStore",
  _lookup: null,

  __formDB: null,
  get _formDB() {
    if (!this.__formDB) {
      var file = Cc["@mozilla.org/file/directory_service;1"].
                 getService(Ci.nsIProperties).
                 get("ProfD", Ci.nsIFile);
      file.append("formhistory.sqlite");
      var stor = Cc["@mozilla.org/storage/service;1"].
                 getService(Ci.mozIStorageService);
      this.__formDB = stor.openDatabase(file);
    }
    return this.__formDB;
  },

  __formHistory: null,
  get _formHistory() {
    if (!this.__formHistory)
      this.__formHistory = Cc["@mozilla.org/satchel/form-history;1"].
                           getService(Ci.nsIFormHistory2);
    return this.__formHistory;
  },

  _createCommand: function FormStore__createCommand(command) {
    this._log.info("FormStore got createCommand: " + command);
    this._formHistory.addEntry(command.data.name, command.data.value);
  },

  _removeCommand: function FormStore__removeCommand(command) {
    this._log.info("FormStore got removeCommand: " + command);

    var data;
    if (command.GUID in this._lookup) {
      data = this._lookup[command.GUID];
    } else {
      this._log.warn("Invalid GUID found, ignoring remove request.");
      return;
    }

    var nam = data.name;
    var val = data.value;
    this._formHistory.removeEntry(nam, val);

    delete this._lookup[command.GUID];
  },

  _editCommand: function FormStore__editCommand(command) {
    this._log.info("FormStore got editCommand: " + command);
    this._log.warn("Form syncs are expected to only be create/remove!");
  },

  wrap: function FormStore_wrap() {
    this._lookup = {};
    var stmnt = this._formDB.createStatement("SELECT * FROM moz_formhistory");

    while (stmnt.executeStep()) {
      var nam = stmnt.getUTF8String(1);
      var val = stmnt.getUTF8String(2);
      var key = Utils.sha1(nam + val);

      this._lookup[key] = { name: nam, value: val };
    }

    return this._lookup;
  },

  wipe: function FormStore_wipe() {
    this._formHistory.removeAllEntries();
  },

  _resetGUIDs: function FormStore__resetGUIDs() {
    let self = yield;
    // Not needed.
  }
};
FormStore.prototype.__proto__ = new Store();

function FormsTracker() {
  this._init();
}
FormsTracker.prototype = {
  _logName: "FormsTracker",

  __formDB: null,
  get _formDB() {
    if (!this.__formDB) {
      var file = Cc["@mozilla.org/file/directory_service;1"].
      getService(Ci.nsIProperties).
      get("ProfD", Ci.nsIFile);
      file.append("formhistory.sqlite");
      var stor = Cc["@mozilla.org/storage/service;1"].
      getService(Ci.mozIStorageService);
      this.__formDB = stor.openDatabase(file);
    }

    return this.__formDB;
  },

  /* nsIFormSubmitObserver is not available in JS.
   * To calculate scores, we instead just count the changes in
   * the database since the last time we were asked.
   *
   * FIXME!: Buggy, because changes in a row doesn't result in
   * an increment of our score. A possible fix is to do a
   * SELECT for each fieldname and compare those instead of the
   * whole row count.
   *
   * Each change is worth 2 points. At some point, we may
   * want to differentiate between search-history rows and other
   * form items, and assign different scores.
   */
  _rowCount: 0,
  get score() {
    var stmnt = this._formDB.createStatement("SELECT COUNT(fieldname) FROM moz_formhistory");
    stmnt.executeStep();
    var count = stmnt.getInt32(0);
    stmnt.reset();

    this._score = Math.abs(this._rowCount - count) * 2;

    if (this._score >= 100)
      return 100;
    else
      return this._score;
  },

  resetScore: function FormsTracker_resetScore() {
    var stmnt = this._formDB.createStatement("SELECT COUNT(fieldname) FROM moz_formhistory");
    stmnt.executeStep();
    this._rowCount = stmnt.getInt32(0);
    stmnt.reset();
    this._score = 0;
  },

  _init: function FormsTracker__init() {
    this._log = Log4Moz.Service.getLogger("Service." + this._logName);
    this._score = 0;

    var stmnt = this._formDB.createStatement("SELECT COUNT(fieldname) FROM moz_formhistory");
    stmnt.executeStep();
    this._rowCount = stmnt.getInt32(0);
    stmnt.reset();
  }
};
FormsTracker.prototype.__proto__ = new Tracker();
