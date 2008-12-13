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

function FormEngine() {
  this._init();
}
FormEngine.prototype = {
  __proto__: SyncEngine.prototype,
  get _super() SyncEngine.prototype,
  
  get name() "forms",
  get displayName() "Forms",
  get logName() "Forms",
  get serverPrefix() "user-data/forms/",

  get _store() {
    let store = new FormStore();
    this.__defineGetter__("_store", function() store);
    return store;
  },

  get _tracker() {
    let tracker = new FormsTracker();
    this.__defineGetter__("_tracker", function() tracker);
    return tracker;
  }
};


function FormStore() {
  this._init();
}
FormStore.prototype = {
  __proto__: Store.prototype,
  _logName: "FormStore",
  _lookup: null,

  get _formDB() {
    let file = Cc["@mozilla.org/file/directory_service;1"].
      getService(Ci.nsIProperties).
      get("ProfD", Ci.nsIFile);
    file.append("formhistory.sqlite");
    let stor = Cc["@mozilla.org/storage/service;1"].
      getService(Ci.mozIStorageService);
    let formDB = stor.openDatabase(file);
      
    this.__defineGetter__("_formDB", function() formDB);
    return formDB;
  },

  get _formHistory() {
    let formHistory = Cc["@mozilla.org/satchel/form-history;1"].
      getService(Ci.nsIFormHistory2);
    this.__defineGetter__("_formHistory", function() formHistory);
    return formHistory;
  },
  
  get _formStatement() {
    let stmnt = this._formDB.createStatement("SELECT * FROM moz_formhistory");
    this.__defineGetter__("_formStatement", function() stmnt);
    return stmnt;
  },

  create: function FormStore_create(record) {
    this._log.debug("Got create record: " + record.id);
    this._formHistory.addEntry(record.cleartext.name, record.cleartext.value);
  },

  remove: function FormStore_remove(record) {
    this._log.debug("Got remove record: " + record.id);
    
    if (record.id in this._lookup) {
      let data = this._lookup[record.id];
    } else {
      this._log.warn("Invalid GUID found, ignoring remove request.");
      return;
    }

    this._formHistory.removeEntry(data.name, data.value);
    delete this._lookup[record.id];
  },

  update: function FormStore__editCommand(record) {
    this._log.debug("Got update record: " + record.id);
    this._log.warn("Ignoring update request");
  },

  wrap: function FormStore_wrap() {
    this._lookup = {};
    let stmnt = this._formStatement;

    while (stmnt.executeStep()) {
      let nam = stmnt.getUTF8String(1);
      let val = stmnt.getUTF8String(2);
      let key = Utils.sha1(nam + val);

      this._lookup[key] = { name: nam, value: val };
    }
    stmnt.reset();
    
    return this._lookup;
  },

  wrapItem: function FormStore_wrapItem(id) {
    if (!this._lookup)
      this._lookup = this.wrap();
    return this._lookup[id];
  },

  getAllIDs: function FormStore_getAllIDs() {
    if (!this._lookup)
      this._lookup = this.wrap();
    return this._lookup;
  },
  
  wipe: function FormStore_wipe() {
    this._formHistory.removeAllEntries();
  }
};

function FormsTracker() {
  this._init();
}
FormsTracker.prototype = {
  __proto__: Tracker.prototype,
  _logName: "FormsTracker",

  get _formDB() {
    let file = Cc["@mozilla.org/file/directory_service;1"].
      getService(Ci.nsIProperties).
      get("ProfD", Ci.nsIFile);
      
    file.append("formhistory.sqlite");
    let stor = Cc["@mozilla.org/storage/service;1"].
      getService(Ci.mozIStorageService);
    
    let formDB = stor.openDatabase(file);
    this.__defineGetter__("_formDB", function() formDB);
    return formDB;
  },
  
  get _formStatement() {
    let stmnt = this._formDB.
      createStatement("SELECT COUNT(fieldname) FROM moz_formhistory");
    this.__defineGetter__("_formStatement", function() stmnt);
    return stmnt;
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
    let stmnt = this._formStatement;
    stmnt.executeStep();
    
    let count = stmnt.getInt32(0);
    this._score = Math.abs(this._rowCount - count) * 2;
    stmnt.reset();
    
    if (this._score >= 100)
      return 100;
    else
      return this._score;
  },

  resetScore: function FormsTracker_resetScore() {
    let stmnt = this._formStatement;
    stmnt.executeStep();    
    
    this._rowCount = stmnt.getInt32(0);
    this._score = 0;
    stmnt.reset();
  },

  _init: function FormsTracker__init() {
    this.__proto__.__proto__._init.call(this);

    let stmnt = this._formStatement;
    stmnt.executeStep();
    
    this._rowCount = stmnt.getInt32(0);
    stmnt.reset();
  }
};

