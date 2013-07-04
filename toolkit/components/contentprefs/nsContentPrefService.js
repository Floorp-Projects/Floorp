/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

const CACHE_MAX_GROUP_ENTRIES = 100;


// We have a whitelist for getting/setting. This is because
// there are potential privacy issues with a compromised
// content process checking the user's content preferences
// and using that to discover all the websites visited, etc.
// Also there are both potential race conditions (if two processes
// set more than one value in succession, and the values
// only make sense together), as well as security issues, if
// a compromised content process can send arbitrary setPref
// messages. The whitelist contains only those settings that
// are not at risk for either.
// We currently whitelist saving/reading the last directory of file
// uploads, and the last current spellchecker dictionary which are so far
// the only need we have identified.
const REMOTE_WHITELIST = [
  "browser.upload.lastDir",
  "spellcheck.lang",
];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

/**
 * Remotes the service. All the remoting/electrolysis code is in here,
 * so the regular service code below remains uncluttered and maintainable.
 */
function electrolify(service) {
  // FIXME: For now, use the wrappedJSObject hack, until bug
  //        593407 which will clean that up.
  //        Note that we also use this in the xpcshell tests, separately.
  service.wrappedJSObject = service;

  var appInfo = Cc["@mozilla.org/xre/app-info;1"];
  if (!appInfo || appInfo.getService(Ci.nsIXULRuntime).processType ==
      Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
    // Parent process

    service.messageManager = Cc["@mozilla.org/parentprocessmessagemanager;1"].
                             getService(Ci.nsIMessageBroadcaster);

    // Setup listener for child messages. We don't need to call
    // addMessageListener as the wakeup service will do that for us.
    service.receiveMessage = function(aMessage) {
      var json = aMessage.json;

      if (REMOTE_WHITELIST.indexOf(json.name) == -1)
        return { succeeded: false };

      switch (aMessage.name) {
        case "ContentPref:getPref":
          return { succeeded: true,
                   value: service.getPref(json.group, json.name, json.value) };

        case "ContentPref:setPref":
          service.setPref(json.group, json.name, json.value);
          return { succeeded: true };
      }
    };
  } else {
    // Child process

    service._dbInit = function(){}; // No local DB

    service.messageManager = Cc["@mozilla.org/childprocessmessagemanager;1"].
                             getService(Ci.nsISyncMessageSender);

    // Child method remoting
    [
      ['getPref', ['group', 'name'], ['_parseGroupParam']],
      ['setPref', ['group', 'name', 'value'], ['_parseGroupParam']],
    ].forEach(function(data) {
      var method = data[0];
      var params = data[1];
      var parsers = data[2];
      service[method] = function __remoted__() {
        var json = {};
        for (var i = 0; i < params.length; i++) {
          if (params[i]) {
            json[params[i]] = arguments[i];
            if (parsers[i])
              json[params[i]] = this[parsers[i]](json[params[i]]);
          }
        }
        var ret = service.messageManager.sendSyncMessage('ContentPref:' + method, json)[0];
        if (!ret.succeeded)
          throw "ContentPrefs remoting failed to pass whitelist";
        return ret.value;
      };
    });

    // Listen to preference change notifications from the parent and notify
    // observers in the child process according to the change
    service.messageManager.addMessageListener("ContentPref:notifyPrefSet",
      function(aMessage) {
        var json = aMessage.json;
        service._notifyPrefSet(json.group, json.name, json.value);
      });
    service.messageManager.addMessageListener("ContentPref:notifyPrefRemoved",
      function(aMessage) {
        var json = aMessage.json;
        service._notifyPrefRemoved(json.group, json.name);
      });
  }
}

function ContentPrefService() {
  electrolify(this);

  // If this throws an exception, it causes the getService call to fail,
  // but the next time a consumer tries to retrieve the service, we'll try
  // to initialize the database again, which might work if the failure
  // was due to a temporary condition (like being out of disk space).
  this._dbInit();

  this._observerSvc.addObserver(this, "last-pb-context-exited", false);

  // Observe shutdown so we can shut down the database connection.
  this._observerSvc.addObserver(this, "xpcom-shutdown", false);
}

Cu.import("resource://gre/modules/ContentPrefStore.jsm");
const cache = new ContentPrefStore();
cache.set = function CPS_cache_set(group, name, val) {
  Object.getPrototypeOf(this).set.apply(this, arguments);
  let groupCount = Object.keys(this._groups).length;
  if (groupCount >= CACHE_MAX_GROUP_ENTRIES) {
    // Clean half of the entries
    for (let [group, name, ] in this) {
      this.remove(group, name);
      groupCount--;
      if (groupCount < CACHE_MAX_GROUP_ENTRIES / 2)
        break;
    }
  }
};

const privModeStorage = new ContentPrefStore();

ContentPrefService.prototype = {
  //**************************************************************************//
  // XPCOM Plumbing

  classID: Components.ID("{e3f772f3-023f-4b32-b074-36cf0fd5d414}"),

  QueryInterface: function CPS_QueryInterface(iid) {
    let supportedIIDs = [
      Ci.nsIContentPrefService,
      Ci.nsIFrameMessageListener,
      Ci.nsISupports,
    ];
    if (supportedIIDs.some(function (i) iid.equals(i)))
      return this;
    if (iid.equals(Ci.nsIContentPrefService2)) {
      if (!this._contentPrefService2) {
        let s = {};
        Cu.import("resource://gre/modules/ContentPrefService2.jsm", s);
        this._contentPrefService2 = new s.ContentPrefService2(this);
      }
      return this._contentPrefService2;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  //**************************************************************************//
  // Convenience Getters

  // Observer Service
  __observerSvc: null,
  get _observerSvc() {
    if (!this.__observerSvc)
      this.__observerSvc = Cc["@mozilla.org/observer-service;1"].
                           getService(Ci.nsIObserverService);
    return this.__observerSvc;
  },

  // Console Service
  __consoleSvc: null,
  get _consoleSvc() {
    if (!this.__consoleSvc)
      this.__consoleSvc = Cc["@mozilla.org/consoleservice;1"].
                          getService(Ci.nsIConsoleService);
    return this.__consoleSvc;
  },

  // Preferences Service
  __prefSvc: null,
  get _prefSvc() {
    if (!this.__prefSvc)
      this.__prefSvc = Cc["@mozilla.org/preferences-service;1"].
                       getService(Ci.nsIPrefBranch);
    return this.__prefSvc;
  },


  //**************************************************************************//
  // Destruction

  _destroy: function ContentPrefService__destroy() {
    this._observerSvc.removeObserver(this, "xpcom-shutdown");
    this._observerSvc.removeObserver(this, "last-pb-context-exited");

    // Finalize statements which may have been used asynchronously.
    // FIXME(696499): put them in an object cache like other components.
    if (this.__stmtSelectPrefID) {
      this.__stmtSelectPrefID.finalize();
      this.__stmtSelectPrefID = null;
    }
    if (this.__stmtSelectGlobalPrefID) {
      this.__stmtSelectGlobalPrefID.finalize();
      this.__stmtSelectGlobalPrefID = null;
    }
    if (this.__stmtInsertPref) {
      this.__stmtInsertPref.finalize();
      this.__stmtInsertPref = null;
    }
    if (this.__stmtInsertGroup) {
      this.__stmtInsertGroup.finalize();
      this.__stmtInsertGroup = null;
    }
    if (this.__stmtInsertSetting) {
      this.__stmtInsertSetting.finalize();
      this.__stmtInsertSetting = null;
    }
    if (this.__stmtSelectGroupID) {
      this.__stmtSelectGroupID.finalize();
      this.__stmtSelectGroupID = null;
    }
    if (this.__stmtSelectSettingID) {
      this.__stmtSelectSettingID.finalize();
      this.__stmtSelectSettingID = null;
    }
    if (this.__stmtSelectPref) {
      this.__stmtSelectPref.finalize();
      this.__stmtSelectPref = null;
    }
    if (this.__stmtSelectGlobalPref) {
      this.__stmtSelectGlobalPref.finalize();
      this.__stmtSelectGlobalPref = null;
    }
    if (this.__stmtSelectPrefsByName) {
      this.__stmtSelectPrefsByName.finalize();
      this.__stmtSelectPrefsByName = null;
    }
    if (this.__stmtDeleteSettingIfUnused) {
      this.__stmtDeleteSettingIfUnused.finalize();
      this.__stmtDeleteSettingIfUnused = null;
    }
    if(this.__stmtSelectPrefs) {
      this.__stmtSelectPrefs.finalize();
      this.__stmtSelectPrefs = null;
    }
    if(this.__stmtDeleteGroupIfUnused) {
      this.__stmtDeleteGroupIfUnused.finalize();
      this.__stmtDeleteGroupIfUnused = null;
    }
    if (this.__stmtDeletePref) {
      this.__stmtDeletePref.finalize();
      this.__stmtDeletePref = null;
    }
    if (this.__stmtUpdatePref) {
      this.__stmtUpdatePref.finalize();
      this.__stmtUpdatePref = null;
    }

    if (this._contentPrefService2)
      this._contentPrefService2.destroy();

    this._dbConnection.asyncClose();

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
      case "last-pb-context-exited":
        this._privModeStorage.removeAll();
        break;
    }
  },


  //**************************************************************************//
  // in-memory cache and private-browsing stores

  _cache: cache,
  _privModeStorage: privModeStorage,

  //**************************************************************************//
  // nsIContentPrefService

  getPref: function ContentPrefService_getPref(aGroup, aName, aContext, aCallback) {
    if (!aName)
      throw Components.Exception("aName cannot be null or an empty string",
                                 Cr.NS_ERROR_ILLEGAL_VALUE);

    var group = this._parseGroupParam(aGroup);

    if (aContext && aContext.usePrivateBrowsing) {
      if (this._privModeStorage.has(group, aName)) {
        let value = this._privModeStorage.get(group, aName);
        if (aCallback) {
          this._scheduleCallback(function(){aCallback.onResult(value);});
          return;
        }
        return value;
      }
      // if we don't have a pref specific to this private mode browsing
      // session, to try to get one from normal mode
    }

    if (group == null)
      return this._selectGlobalPref(aName, aCallback);
    return this._selectPref(group, aName, aCallback);
  },

  setPref: function ContentPrefService_setPref(aGroup, aName, aValue, aContext) {
    // If the pref is already set to the value, there's nothing more to do.
    var currentValue = this.getPref(aGroup, aName, aContext);
    if (typeof currentValue != "undefined") {
      if (currentValue == aValue)
        return;
    }

    var group = this._parseGroupParam(aGroup);

    if (aContext && aContext.usePrivateBrowsing) {
      this._privModeStorage.setWithCast(group, aName, aValue);
      this._broadcastPrefSet(group, aName, aValue);
      return;
    }

    var settingID = this._selectSettingID(aName) || this._insertSetting(aName);
    var groupID, prefID;
    if (group == null) {
      groupID = null;
      prefID = this._selectGlobalPrefID(settingID);
    }
    else {
      groupID = this._selectGroupID(group) || this._insertGroup(group);
      prefID = this._selectPrefID(groupID, settingID);
    }

    // Update the existing record, if any, or create a new one.
    if (prefID)
      this._updatePref(prefID, aValue);
    else
      this._insertPref(groupID, settingID, aValue);

    this._cache.setWithCast(group, aName, aValue);
    this._broadcastPrefSet(group, aName, aValue);
  },

  hasPref: function ContentPrefService_hasPref(aGroup, aName, aContext) {
    // XXX If consumers end up calling this method regularly, then we should
    // optimize this to query the database directly.
    return (typeof this.getPref(aGroup, aName, aContext) != "undefined");
  },

  hasCachedPref: function ContentPrefService_hasCachedPref(aGroup, aName, aContext) {
    if (!aName)
      throw Components.Exception("aName cannot be null or an empty string",
                                 Cr.NS_ERROR_ILLEGAL_VALUE);

    let group = this._parseGroupParam(aGroup);
    let storage = aContext && aContext.usePrivateBrowsing ? this._privModeStorage: this._cache;
    return storage.has(group, aName);
  },

  removePref: function ContentPrefService_removePref(aGroup, aName, aContext) {
    // If there's no old value, then there's nothing to remove.
    if (!this.hasPref(aGroup, aName, aContext))
      return;

    var group = this._parseGroupParam(aGroup);

    if (aContext && aContext.usePrivateBrowsing) {
      this._privModeStorage.remove(group, aName);
      this._broadcastPrefRemoved(group, aName);
      return;
    }

    var settingID = this._selectSettingID(aName);
    var groupID, prefID;
    if (group == null) {
      groupID = null;
      prefID = this._selectGlobalPrefID(settingID);
    }
    else {
      groupID = this._selectGroupID(group);
      prefID = this._selectPrefID(groupID, settingID);
    }

    this._deletePref(prefID);

    // Get rid of extraneous records that are no longer being used.
    this._deleteSettingIfUnused(settingID);
    if (groupID)
      this._deleteGroupIfUnused(groupID);

    this._cache.remove(group, aName);
    this._broadcastPrefRemoved(group, aName);
  },

  removeGroupedPrefs: function ContentPrefService_removeGroupedPrefs(aContext) {
    // will not delete global preferences
    if (aContext && aContext.usePrivateBrowsing) {
        // keep only global prefs
        this._privModeStorage.removeAllGroups();
    }
    this._cache.removeAllGroups();
    this._dbConnection.beginTransaction();
    try {
      this._dbConnection.executeSimpleSQL("DELETE FROM prefs WHERE groupID IS NOT NULL");
      this._dbConnection.executeSimpleSQL("DELETE FROM groups");
      this._dbConnection.executeSimpleSQL(
        "DELETE FROM settings " +
        "WHERE id NOT IN (SELECT DISTINCT settingID FROM prefs)"
      );
      this._dbConnection.commitTransaction();
    }
    catch(ex) {
      this._dbConnection.rollbackTransaction();
      throw ex;
    }
  },

  removePrefsByName: function ContentPrefService_removePrefsByName(aName, aContext) {
    if (!aName)
      throw Components.Exception("aName cannot be null or an empty string",
                                 Cr.NS_ERROR_ILLEGAL_VALUE);

    if (aContext && aContext.usePrivateBrowsing) {
      for (let [group, name, ] in this._privModeStorage) {
        if (name === aName) {
          this._privModeStorage.remove(group, aName);
          this._broadcastPrefRemoved(group, aName);
        }
      }
    }

    var settingID = this._selectSettingID(aName);
    if (!settingID)
      return;
    
    var selectGroupsStmt = this._dbCreateStatement(
      "SELECT groups.id AS groupID, groups.name AS groupName " +
      "FROM prefs " +
      "JOIN groups ON prefs.groupID = groups.id " +
      "WHERE prefs.settingID = :setting "
    );
    
    var groupNames = [];
    var groupIDs = [];
    try {
      selectGroupsStmt.params.setting = settingID;
    
      while (selectGroupsStmt.executeStep()) {
        groupIDs.push(selectGroupsStmt.row["groupID"]);
        groupNames.push(selectGroupsStmt.row["groupName"]);
      }
    }
    finally {
      selectGroupsStmt.reset();
    }
    
    if (this.hasPref(null, aName)) {
      groupNames.push(null);
    }

    this._dbConnection.executeSimpleSQL("DELETE FROM prefs WHERE settingID = " + settingID);
    this._dbConnection.executeSimpleSQL("DELETE FROM settings WHERE id = " + settingID);

    for (var i = 0; i < groupNames.length; i++) {
      this._cache.remove(groupNames[i], aName);
      if (groupNames[i]) // ie. not null, which will be last (and i == groupIDs.length)
        this._deleteGroupIfUnused(groupIDs[i]);
      if (!aContext || !aContext.usePrivateBrowsing) {
        this._broadcastPrefRemoved(groupNames[i], aName);
      }
    }
  },

  getPrefs: function ContentPrefService_getPrefs(aGroup, aContext) {
    var group = this._parseGroupParam(aGroup);
    if (aContext && aContext.usePrivateBrowsing) {
        let prefs = Cc["@mozilla.org/hash-property-bag;1"].
                    createInstance(Ci.nsIWritablePropertyBag);
        for (let [sgroup, sname, sval] in this._privModeStorage) {
          if (sgroup === group)
            prefs.setProperty(sname, sval);
        }
        return prefs;
    }

    if (group == null)
      return this._selectGlobalPrefs();
    return this._selectPrefs(group);
  },

  getPrefsByName: function ContentPrefService_getPrefsByName(aName, aContext) {
    if (!aName)
      throw Components.Exception("aName cannot be null or an empty string",
                                 Cr.NS_ERROR_ILLEGAL_VALUE);

    if (aContext && aContext.usePrivateBrowsing) {
      let prefs = Cc["@mozilla.org/hash-property-bag;1"].
                  createInstance(Ci.nsIWritablePropertyBag);
      for (let [sgroup, sname, sval] in this._privModeStorage) {
        if (sname === aName)
          prefs.setProperty(sgroup, sval);
      }
      return prefs;
    }

    return this._selectPrefsByName(aName);
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

  /**
   * Notify all observers about the removal of a preference.
   */
  _notifyPrefRemoved: function ContentPrefService__notifyPrefRemoved(aGroup, aName) {
    for each (var observer in this._getObservers(aName)) {
      try {
        observer.onContentPrefRemoved(aGroup, aName);
      }
      catch(ex) {
        Cu.reportError(ex);
      }
    }
  },

  /**
   * Notify all observers about a preference change.
   */
  _notifyPrefSet: function ContentPrefService__notifyPrefSet(aGroup, aName, aValue) {
    for each (var observer in this._getObservers(aName)) {
      try {
        observer.onContentPrefSet(aGroup, aName, aValue);
      }
      catch(ex) {
        Cu.reportError(ex);
      }
    }
  },

  /**
   * Notify all observers in the current process about the removal of a
   * preference and send a message to all other processes so that they can in
   * turn notify their observers about the change. This is meant to be called
   * only in the parent process. Only whitelisted preferences are broadcast to
   * the child processes.
   */
  _broadcastPrefRemoved: function ContentPrefService__broadcastPrefRemoved(aGroup, aName) {
    this._notifyPrefRemoved(aGroup, aName);

    if (REMOTE_WHITELIST.indexOf(aName) != -1) {
      this.messageManager.broadcastAsyncMessage('ContentPref:notifyPrefRemoved',
        { "group": aGroup, "name": aName } );
    }
  },

  /**
   * Notify all observers in the current process about a preference change and
   * send a message to all other processes so that they can in turn notify
   * their observers about the change. This is meant to be called only in the
   * parent process. Only whitelisted preferences are broadcast to the child
   * processes.
   */
  _broadcastPrefSet: function ContentPrefService__broadcastPrefSet(aGroup, aName, aValue) {
    this._notifyPrefSet(aGroup, aName, aValue);

    if (REMOTE_WHITELIST.indexOf(aName) != -1) {
      this.messageManager.broadcastAsyncMessage('ContentPref:notifyPrefSet',
        { "group": aGroup, "name": aName, "value": aValue } );
    }
  },

  _grouper: null,
  get grouper() {
    if (!this._grouper)
      this._grouper = Cc["@mozilla.org/content-pref/hostname-grouper;1"].
                      getService(Ci.nsIContentURIGrouper);
    return this._grouper;
  },

  get DBConnection() {
    return this._dbConnection;
  },


  //**************************************************************************//
  // Data Retrieval & Modification

  __stmtSelectPref: null,
  get _stmtSelectPref() {
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

  _scheduleCallback: function(func) {
    let tm = Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager);
    tm.mainThread.dispatch(func, Ci.nsIThread.DISPATCH_NORMAL);
  },

  _selectPref: function ContentPrefService__selectPref(aGroup, aSetting, aCallback) {
    let value = undefined;
    if (this._cache.has(aGroup, aSetting)) {
      value = this._cache.get(aGroup, aSetting);
      if (aCallback) {
        this._scheduleCallback(function(){aCallback.onResult(value);});
        return;
      }
      return value;
    }

    try {
      this._stmtSelectPref.params.group = aGroup;
      this._stmtSelectPref.params.setting = aSetting;

      if (aCallback) {
        let cache = this._cache;
        new AsyncStatement(this._stmtSelectPref).execute({onResult: function(aResult) {
          cache.set(aGroup, aSetting, aResult);
          aCallback.onResult(aResult);
        }});
      }
      else {
        if (this._stmtSelectPref.executeStep()) {
          value = this._stmtSelectPref.row["value"];
        }
        this._cache.set(aGroup, aSetting, value);
      }
    }
    finally {
      this._stmtSelectPref.reset();
    }

    return value;
  },

  __stmtSelectGlobalPref: null,
  get _stmtSelectGlobalPref() {
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

  _selectGlobalPref: function ContentPrefService__selectGlobalPref(aName, aCallback) {
    let value = undefined;
    if (this._cache.has(null, aName)) {
      value = this._cache.get(null, aName);
      if (aCallback) {
        this._scheduleCallback(function(){aCallback.onResult(value);});
        return;
      }
      return value;
    }

    try {
      this._stmtSelectGlobalPref.params.name = aName;

      if (aCallback) {
        let cache = this._cache;
        new AsyncStatement(this._stmtSelectGlobalPref).execute({onResult: function(aResult) {
          cache.set(null, aName, aResult);
          aCallback.onResult(aResult);
        }});
      }
      else {
        if (this._stmtSelectGlobalPref.executeStep()) {
          value = this._stmtSelectGlobalPref.row["value"];
        }
        this._cache.set(null, aName, value);
      }
    }
    finally {
      this._stmtSelectGlobalPref.reset();
    }

    return value;
  },

  __stmtSelectGroupID: null,
  get _stmtSelectGroupID() {
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

      if (this._stmtSelectGroupID.executeStep())
        id = this._stmtSelectGroupID.row["id"];
    }
    finally {
      this._stmtSelectGroupID.reset();
    }

    return id;
  },

  __stmtInsertGroup: null,
  get _stmtInsertGroup() {
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
  get _stmtSelectSettingID() {
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

      if (this._stmtSelectSettingID.executeStep())
        id = this._stmtSelectSettingID.row["id"];
    }
    finally {
      this._stmtSelectSettingID.reset();
    }

    return id;
  },

  __stmtInsertSetting: null,
  get _stmtInsertSetting() {
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
  get _stmtSelectPrefID() {
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

      if (this._stmtSelectPrefID.executeStep())
        id = this._stmtSelectPrefID.row["id"];
    }
    finally {
      this._stmtSelectPrefID.reset();
    }

    return id;
  },

  __stmtSelectGlobalPrefID: null,
  get _stmtSelectGlobalPrefID() {
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

      if (this._stmtSelectGlobalPrefID.executeStep())
        id = this._stmtSelectGlobalPrefID.row["id"];
    }
    finally {
      this._stmtSelectGlobalPrefID.reset();
    }

    return id;
  },

  __stmtInsertPref: null,
  get _stmtInsertPref() {
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
  get _stmtUpdatePref() {
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
  get _stmtDeletePref() {
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
  get _stmtDeleteSettingIfUnused() {
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
  get _stmtDeleteGroupIfUnused() {
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
  get _stmtSelectPrefs() {
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

      while (this._stmtSelectPrefs.executeStep())
        prefs.setProperty(this._stmtSelectPrefs.row["name"],
                          this._stmtSelectPrefs.row["value"]);
    }
    finally {
      this._stmtSelectPrefs.reset();
    }

    return prefs;
  },

  __stmtSelectGlobalPrefs: null,
  get _stmtSelectGlobalPrefs() {
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
      while (this._stmtSelectGlobalPrefs.executeStep())
        prefs.setProperty(this._stmtSelectGlobalPrefs.row["name"],
                          this._stmtSelectGlobalPrefs.row["value"]);
    }
    finally {
      this._stmtSelectGlobalPrefs.reset();
    }

    return prefs;
  },

  __stmtSelectPrefsByName: null,
  get _stmtSelectPrefsByName() {
    if (!this.__stmtSelectPrefsByName)
      this.__stmtSelectPrefsByName = this._dbCreateStatement(
        "SELECT groups.name AS groupName, prefs.value AS value " +
        "FROM prefs " +
        "JOIN groups ON prefs.groupID = groups.id " +
        "JOIN settings ON prefs.settingID = settings.id " +
        "WHERE settings.name = :setting "
      );

    return this.__stmtSelectPrefsByName;
  },

  _selectPrefsByName: function ContentPrefService__selectPrefsByName(aName) {
    var prefs = Cc["@mozilla.org/hash-property-bag;1"].
                createInstance(Ci.nsIWritablePropertyBag);

    try {
      this._stmtSelectPrefsByName.params.setting = aName;

      while (this._stmtSelectPrefsByName.executeStep())
        prefs.setProperty(this._stmtSelectPrefsByName.row["groupName"],
                          this._stmtSelectPrefsByName.row["value"]);
    }
    finally {
      this._stmtSelectPrefsByName.reset();
    }
    
    var global = this._selectGlobalPref(aName);
    if (typeof global != "undefined") {
      prefs.setProperty(null, global);
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

    return statement;
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
      }
      // If the connection isn't ready after we open the database, that means
      // the database has been corrupted, so we back it up and then recreate it.
      catch (e if e.result == Cr.NS_ERROR_FILE_CORRUPTED) {
        dbConnection = this._dbBackUpAndRecreate(dbService, dbFile,
                                                 dbConnection);
      }

      // Get the version of the schema in the file.
      var version = dbConnection.schemaVersion;

      // Try to migrate the schema in the database to the current schema used by
      // the service.  If migration fails, back up the database and recreate it.
      if (version != this._dbVersion) {
        try {
          this._dbMigrate(dbConnection, version, this._dbVersion);
        }
        catch(ex) {
          Cu.reportError("error migrating DB: " + ex + "; backing up and recreating");
          dbConnection = this._dbBackUpAndRecreate(dbService, dbFile, dbConnection);
        }
      }
    }

    // Turn off disk synchronization checking to reduce disk churn and speed up
    // operations when prefs are changed rapidly (such as when a user repeatedly
    // changes the value of the browser zoom setting for a site).
    //
    // Note: this could cause database corruption if the OS crashes or machine
    // loses power before the data gets written to disk, but this is considered
    // a reasonable risk for the not-so-critical data stored in this database.
    //
    // If you really don't want to take this risk, however, just set the
    // toolkit.storage.synchronous pref to 1 (NORMAL synchronization) or 2
    // (FULL synchronization), in which case mozStorageConnection::Initialize
    // will use that value, and we won't override it here.
    if (!this._prefSvc.prefHasUserValue("toolkit.storage.synchronous"))
      dbConnection.executeSimpleSQL("PRAGMA synchronous = OFF");

    this._dbConnection = dbConnection;
  },

  _dbCreate: function ContentPrefService__dbCreate(aDBService, aDBFile) {
    var dbConnection = aDBService.openDatabase(aDBFile);

    try {
      this._dbCreateSchema(dbConnection);
      dbConnection.schemaVersion = this._dbVersion;
    }
    catch(ex) {
      // If we failed to create the database (perhaps because the disk ran out
      // of space), then remove the database file so we don't leave it in some
      // half-created state from which we won't know how to recover.
      dbConnection.close();
      aDBFile.remove(false);
      throw ex;
    }

    return dbConnection;
  },

  _dbCreateSchema: function ContentPrefService__dbCreateSchema(aDBConnection) {
    this._dbCreateTables(aDBConnection);
    this._dbCreateIndices(aDBConnection);
  },

  _dbCreateTables: function ContentPrefService__dbCreateTables(aDBConnection) {
    for (let name in this._dbSchema.tables)
      aDBConnection.createTable(name, this._dbSchema.tables[name]);
  },

  _dbCreateIndices: function ContentPrefService__dbCreateIndices(aDBConnection) {
    for (let name in this._dbSchema.indices) {
      let index = this._dbSchema.indices[name];
      let statement = "CREATE INDEX IF NOT EXISTS " + name + " ON " + index.table +
                      "(" + index.columns.join(", ") + ")";
      aDBConnection.executeSimpleSQL(statement);
    }
  },

  _dbBackUpAndRecreate: function ContentPrefService__dbBackUpAndRecreate(aDBService,
                                                                         aDBFile,
                                                                         aDBConnection) {
    aDBService.backupDatabaseFile(aDBFile, "content-prefs.sqlite.corrupt");

    // Close the database, ignoring the "already closed" exception, if any.
    // It'll be open if we're here because of a migration failure but closed
    // if we're here because of database corruption.
    try { aDBConnection.close() } catch(ex) {}

    aDBFile.remove(false);

    let dbConnection = this._dbCreate(aDBService, aDBFile);

    return dbConnection;
  },

  _dbMigrate: function ContentPrefService__dbMigrate(aDBConnection, aOldVersion, aNewVersion) {
    if (this["_dbMigrate" + aOldVersion + "To" + aNewVersion]) {
      aDBConnection.beginTransaction();
      try {
        this["_dbMigrate" + aOldVersion + "To" + aNewVersion](aDBConnection);
        aDBConnection.schemaVersion = aNewVersion;
        aDBConnection.commitTransaction();
      }
      catch(ex) {
        aDBConnection.rollbackTransaction();
        throw ex;
      }
    }
    else
      throw("no migrator function from version " + aOldVersion +
            " to version " + aNewVersion);
  },

  /**
   * If the schema version is 0, that means it was never set, which means
   * the database was somehow created without the schema being applied, perhaps
   * because the system ran out of disk space (although we check for this
   * in _createDB) or because some other code created the database file without
   * applying the schema.  In any case, recover by simply reapplying the schema.
   */
  _dbMigrate0To3: function ContentPrefService___dbMigrate0To3(aDBConnection) {
    this._dbCreateSchema(aDBConnection);
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
  },

  _dbMigrate2To3: function ContentPrefService__dbMigrate2To3(aDBConnection) {
    this._dbCreateIndices(aDBConnection);
  },

  _parseGroupParam: function ContentPrefService__parseGroupParam(aGroup) {
    if (aGroup == null)
      return null;
    if (aGroup.constructor.name == "String")
      return aGroup.toString();
    if (aGroup instanceof Ci.nsIURI)
      return this.grouper.group(aGroup);

    throw Components.Exception("aGroup is not a string, nsIURI or null",
                               Cr.NS_ERROR_ILLEGAL_VALUE);
  },
};


function HostnameGrouper() {}

HostnameGrouper.prototype = {
  //**************************************************************************//
  // XPCOM Plumbing
  
  classID:          Components.ID("{8df290ae-dcaa-4c11-98a5-2429a4dc97bb}"),
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

function AsyncStatement(aStatement) {
  this.stmt = aStatement;
}

AsyncStatement.prototype = {
  execute: function AsyncStmt_execute(aCallback) {
    let stmt = this.stmt;
    stmt.executeAsync({
      _callback: aCallback,
      _hadResult: false,
      handleResult: function(aResult) {
        this._hadResult = true;
        if (this._callback) {
          let row = aResult.getNextRow();
          this._callback.onResult(row.getResultByName("value"));
        }
      },
      handleCompletion: function(aReason) {
        if (!this._hadResult && this._callback &&
            aReason == Ci.mozIStorageStatementCallback.REASON_FINISHED)
          this._callback.onResult(undefined);
      },
      handleError: function(aError) {}
    });
  }
};

//****************************************************************************//
// XPCOM Plumbing

var components = [ContentPrefService, HostnameGrouper];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
