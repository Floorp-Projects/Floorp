const EXPORTED_SYMBOLS = ['FormEngine'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/syncCores.js");
Cu.import("resource://weave/stores.js");
Cu.import("resource://weave/trackers.js");

/*
 * Generate GUID from a name,value pair.
 * If the concatenated length is less than 40, we just Base64 the JSON.
 * Otherwise, we Base64 the JSON of the name and SHA1 of the value.
 * The first character of the key determines which method we used:
 * '0' for full Base64, '1' for SHA-1'ed val.
 */
function _generateFormGUID(nam, val) {
  var key;
  var con = nam + val;
  
  var jso = Cc["@mozilla.org/dom/json;1"].
            createInstance(Ci.nsIJSON);

  if (con.length <= 40) {
    key = '0' + btoa(jso.encode([nam, val]));
  } else {
    val = Utils.sha1(val);
    key = '1' + btoa(jso.encode([nam, val]));
  }
  
  return key;
}

/*
 * Unwrap a name,value pair from a GUID.
 * Return an array [sha1ed, name, value]
 * sha1ed is a boolean determining if the value is SHA-1'ed or not.
 */
function _unwrapFormGUID(guid) {
  var jso = Cc["@mozilla.org/dom/json;1"].
            createInstance(Ci.nsIJSON);
  
  var ret;
  var dec = atob(guid.slice(1));
  var obj = jso.decode(dec);
  
  switch (guid[0]) {
    case '0':
      ret = [false, obj[0], obj[1]];
      break;
    case '1':
      ret = [true, obj[0], obj[1]];
      break;
    default:
      this._log.warn("Unexpected GUID header: " + guid[0] + ", aborting!");
      return false;
  }
  
  return ret;
}

function FormEngine(pbeId) {
  this._init(pbeId);
}
FormEngine.prototype = {
  get name() { return "forms"; },
  get logName() { return "FormEngine"; },
  get serverPrefix() { return "user-data/forms/"; },

  __core: null,
  get _core() {
    if (!this.__core)
      this.__core = new FormSyncCore();
    return this.__core;
  },

  __store: null,
  get _store() {
    if (!this.__store)
      this.__store = new FormStore();
    return this.__store;
  },

  __tracker: null,
  get _tracker() {
    if (!this.__tracker)
      this.__tracker = new FormsTracker();
    return this.__tracker;
  }
};
FormEngine.prototype.__proto__ = new Engine();

function FormSyncCore() {
  this._init();
}
FormSyncCore.prototype = {
  _logName: "FormSync",

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

  _itemExists: function FSC__itemExists(GUID) {
    var found = false;
    var stmnt = this._formDB.createStatement("SELECT * FROM moz_formhistory");

    /* Same performance restrictions as PasswordSyncCore apply here:
       caching required */
    while (stmnt.executeStep()) {
      var nam = stmnt.getUTF8String(1);
      var val = stmnt.getUTF8String(2);
      var key = _generateFormGUID(nam, val);

      if (key == GUID)
        found = true;
    }

    return found;
  },

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

  _getValueFromSHA1: function FormStore__getValueFromSHA1(name, sha) {
    var query = "SELECT value FROM moz_formhistory WHERE fieldname = '" + name + "'";
    var stmnt = this._formDB.createStatement(query);
    var found = false;
    
    while (stmnt.executeStep()) {
      var val = stmnt.getUTF8String(0);
      if (Utils.sha1(val) == sha) {
        found = val;
        break;
      }
    }
    return found;
  },
  
  _createCommand: function FormStore__createCommand(command) {
    this._log.info("FormStore got createCommand: " + command);
    this._formHistory.addEntry(command.data.name, command.data.value);
  },

  _removeCommand: function FormStore__removeCommand(command) {
    this._log.info("FormStore got removeCommand: " + command);
    
    var data = _unwrapFormGUID(command.GUID);
    if (!data) {
      this._log.warn("Invalid GUID found, ignoring remove request.");
      return;
    }
    
    var nam = data[1];
    var val = data[2];
    if (data[0]) {
      val = this._getValueFromSHA1(nam, val);
    }
    
    if (val) {
      this._formHistory.removeEntry(nam, val);
    } else {
      this._log.warn("Form value not found from GUID, ignoring remove request.");
    }
  },

  _editCommand: function FormStore__editCommand(command) {
    this._log.info("FormStore got editCommand: " + command);
    this._log.warn("Form syncs are expected to only be create/remove!");
  },

  wrap: function FormStore_wrap() {
    var items = [];
    var stmnt = this._formDB.createStatement("SELECT * FROM moz_formhistory");

    while (stmnt.executeStep()) {
      var nam = stmnt.getUTF8String(1);
      var val = stmnt.getUTF8String(2);
      var key = _generateFormGUID(nam, val);

      items[key] = { name: nam, value: val };
    }

    return items;
  },

  wipe: function FormStore_wipe() {
    this._formHistory.removeAllEntries();
  },

  resetGUIDs: function FormStore_resetGUIDs() {
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
}
FormsTracker.prototype.__proto__ = new Tracker();
