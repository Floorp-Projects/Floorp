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

const EXPORTED_SYMBOLS = ['InputEngine'];

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

function InputEngine(pbeId) {
  this._init(pbeId);
}
InputEngine.prototype = {
  __proto__: new SyncEngine(),

  get name() { return "input"; },
  get displayName() { return "Input History"; },
  get logName() { return "InputEngine"; },
  get serverPrefix() { return "user-data/input/"; },

  __store: null,
  get _store() {
    if (!this.__store)
      this.__store = new InputStore();
    return this.__store;
  },

  __core: null,
  get _core() {
    if (!this.__core)
      this.__core = new InputSyncCore(this._store);
    return this.__core;
  },

  __tracker: null,
  get _tracker() {
    if (!this.__tracker)
      this.__tracker = new InputTracker();
    return this.__tracker;
  }
};

function InputSyncCore(store) {
  this._store = store;
  this._init();
}
InputSyncCore.prototype = {
  _logName: "InputSync",
  _store: null,

  _commandLike: function FSC_commandLike(a, b) {
    /* Not required as GUIDs for similar data sets will be the same */
    return false;
  }
};
InputSyncCore.prototype.__proto__ = new SyncCore();

function InputStore() {
  this._init();
}
InputStore.prototype = {
  _logName: "InputStore",
  _lookup: null,

  __placeDB: null,
  get _placeDB() {
    if (!this.__placeDB) {
      let file = Cc["@mozilla.org/file/directory_service;1"].
                 getService(Ci.nsIProperties).
                 get("ProfD", Ci.nsIFile);
      file.append("places.sqlite");
      let stor = Cc["@mozilla.org/storage/service;1"].
                 getService(Ci.mozIStorageService);
      this.__histDB = stor.openDatabase(file);
    }
    return this.__histDB;
  },
  
  _getIDfromURI: function InputStore__getIDfromURI(uri) {
    let pidStmnt = this._placeDB.createStatement("SELECT id FROM moz_places WHERE url = ?1");
    pidStmnt.bindUTF8StringParameter(0, uri);
    if (pidStmnt.executeStep())
      return pidStmnt.getInt32(0);
    else
      return null;
  },
  
  _getInputHistory: function InputStore__getInputHistory(id) {
    let ipStmnt = this._placeDB.createStatement("SELECT input, use_count FROM moz_inputhistory WHERE place_id = ?1");
    ipStmnt.bindInt32Parameter(0, id);
    
    let input = [];
    while (ipStmnt.executeStep()) {
      let ip = ipStmnt.getUTF8String(0);
      let cnt = ipStmnt.getInt32(1);
      input[input.length] = {'input': ip, 'count': cnt};
    }
    
    return input;
  },

  _createCommand: function InputStore__createCommand(command) {
    this._log.info("InputStore got createCommand: " + command);
    
    let placeID = this._getIDfromURI(command.GUID);
    if (placeID) {
      let createStmnt = this._placeDB.createStatement("INSERT INTO moz_inputhistory (?1, ?2, ?3)");
      createStmnt.bindInt32Parameter(0, placeID);
      createStmnt.bindUTF8StringParameter(1, command.data.input);
      createStmnt.bindInt32Parameter(2, command.data.count);
      
      createStmnt.execute();
    }
  },

  _removeCommand: function InputStore__removeCommand(command) {
    this._log.info("InputStore got removeCommand: " + command);

    if (!(command.GUID in this._lookup)) {
      this._log.warn("Invalid GUID found, ignoring remove request.");
      return;
    }

    let placeID = this._getIDfromURI(command.GUID);
    let remStmnt = this._placeDB.createStatement("DELETE FROM moz_inputhistory WHERE place_id = ?1 AND input = ?2");
    
    remStmnt.bindInt32Parameter(0, placeID);
    remStmnt.bindUTF8StringParameter(1, command.data.input);
    remStmnt.execute();
    
    delete this._lookup[command.GUID];
  },

  _editCommand: function InputStore__editCommand(command) {
    this._log.info("InputStore got editCommand: " + command);
    
    if (!(command.GUID in this._lookup)) {
      this._log.warn("Invalid GUID found, ignoring remove request.");
      return;
    }

    let placeID = this._getIDfromURI(command.GUID);
    let editStmnt = this._placeDB.createStatement("UPDATE moz_inputhistory SET input = ?1, use_count = ?2 WHERE place_id = ?3");
    
    if ('input' in command.data) {
      editStmnt.bindUTF8StringParameter(0, command.data.input);
    } else {
      editStmnt.bindUTF8StringParameter(0, this._lookup[command.GUID].input);
    }
    
    if ('count' in command.data) {
      editStmnt.bindInt32Parameter(1, command.data.count);
    } else {
      editStmnt.bindInt32Parameter(1, this._lookup[command.GUID].count);
    }
    
    editStmnt.bindInt32Parameter(2, placeID);
    editStmnt.execute();
  },

  wrap: function InputStore_wrap() {
    this._lookup = {};
    let stmnt = this._placeDB.createStatement("SELECT * FROM moz_inputhistory");
    
    while (stmnt.executeStep()) {
      let pid = stmnt.getInt32(0);
      let inp = stmnt.getUTF8String(1);
      let cnt = stmnt.getInt32(2);

      let idStmnt = this._placeDB.createStatement("SELECT url FROM moz_places WHERE id = ?1");
      idStmnt.bindInt32Parameter(0, pid);
      if (idStmnt.executeStep()) {
        let key = idStmnt.getUTF8String(0);
        this._lookup[key] = { 'input': inp, 'count': cnt };
      }
    }

    return this._lookup;
  },

  wipe: function InputStore_wipe() {
    var stmnt = this._placeDB.createStatement("DELETE FROM moz_inputhistory");
    stmnt.execute();
  },

  _resetGUIDs: function InputStore__resetGUIDs() {
    let self = yield;
    // Not needed.
  }
};
InputStore.prototype.__proto__ = new Store();

function InputTracker() {
  this._init();
}
InputTracker.prototype = {
  _logName: "InputTracker",

  __placeDB: null,
  get _placeDB() {
    if (!this.__placeDB) {
      let file = Cc["@mozilla.org/file/directory_service;1"].
                 getService(Ci.nsIProperties).
                 get("ProfD", Ci.nsIFile);
      file.append("places.sqlite");
      let stor = Cc["@mozilla.org/storage/service;1"].
                 getService(Ci.mozIStorageService);
      this.__histDB = stor.openDatabase(file);
    }
    return this.__histDB;
  },

  /* 
   * To calculate scores, we just count the changes in
   * the database since the last time we were asked.
   *
   * Each change is worth 5 points.
   */
  _rowCount: 0,
  get score() {
    var stmnt = this._placeDB.createStatement("SELECT COUNT(place_id) FROM moz_inputhistory");
    stmnt.executeStep();
    var count = stmnt.getInt32(0);
    stmnt.reset();

    this._score = Math.abs(this._rowCount - count) * 5;

    if (this._score >= 100)
      return 100;
    else
      return this._score;
  },

  resetScore: function InputTracker_resetScore() {
    var stmnt = this._placeDB.createStatement("SELECT COUNT(place_id) FROM moz_inputhistory");
    stmnt.executeStep();
    this._rowCount = stmnt.getInt32(0);
    stmnt.reset();
    this._score = 0;
  },

  _init: function InputTracker__init() {
    this._log = Log4Moz.Service.getLogger("Service." + this._logName);
    this._score = 0;

    var stmnt = this._placeDB.createStatement("SELECT COUNT(place_id) FROM moz_inputhistory");
    stmnt.executeStep();
    this._rowCount = stmnt.getInt32(0);
    stmnt.reset();
  }
};
InputTracker.prototype.__proto__ = new Tracker();
