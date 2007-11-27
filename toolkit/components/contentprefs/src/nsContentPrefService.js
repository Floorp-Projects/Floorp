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
 * The Original Code is Content Preferences (cpref).
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Myk Melez <myk@mozilla.org>
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

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function ContentPrefService() {
  // If this throws an exception, it causes the getService call to fail,
  // but the next time a consumer tries to retrieve the service, we'll try
  // to initialize the database again, which might work if the failure
  // was due to a temporary condition (like being out of disk space).
  this._dbInit();

  // Observe shutdown so we can shut down the database connection.
  this._observerSvc.addObserver(this, "xpcom-shutdown", false);
}

ContentPrefService.prototype = {
  //**************************************************************************//
  // XPCOM Plumbing

  classDescription: "Content Pref Service",
  classID:          Components.ID("{e6a3f533-4ffa-4615-8eb4-d4e72d883fa7}"),
  contractID:       "@mozilla.org/content-pref/service;1",
  QueryInterface:   XPCOMUtils.generateQI([Ci.nsIContentPrefService]),


  //**************************************************************************//
  // Convenience Getters

  // Observer Service
  __observerSvc: null,
  get _observerSvc ContentPrefService_get__observerSvc() {
    if (!this.__observerSvc)
      this.__observerSvc = Cc["@mozilla.org/observer-service;1"].
                           getService(Ci.nsIObserverService);
    return this.__observerSvc;
  },

  // Console Service
  __consoleSvc: null,
  get _consoleSvc ContentPrefService_get__consoleSvc() {
    if (!this.__consoleSvc)
      this.__consoleSvc = Cc["@mozilla.org/consoleservice;1"].
                          getService(Ci.nsIConsoleService);
    return this.__consoleSvc;
  },


  //**************************************************************************//
  // Destruction

  _destroy: function ContentPrefService__destroy() {
    this._observerSvc.removeObserver(this, "xpcom-shutdown");

    // Delete references to XPCOM components to make sure we don't leak them
    // (although we haven't observed leakage in tests).  Also delete references
    // in _observers and _genericObservers to avoid cycles with those that
    // refer to us and don't remove themselves from those observer pools.
    for (var i in this) {
      try { this[i] = null }
      // Ignore "setting a property that has only a getter" exceptions.
      catch(ex) {}
    }
  },


  //**************************************************************************//
  // nsIObserver

  observe: function ContentPrefService_observe(subject, topic, data) {
    switch (topic) {
      case "xpcom-shutdown":
        this._destroy();
        break;
    }
  },


  //**************************************************************************//
  // nsIContentPrefService

  getPref: function ContentPrefService_getPref(aURI, aName) {
    if (aURI) {
      var group = this.grouper.group(aURI);
      return this._selectPref(group, aName);
    }

    return this._selectGlobalPref(aName);
  },

  setPref: function ContentPrefService_setPref(aURI, aName, aValue) {
    // If the pref is already set to the value, there's nothing more to do.
    var currentValue = this.getPref(aURI, aName);
    if (typeof currentValue != "undefined" && currentValue == aValue)
      return;

    var settingID = this._selectSettingID(aName) || this._insertSetting(aName);
    var group, groupID, prefID;
    if (aURI) {
      group = this.grouper.group(aURI);
      groupID = this._selectGroupID(group) || this._insertGroup(group);
      prefID = this._selectPrefID(groupID, settingID);
    }
    else {
      group = null;
      groupID = null;
      prefID = this._selectGlobalPrefID(settingID);
    }

    // Update the existing record, if any, or create a new one.
    if (prefID)
      this._updatePref(prefID, aValue);
    else
      this._insertPref(groupID, settingID, aValue);

    for each (var observer in this._getObservers(aName)) {
      try {
        observer.onContentPrefSet(group, aName, aValue);
      }
      catch(ex) {
        Cu.reportError(ex);
      }
    }
  },

  hasPref: function ContentPrefService_hasPref(aURI, aName) {
    // XXX If consumers end up calling this method regularly, then we should
    // optimize this to query the database directly.
    return (typeof this.getPref(aURI, aName) != "undefined");
  },

  removePref: function ContentPrefService_removePref(aURI, aName) {
    // If there's no old value, then there's nothing to remove.
    if (!this.hasPref(aURI, aName))
      return;

    var settingID = this._selectSettingID(aName);
    var group, groupID, prefID;
    if (aURI) {
      group = this.grouper.group(aURI);
      groupID = this._selectGroupID(group);
      prefID = this._selectPrefID(groupID, settingID);
    }
    else {
      group = null;
      groupID = null;
      prefID = this._selectGlobalPrefID(settingID);
    }

    this._deletePref(prefID);

    // Get rid of extraneous records that are no longer being used.
    this._deleteSettingIfUnused(settingID);
    if (groupID)
      this._deleteGroupIfUnused(groupID);

    for each (var observer in this._getObservers(aName)) {
      try {
        observer.onContentPrefRemoved(group, aName);
      }
      catch(ex) {
        Cu.reportError(ex);
      }
    }
  },

  getPrefs: function ContentPrefService_getPrefs(aURI) {
    if (aURI) {
      var group = this.grouper.group(aURI);
      return this._selectPrefs(group);
    }

    return this._selectGlobalPrefs();
  },

  // A hash of arrays of observers, indexed by setting name.
  _observers: {},

  // An array of generic observers, which observe all settings.
  _genericObservers: [],

  addObserver: function ContentPrefService_addObserver(aName, aObserver) {
    var observers;
    if (aName) {
      if (!this._observers[aName])
        this._observers[aName] = [];
      observers = this._observers[aName];
    }
    else
      observers = this._genericObservers;

    if (observers.indexOf(aObserver) == -1)
      observers.push(aObserver);
  },

  removeObserver: function ContentPrefService_removeObserver(aName, aObserver) {
    var observers;
    if (aName) {
      if (!this._observers[aName])
        return;
      observers = this._observers[aName];
    }
    else
      observers = this._genericObservers;

    if (observers.indexOf(aObserver) != -1)
      observers.splice(observers.indexOf(aObserver), 1);
  },

  /**
   * Construct a list of observers to notify about a change to some setting,
   * putting setting-specific observers before before generic ones, so observers
   * that initialize individual settings (like the page style controller)
   * execute before observers that display multiple settings and depend on them
   * being initialized first (like the content prefs sidebar).
   */
  _getObservers: function ContentPrefService__getObservers(aName) {
    var observers = [];

    if (aName && this._observers[aName])
      observers = observers.concat(this._observers[aName]);
    observers = observers.concat(this._genericObservers);

    return observers;
  },

  _grouper: null,
  get grouper ContentPrefService_get_grouper() {
    if (!this._grouper)
      this._grouper = Cc["@mozilla.org/content-pref/hostname-grouper;1"].
                      getService(Ci.nsIContentURIGrouper);
    return this._grouper;
  },


  //**************************************************************************//
  // Data Retrieval & Modification

  __stmtSelectPref: null,
  get _stmtSelectPref ContentPrefService_get__stmtSelectPref() {
    if (!this.__stmtSelectPref)
      this.__stmtSelectPref = this._dbCreateStatement(
        "SELECT prefs.value AS value " +
        "FROM prefs " +
        "JOIN groups ON prefs.groupID = groups.id " +
        "JOIN settings ON prefs.settingID = settings.id " +
        "WHERE groups.name = :group " +
        "AND settings.name = :setting"
      );

    return this.__stmtSelectPref;
  },

  _selectPref: function ContentPrefService__selectPref(aGroup, aSetting) {
    var value;

    try {
      this._stmtSelectPref.params.group = aGroup;
      this._stmtSelectPref.params.setting = aSetting;

      if (this._stmtSelectPref.step())
        value = this._stmtSelectPref.row["value"];
    }
    finally {
      this._stmtSelectPref.reset();
    }

    return value;
  },

  __stmtSelectGlobalPref: null,
  get _stmtSelectGlobalPref ContentPrefService_get__stmtSelectGlobalPref() {
    if (!this.__stmtSelectGlobalPref)
      this.__stmtSelectGlobalPref = this._dbCreateStatement(
        "SELECT prefs.value AS value " +
        "FROM prefs " +
        "JOIN settings ON prefs.settingID = settings.id " +
        "WHERE prefs.groupID IS NULL " +
        "AND settings.name = :name"
      );

    return this.__stmtSelectGlobalPref;
  },

  _selectGlobalPref: function ContentPrefService__selectGlobalPref(aName) {
    var value;

    try {
      this._stmtSelectGlobalPref.params.name = aName;

      if (this._stmtSelectGlobalPref.step())
        value = this._stmtSelectGlobalPref.row["value"];
    }
    finally {
      this._stmtSelectGlobalPref.reset();
    }

    return value;
  },

  __stmtSelectGroupID: null,
  get _stmtSelectGroupID ContentPrefService_get__stmtSelectGroupID() {
    if (!this.__stmtSelectGroupID)
      this.__stmtSelectGroupID = this._dbCreateStatement(
        "SELECT groups.id AS id " +
        "FROM groups " +
        "WHERE groups.name = :name "
      );

    return this.__stmtSelectGroupID;
  },

  _selectGroupID: function ContentPrefService__selectGroupID(aName) {
    var id;

    try {
      this._stmtSelectGroupID.params.name = aName;

      if (this._stmtSelectGroupID.step())
        id = this._stmtSelectGroupID.row["id"];
    }
    finally {
      this._stmtSelectGroupID.reset();
    }

    return id;
  },

  __stmtInsertGroup: null,
  get _stmtInsertGroup ContentPrefService_get__stmtInsertGroup() {
    if (!this.__stmtInsertGroup)
      this.__stmtInsertGroup = this._dbCreateStatement(
        "INSERT INTO groups (name) VALUES (:name)"
      );

    return this.__stmtInsertGroup;
  },

  _insertGroup: function ContentPrefService__insertGroup(aName) {
    this._stmtInsertGroup.params.name = aName;
    this._stmtInsertGroup.execute();
    return this._dbConnection.lastInsertRowID;
  },

  __stmtSelectSettingID: null,
  get _stmtSelectSettingID ContentPrefService_get__stmtSelectSettingID() {
    if (!this.__stmtSelectSettingID)
      this.__stmtSelectSettingID = this._dbCreateStatement(
        "SELECT id FROM settings WHERE name = :name"
      );

    return this.__stmtSelectSettingID;
  },

  _selectSettingID: function ContentPrefService__selectSettingID(aName) {
    var id;

    try {
      this._stmtSelectSettingID.params.name = aName;

      if (this._stmtSelectSettingID.step())
        id = this._stmtSelectSettingID.row["id"];
    }
    finally {
      this._stmtSelectSettingID.reset();
    }

    return id;
  },

  __stmtInsertSetting: null,
  get _stmtInsertSetting ContentPrefService_get__stmtInsertSetting() {
    if (!this.__stmtInsertSetting)
      this.__stmtInsertSetting = this._dbCreateStatement(
        "INSERT INTO settings (name) VALUES (:name)"
      );

    return this.__stmtInsertSetting;
  },

  _insertSetting: function ContentPrefService__insertSetting(aName) {
    this._stmtInsertSetting.params.name = aName;
    this._stmtInsertSetting.execute();
    return this._dbConnection.lastInsertRowID;
  },

  __stmtSelectPrefID: null,
  get _stmtSelectPrefID ContentPrefService_get__stmtSelectPrefID() {
    if (!this.__stmtSelectPrefID)
      this.__stmtSelectPrefID = this._dbCreateStatement(
        "SELECT id FROM prefs WHERE groupID = :groupID AND settingID = :settingID"
      );

    return this.__stmtSelectPrefID;
  },

  _selectPrefID: function ContentPrefService__selectPrefID(aGroupID, aSettingID) {
    var id;

    try {
      this._stmtSelectPrefID.params.groupID = aGroupID;
      this._stmtSelectPrefID.params.settingID = aSettingID;

      if (this._stmtSelectPrefID.step())
        id = this._stmtSelectPrefID.row["id"];
    }
    finally {
      this._stmtSelectPrefID.reset();
    }

    return id;
  },

  __stmtSelectGlobalPrefID: null,
  get _stmtSelectGlobalPrefID ContentPrefService_get__stmtSelectGlobalPrefID() {
    if (!this.__stmtSelectGlobalPrefID)
      this.__stmtSelectGlobalPrefID = this._dbCreateStatement(
        "SELECT id FROM prefs WHERE groupID IS NULL AND settingID = :settingID"
      );

    return this.__stmtSelectGlobalPrefID;
  },

  _selectGlobalPrefID: function ContentPrefService__selectGlobalPrefID(aSettingID) {
    var id;

    try {
      this._stmtSelectGlobalPrefID.params.settingID = aSettingID;

      if (this._stmtSelectGlobalPrefID.step())
        id = this._stmtSelectGlobalPrefID.row["id"];
    }
    finally {
      this._stmtSelectGlobalPrefID.reset();
    }

    return id;
  },

  __stmtInsertPref: null,
  get _stmtInsertPref ContentPrefService_get__stmtInsertPref() {
    if (!this.__stmtInsertPref)
      this.__stmtInsertPref = this._dbCreateStatement(
        "INSERT INTO prefs (groupID, settingID, value) " +
        "VALUES (:groupID, :settingID, :value)"
      );

    return this.__stmtInsertPref;
  },

  _insertPref: function ContentPrefService__insertPref(aGroupID, aSettingID, aValue) {
    this._stmtInsertPref.params.groupID = aGroupID;
    this._stmtInsertPref.params.settingID = aSettingID;
    this._stmtInsertPref.params.value = aValue;
    this._stmtInsertPref.execute();
    return this._dbConnection.lastInsertRowID;
  },

  __stmtUpdatePref: null,
  get _stmtUpdatePref ContentPrefService_get__stmtUpdatePref() {
    if (!this.__stmtUpdatePref)
      this.__stmtUpdatePref = this._dbCreateStatement(
        "UPDATE prefs SET value = :value WHERE id = :id"
      );

    return this.__stmtUpdatePref;
  },

  _updatePref: function ContentPrefService__updatePref(aPrefID, aValue) {
    this._stmtUpdatePref.params.id = aPrefID;
    this._stmtUpdatePref.params.value = aValue;
    this._stmtUpdatePref.execute();
  },

  __stmtDeletePref: null,
  get _stmtDeletePref ContentPrefService_get__stmtDeletePref() {
    if (!this.__stmtDeletePref)
      this.__stmtDeletePref = this._dbCreateStatement(
        "DELETE FROM prefs WHERE id = :id"
      );

    return this.__stmtDeletePref;
  },

  _deletePref: function ContentPrefService__deletePref(aPrefID) {
    this._stmtDeletePref.params.id = aPrefID;
    this._stmtDeletePref.execute();
  },

  __stmtDeleteSettingIfUnused: null,
  get _stmtDeleteSettingIfUnused ContentPrefService_get__stmtDeleteSettingIfUnused() {
    if (!this.__stmtDeleteSettingIfUnused)
      this.__stmtDeleteSettingIfUnused = this._dbCreateStatement(
        "DELETE FROM settings WHERE id = :id " +
        "AND id NOT IN (SELECT DISTINCT settingID FROM prefs)"
      );

    return this.__stmtDeleteSettingIfUnused;
  },

  _deleteSettingIfUnused: function ContentPrefService__deleteSettingIfUnused(aSettingID) {
    this._stmtDeleteSettingIfUnused.params.id = aSettingID;
    this._stmtDeleteSettingIfUnused.execute();
  },

  __stmtDeleteGroupIfUnused: null,
  get _stmtDeleteGroupIfUnused ContentPrefService_get__stmtDeleteGroupIfUnused() {
    if (!this.__stmtDeleteGroupIfUnused)
      this.__stmtDeleteGroupIfUnused = this._dbCreateStatement(
        "DELETE FROM groups WHERE id = :id " +
        "AND id NOT IN (SELECT DISTINCT groupID FROM prefs)"
      );

    return this.__stmtDeleteGroupIfUnused;
  },

  _deleteGroupIfUnused: function ContentPrefService__deleteGroupIfUnused(aGroupID) {
    this._stmtDeleteGroupIfUnused.params.id = aGroupID;
    this._stmtDeleteGroupIfUnused.execute();
  },

  __stmtSelectPrefs: null,
  get _stmtSelectPrefs ContentPrefService_get__stmtSelectPrefs() {
    if (!this.__stmtSelectPrefs)
      this.__stmtSelectPrefs = this._dbCreateStatement(
        "SELECT settings.name AS name, prefs.value AS value " +
        "FROM prefs " +
        "JOIN groups ON prefs.groupID = groups.id " +
        "JOIN settings ON prefs.settingID = settings.id " +
        "WHERE groups.name = :group "
      );

    return this.__stmtSelectPrefs;
  },

  _selectPrefs: function ContentPrefService__selectPrefs(aGroup) {
    var prefs = Cc["@mozilla.org/hash-property-bag;1"].
                createInstance(Ci.nsIWritablePropertyBag);

    try {
      this._stmtSelectPrefs.params.group = aGroup;

      while (this._stmtSelectPrefs.step())
        prefs.setProperty(this._stmtSelectPrefs.row["name"],
                          this._stmtSelectPrefs.row["value"]);
    }
    finally {
      this._stmtSelectPrefs.reset();
    }

    return prefs;
  },

  __stmtSelectGlobalPrefs: null,
  get _stmtSelectGlobalPrefs ContentPrefService_get__stmtSelectGlobalPrefs() {
    if (!this.__stmtSelectGlobalPrefs)
      this.__stmtSelectGlobalPrefs = this._dbCreateStatement(
        "SELECT settings.name AS name, prefs.value AS value " +
        "FROM prefs " +
        "JOIN settings ON prefs.settingID = settings.id " +
        "WHERE prefs.groupID IS NULL"
      );

    return this.__stmtSelectGlobalPrefs;
  },

  _selectGlobalPrefs: function ContentPrefService__selectGlobalPrefs() {
    var prefs = Cc["@mozilla.org/hash-property-bag;1"].
                createInstance(Ci.nsIWritablePropertyBag);

    try {
      while (this._stmtSelectGlobalPrefs.step())
        prefs.setProperty(this._stmtSelectGlobalPrefs.row["name"],
                          this._stmtSelectGlobalPrefs.row["value"]);
    }
    finally {
      this._stmtSelectGlobalPrefs.reset();
    }

    return prefs;
  },


  //**************************************************************************//
  // Database Creation & Access

  _dbVersion: 3,

  _dbSchema: {
    tables: {
      groups:     "id           INTEGER PRIMARY KEY, \
                   name         TEXT NOT NULL",
  
      settings:   "id           INTEGER PRIMARY KEY, \
                   name         TEXT NOT NULL",
  
      prefs:      "id           INTEGER PRIMARY KEY, \
                   groupID      INTEGER REFERENCES groups(id), \
                   settingID    INTEGER NOT NULL REFERENCES settings(id), \
                   value        BLOB"
    },
    indices: {
      groups_idx: {
        table: "groups",
        columns: ["name"]
      },
      settings_idx: {
        table: "settings",
        columns: ["name"]
      },
      prefs_idx: {
        table: "prefs",
        columns: ["groupID", "settingID"]
      }
    }
  },

  _dbConnection: null,

  _dbCreateStatement: function ContentPrefService__dbCreateStatement(aSQLString) {
    try {
      var statement = this._dbConnection.createStatement(aSQLString);
    }
    catch(ex) {
      Cu.reportError("error creating statement " + aSQLString + ": " +
                     this._dbConnection.lastError + " - " +
                     this._dbConnection.lastErrorString);
      throw ex;
    }

    var wrappedStatement = Cc["@mozilla.org/storage/statement-wrapper;1"].
                           createInstance(Ci.mozIStorageStatementWrapper);
    wrappedStatement.initialize(statement);
    return wrappedStatement;
  },

  // _dbInit and the methods it calls (_dbCreate, _dbMigrate, and version-
  // specific migration methods) must be careful not to call any method
  // of the service that assumes the database connection has already been
  // initialized, since it won't be initialized until at the end of _dbInit.

  _dbInit: function ContentPrefService__dbInit() {
    var dirService = Cc["@mozilla.org/file/directory_service;1"].
                     getService(Ci.nsIProperties);
    var dbFile = dirService.get("ProfD", Ci.nsIFile);
    dbFile.append("content-prefs.sqlite");

    var dbService = Cc["@mozilla.org/storage/service;1"].
                    getService(Ci.mozIStorageService);

    var dbConnection;

    if (!dbFile.exists())
      dbConnection = this._dbCreate(dbService, dbFile);
    else {
      try {
        dbConnection = dbService.openDatabase(dbFile);

        // Get the version of the database in the file.
        var version = dbConnection.schemaVersion;

        if (version != this._dbVersion)
          this._dbMigrate(dbConnection, version, this._dbVersion);
      }
      catch (ex) {
        // If the database file is corrupted, I'm not sure whether we should
        // just delete the corrupted file or back it up.  For now I'm just
        // deleting it, but here's some code that backs it up (but doesn't limit
        // the number of backups, which is probably necessary, thus I'm not
        // using this code):
        //var backup = this._dbFile.clone();
        //backup.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
        //backup.remove(false);
        //this._dbFile.moveTo(null, backup.leafName);
        if (ex.result == Cr.NS_ERROR_FILE_CORRUPTED) {
          // Remove the corrupted file, then recreate it.
          dbFile.remove(false);
          dbConnection = this._dbCreate(dbService, dbFile);
        }
        else
          throw ex;
      }
    }

    this._dbConnection = dbConnection;
  },

  _dbCreate: function ContentPrefService__dbCreate(aDBService, aDBFile) {
    var dbConnection = aDBService.openDatabase(aDBFile);
    for (let name in this._dbSchema.tables)
      dbConnection.createTable(name, this._dbSchema.tables[name]);
    this._dbCreateIndices(dbConnection);
    dbConnection.schemaVersion = this._dbVersion;
    return dbConnection;
  },

  _dbCreateIndices: function ContentPrefService__dbCreateIndices(aDBConnection) {
    for (let name in this._dbSchema.indices) {
      let index = this._dbSchema.indices[name];
      let statement = "CREATE INDEX " + name + " ON " + index.table +
                      "(" + index.columns.join(", ") + ")";
      aDBConnection.executeSimpleSQL(statement);
    }
  },

  _dbMigrate: function ContentPrefService__dbMigrate(aDBConnection, aOldVersion, aNewVersion) {
    if (this["_dbMigrate" + aOldVersion + "To" + aNewVersion]) {
      aDBConnection.beginTransaction();
      try {
        this["_dbMigrate" + aOldVersion + "To" + aNewVersion](aDBConnection);
        aDBConnection.commitTransaction();
      }
      catch(ex) {
        aDBConnection.rollbackTransaction();
        throw ex;
      }
    }
    else
      throw("can't migrate database from v" + aOldVersion +
            " to v" + aNewVersion + ": no migrator function");
  },

  _dbMigrate0To3: function ContentPrefService___dbMigrate0To3(aDBConnection) {
    aDBConnection.createTable("groups", this._dbSchema.tables.groups);
    aDBConnection.executeSimpleSQL(
      "INSERT INTO groups (id, name) SELECT id, name FROM sites"
    );

    aDBConnection.createTable("settings", this._dbSchema.tables.settings);
    aDBConnection.executeSimpleSQL(
      "INSERT INTO settings (id, name) SELECT id, name FROM keys"
    );

    aDBConnection.executeSimpleSQL("ALTER TABLE prefs RENAME TO prefsOld");
    aDBConnection.createTable("prefs", this._dbSchema.tables.prefs);
    aDBConnection.executeSimpleSQL(
      "INSERT INTO prefs (id, groupID, settingID, value) " +
      "SELECT id, site_id, key_id, value FROM prefsOld"
    );

    // Drop obsolete tables.
    aDBConnection.executeSimpleSQL("DROP TABLE prefsOld");
    aDBConnection.executeSimpleSQL("DROP TABLE keys");
    aDBConnection.executeSimpleSQL("DROP TABLE sites");

    this._dbCreateIndices(aDBConnection);

    aDBConnection.schemaVersion = this._dbVersion;
  },

  _dbMigrate1To3: function ContentPrefService___dbMigrate1To3(aDBConnection) {
    aDBConnection.executeSimpleSQL("ALTER TABLE groups RENAME TO groupsOld");
    aDBConnection.createTable("groups", this._dbSchema.tables.groups);
    aDBConnection.executeSimpleSQL(
      "INSERT INTO groups (id, name) " +
      "SELECT id, name FROM groupsOld"
    );

    aDBConnection.executeSimpleSQL("DROP TABLE groupers");
    aDBConnection.executeSimpleSQL("DROP TABLE groupsOld");

    this._dbCreateIndices(aDBConnection);

    aDBConnection.schemaVersion = this._dbVersion;
  },

  _dbMigrate2To3: function ContentPrefService__dbMigrate2To3(aDBConnection) {
    this._dbCreateIndices(aDBConnection);
  }

};


function HostnameGrouper() {}

HostnameGrouper.prototype = {
  //**************************************************************************//
  // XPCOM Plumbing
  
  classDescription: "Hostname Grouper",
  classID:          Components.ID("{8df290ae-dcaa-4c11-98a5-2429a4dc97bb}"),
  contractID:       "@mozilla.org/content-pref/hostname-grouper;1",
  QueryInterface:   XPCOMUtils.generateQI([Ci.nsIContentURIGrouper]),


  //**************************************************************************//
  // nsIContentURIGrouper

  group: function HostnameGrouper_group(aURI) {
    var group;

    try {
      // Accessing the host property of the URI will throw an exception
      // if the URI is of a type that doesn't have a host property.
      // Otherwise, we manually throw an exception if the host is empty,
      // since the effect is the same (we can't derive a group from it).

      group = aURI.host;
      if (!group)
        throw("can't derive group from host; no host in URI");
    }
    catch(ex) {
      // If we don't have a host, then use the entire URI (minus the query,
      // reference, and hash, if possible) as the group.  This means that URIs
      // like about:mozilla and about:blank will be considered separate groups,
      // but at least they'll be grouped somehow.
      
      // This also means that each individual file: URL will be considered
      // its own group.  This seems suboptimal, but so does treating the entire
      // file: URL space as a single group (especially if folks start setting
      // group-specific capabilities prefs).

      // XXX Is there something better we can do here?

      try {
        var url = aURI.QueryInterface(Ci.nsIURL);
        group = aURI.prePath + url.filePath;
      }
      catch(ex) {
        group = aURI.spec;
      }
    }

    return group;
  }
};


//****************************************************************************//
// XPCOM Plumbing

var components = [ContentPrefService, HostnameGrouper];
var NSGetModule = function ContentPrefService_NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule(components);
}
