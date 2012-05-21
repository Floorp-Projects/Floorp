/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");

//=================================================
// Shutdown - used to store cleanup functions which will
//            be called on Application shutdown
var gShutdown = [];

//=================================================
// Console constructor
function Console() {
  this._console = Components.classes["@mozilla.org/consoleservice;1"]
    .getService(Ci.nsIConsoleService);
}

//=================================================
// Console implementation
Console.prototype = {
  log : function cs_log(aMsg) {
    this._console.logStringMessage(aMsg);
  },

  open : function cs_open() {
    var wMediator = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                              .getService(Ci.nsIWindowMediator);
    var console = wMediator.getMostRecentWindow("global:console");
    if (!console) {
      var wWatch = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                             .getService(Ci.nsIWindowWatcher);
      wWatch.openWindow(null, "chrome://global/content/console.xul", "_blank",
                        "chrome,dialog=no,all", null);
    } else {
      // console was already open
      console.focus();
    }
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.extIConsole])
};


//=================================================
// EventItem constructor
function EventItem(aType, aData) {
  this._type = aType;
  this._data = aData;
}

//=================================================
// EventItem implementation
EventItem.prototype = {
  _cancel : false,

  get type() {
    return this._type;
  },

  get data() {
    return this._data;
  },

  preventDefault : function ei_pd() {
    this._cancel = true;
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.extIEventItem])
};


//=================================================
// Events constructor
function Events(notifier) {
  this._listeners = [];
  this._notifier = notifier;
}

//=================================================
// Events implementation
Events.prototype = {
  addListener : function evts_al(aEvent, aListener) {
    function hasFilter(element) {
      return element.event == aEvent && element.listener == aListener;
    }

    if (this._listeners.some(hasFilter))
      return;

    this._listeners.push({
      event: aEvent,
      listener: aListener
    });

    if (this._notifier) {
      this._notifier(aEvent, aListener);
    }
  },

  removeListener : function evts_rl(aEvent, aListener) {
    function hasFilter(element) {
      return (element.event != aEvent) || (element.listener != aListener);
    }

    this._listeners = this._listeners.filter(hasFilter);
  },

  dispatch : function evts_dispatch(aEvent, aEventItem) {
    var eventItem = new EventItem(aEvent, aEventItem);

    this._listeners.forEach(function(key){
      if (key.event == aEvent) {
        key.listener.handleEvent ?
          key.listener.handleEvent(eventItem) :
          key.listener(eventItem);
      }
    });

    return !eventItem._cancel;
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.extIEvents])
};


//=================================================
// PreferenceBranch constructor
function PreferenceBranch(aBranch) {
  if (!aBranch)
    aBranch = "";

  this._root = aBranch;
  this._prefs = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(Ci.nsIPrefService);

  if (aBranch)
    this._prefs = this._prefs.getBranch(aBranch);

  this._prefs.QueryInterface(Ci.nsIPrefBranch);

  // we want to listen to "all" changes for this branch, so pass in a blank domain
  this._prefs.addObserver("", this, true);
  this._events = new Events();

  var self = this;
  gShutdown.push(function() { self._shutdown(); });
}

//=================================================
// PreferenceBranch implementation
PreferenceBranch.prototype = {
  // cleanup observer so we don't leak
  _shutdown: function prefs_shutdown() {
    this._prefs.removeObserver(this._root, this);

    this._prefs = null;
    this._events = null;
  },

  // for nsIObserver
  observe: function prefs_observe(aSubject, aTopic, aData) {
    if (aTopic == "nsPref:changed")
      this._events.dispatch("change", aData);
  },

  get root() {
    return this._root;
  },

  get all() {
    return this.find({});
  },

  get events() {
    return this._events;
  },

  // XXX: Disabled until we can figure out the wrapped object issues
  // name: "name" or /name/
  // path: "foo.bar." or "" or /fo+\.bar/
  // type: Boolean, Number, String (getPrefType)
  // locked: true, false (prefIsLocked)
  // modified: true, false (prefHasUserValue)
  find : function prefs_find(aOptions) {
    var retVal = [];
    var items = this._prefs.getChildList("");

    for (var i = 0; i < items.length; i++) {
      retVal.push(new Preference(items[i], this));
    }

    return retVal;
  },

  has : function prefs_has(aName) {
    return (this._prefs.getPrefType(aName) != Ci.nsIPrefBranch.PREF_INVALID);
  },

  get : function prefs_get(aName) {
    return this.has(aName) ? new Preference(aName, this) : null;
  },

  getValue : function prefs_gv(aName, aValue) {
    var type = this._prefs.getPrefType(aName);

    switch (type) {
      case Ci.nsIPrefBranch.PREF_STRING:
        aValue = this._prefs.getComplexValue(aName, Ci.nsISupportsString).data;
        break;
      case Ci.nsIPrefBranch.PREF_BOOL:
        aValue = this._prefs.getBoolPref(aName);
        break;
      case Ci.nsIPrefBranch.PREF_INT:
        aValue = this._prefs.getIntPref(aName);
        break;
    }

    return aValue;
  },

  setValue : function prefs_sv(aName, aValue) {
    var type = aValue != null ? aValue.constructor.name : "";

    switch (type) {
      case "String":
        var str = Components.classes["@mozilla.org/supports-string;1"]
                            .createInstance(Ci.nsISupportsString);
        str.data = aValue;
        this._prefs.setComplexValue(aName, Ci.nsISupportsString, str);
        break;
      case "Boolean":
        this._prefs.setBoolPref(aName, aValue);
        break;
      case "Number":
        this._prefs.setIntPref(aName, aValue);
        break;
      default:
        throw("Unknown preference value specified.");
    }
  },

  reset : function prefs_reset() {
    this._prefs.resetBranch("");
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.extIPreferenceBranch, Ci.nsISupportsWeakReference])
};


//=================================================
// Preference constructor
function Preference(aName, aBranch) {
  this._name = aName;
  this._branch = aBranch;
  this._events = new Events();

  var self = this;

  this.branch.events.addListener("change", function(aEvent){
    if (aEvent.data == self.name)
      self.events.dispatch(aEvent.type, aEvent.data);
  });
}

//=================================================
// Preference implementation
Preference.prototype = {
  get name() {
    return this._name;
  },

  get type() {
    var value = "";
    var type = this.branch._prefs.getPrefType(this._name);

    switch (type) {
      case Ci.nsIPrefBranch.PREF_STRING:
        value = "String";
        break;
      case Ci.nsIPrefBranch.PREF_BOOL:
        value = "Boolean";
        break;
      case Ci.nsIPrefBranch.PREF_INT:
        value = "Number";
        break;
    }

    return value;
  },

  get value() {
    return this.branch.getValue(this._name, null);
  },

  set value(aValue) {
    return this.branch.setValue(this._name, aValue);
  },

  get locked() {
    return this.branch._prefs.prefIsLocked(this.name);
  },

  set locked(aValue) {
    this.branch._prefs[ aValue ? "lockPref" : "unlockPref" ](this.name);
  },

  get modified() {
    return this.branch._prefs.prefHasUserValue(this.name);
  },

  get branch() {
    return this._branch;
  },

  get events() {
    return this._events;
  },

  reset : function pref_reset() {
    this.branch._prefs.clearUserPref(this.name);
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.extIPreference])
};


//=================================================
// SessionStorage constructor
function SessionStorage() {
  this._storage = {};
  this._events = new Events();
}

//=================================================
// SessionStorage implementation
SessionStorage.prototype = {
  get events() {
    return this._events;
  },

  has : function ss_has(aName) {
    return this._storage.hasOwnProperty(aName);
  },

  set : function ss_set(aName, aValue) {
    this._storage[aName] = aValue;
    this._events.dispatch("change", aName);
  },

  get : function ss_get(aName, aDefaultValue) {
    return this.has(aName) ? this._storage[aName] : aDefaultValue;
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.extISessionStorage])
};


//=================================================
// Extension constructor
function Extension(aItem) {
  this._item = aItem;
  this._firstRun = false;
  this._prefs = new PreferenceBranch("extensions." + this.id + ".");
  this._storage = new SessionStorage();
  this._events = new Events();

  var installPref = "install-event-fired";
  if (!this._prefs.has(installPref)) {
    this._prefs.setValue(installPref, true);
    this._firstRun = true;
  }

  AddonManager.addAddonListener(this);
  AddonManager.addInstallListener(this);

  var self = this;
  gShutdown.push(function(){ self._shutdown(); });
}

//=================================================
// Extension implementation
Extension.prototype = {
  // cleanup observer so we don't leak
  _shutdown: function ext_shutdown() {
    AddonManager.removeAddonListener(this);
    AddonManager.removeInstallListener(this);

    this._prefs = null;
    this._storage = null;
    this._events = null;
  },

  // for AddonListener
  onDisabling: function(addon, needsRestart) {
    if (addon.id == this.id)
      this._events.dispatch("disable", this.id);
  },

  onEnabling: function(addon, needsRestart) {
    if (addon.id == this.id)
      this._events.dispatch("enable", this.id);
  },

  onUninstalling: function(addon, needsRestart) {
    if (addon.id == this.id)
      this._events.dispatch("uninstall", this.id);
  },

  onOperationCancelled: function(addon) {
    if (addon.id == this.id)
      this._events.dispatch("cancel", this.id);
  },

  onInstallEnded: function(install, addon) {
    if (addon.id == this.id)
      this._events.dispatch("upgrade", this.id);
  },

  get id() {
    return this._item.id;
  },

  get name() {
    return this._item.name;
  },

  get enabled() {
    return this._item.isActive;
  },

  get version() {
    return this._item.version;
  },

  get firstRun() {
    return this._firstRun;
  },

  get storage() {
    return this._storage;
  },

  get prefs() {
    return this._prefs;
  },

  get events() {
    return this._events;
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.extIExtension])
};


//=================================================
// Extensions constructor
function Extensions(addons) {
  this._cache = {};

  addons.forEach(function(addon) {
    this._cache[addon.id] = new Extension(addon);
  }, this);

  var self = this;
  gShutdown.push(function() { self._shutdown(); });
}

//=================================================
// Extensions implementation
Extensions.prototype = {
  _shutdown : function exts_shutdown() {
    this._cache = null;
  },

  get all() {
    return this.find({});
  },

  // XXX: Disabled until we can figure out the wrapped object issues
  // id: "some@id" or /id/
  // name: "name" or /name/
  // version: "1.0.1"
  // minVersion: "1.0"
  // maxVersion: "2.0"
  find : function exts_find(aOptions) {
    return [e for each (e in this._cache)];
  },

  has : function exts_has(aId) {
    return aId in this._cache;
  },

  get : function exts_get(aId) {
    return this.has(aId) ? this._cache[aId] : null;
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.extIExtensions])
};

//=================================================
// extApplication constructor
function extApplication() {
}

//=================================================
// extApplication implementation
extApplication.prototype = {
  initToolkitHelpers: function extApp_initToolkitHelpers() {
    XPCOMUtils.defineLazyServiceGetter(this, "_info",
                                       "@mozilla.org/xre/app-info;1",
                                       "nsIXULAppInfo");

    // While the other event listeners are loaded only if needed,
    // FUEL *must* listen for shutdown in order to clean up it's
    // references to various services, and to remove itself as
    // observer of any other notifications.
    this._obs = Cc["@mozilla.org/observer-service;1"].
                getService(Ci.nsIObserverService);
    this._obs.addObserver(this, "xpcom-shutdown", false);
    this._registered = {"unload": true};
  },

  classInfo: XPCOMUtils.generateCI({interfaces: [Ci.extIApplication,
                                                 Ci.nsIObserver],
                                    flags: Ci.nsIClassInfo.SINGLETON}),

  // extIApplication
  get id() {
    return this._info.ID;
  },

  get name() {
    return this._info.name;
  },

  get version() {
    return this._info.version;
  },

  // for nsIObserver
  observe: function app_observe(aSubject, aTopic, aData) {
    if (aTopic == "app-startup") {
      this.events.dispatch("load", "application");
    }
    else if (aTopic == "final-ui-startup") {
      this.events.dispatch("ready", "application");
    }
    else if (aTopic == "quit-application-requested") {
      // we can stop the quit by checking the return value
      if (this.events.dispatch("quit", "application") == false)
        aSubject.data = true;
    }
    else if (aTopic == "xpcom-shutdown") {

      this.events.dispatch("unload", "application");

      // call the cleanup functions and empty the array
      for (let i = 0; i < gShutdown.length; i++) {
        gShutdown[i]();
      }
      gShutdown.splice(0, gShutdown.length);

      // release our observers
      this._obs.removeObserver(this, "app-startup");
      this._obs.removeObserver(this, "final-ui-startup");
      this._obs.removeObserver(this, "quit-application-requested");
      this._obs.removeObserver(this, "xpcom-shutdown");
    }
  },

  get console() {
    let console = new Console();
    this.__defineGetter__("console", function() console);
    return this.console;
  },

  get storage() {
    let storage = new SessionStorage();
    this.__defineGetter__("storage", function() storage);
    return this.storage;
  },

  get prefs() {
    let prefs = new PreferenceBranch("");
    this.__defineGetter__("prefs", function() prefs);
    return this.prefs;
  },

  getExtensions: function(callback) {
    AddonManager.getAddonsByTypes(["extension"], function(addons) {
      callback.callback(new Extensions(addons));
    });
  },

  get events() {

    // This ensures that FUEL only registers for notifications as needed
    // by callers. Note that the unload (xpcom-shutdown) event is listened
    // for by default, as it's needed for cleanup purposes.
    var self = this;
    function registerCheck(aEvent) {
      var rmap = { "load": "app-startup",
                   "ready": "final-ui-startup",
                   "quit": "quit-application-requested"};
      if (!(aEvent in rmap) || aEvent in self._registered)
        return;

      self._obs.addObserver(self, rmap[aEvent]);
      self._registered[aEvent] = true;
    }

    let events = new Events(registerCheck);
    this.__defineGetter__("events", function() events);
    return this.events;
  },

  // helper method for correct quitting/restarting
  _quitWithFlags: function app__quitWithFlags(aFlags) {
    let cancelQuit = Components.classes["@mozilla.org/supports-PRBool;1"]
                               .createInstance(Components.interfaces.nsISupportsPRBool);
    let quitType = aFlags & Components.interfaces.nsIAppStartup.eRestart ? "restart" : null;
    this._obs.notifyObservers(cancelQuit, "quit-application-requested", quitType);
    if (cancelQuit.data)
      return false; // somebody canceled our quit request

    let appStartup = Components.classes['@mozilla.org/toolkit/app-startup;1']
                               .getService(Components.interfaces.nsIAppStartup);
    appStartup.quit(aFlags);
    return true;
  },

  quit: function app_quit() {
    return this._quitWithFlags(Components.interfaces.nsIAppStartup.eAttemptQuit);
  },

  restart: function app_restart() {
    return this._quitWithFlags(Components.interfaces.nsIAppStartup.eAttemptQuit |
                               Components.interfaces.nsIAppStartup.eRestart);
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.extIApplication, Ci.nsISupportsWeakReference])
};
