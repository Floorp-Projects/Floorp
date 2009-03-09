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
 * The Original Code is FUEL.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mark Finkle <mfinkle@mozilla.com> (Original Author)
 *  John Resig  <jresig@mozilla.com> (Original Author)
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
function Events() {
  this._listeners = [];
}

//=================================================
// Events implementation
Events.prototype = {
  addListener : function evts_al(aEvent, aListener) {
    if (this._listeners.some(hasFilter))
      return;

    this._listeners.push({
      event: aEvent,
      listener: aListener
    });

    function hasFilter(element) {
      return element.event == aEvent && element.listener == aListener;
    }
  },

  removeListener : function evts_rl(aEvent, aListener) {
    this._listeners = this._listeners.filter(hasFilter);

    function hasFilter(element) {
      return (element.event != aEvent) || (element.listener != aListener);
    }
  },

  dispatch : function evts_dispatch(aEvent, aEventItem) {
    eventItem = new EventItem(aEvent, aEventItem);

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
  this._prefs.QueryInterface(Ci.nsIPrefBranch2);

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
    var items = this._prefs.getChildList("", []);

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
      case Ci.nsIPrefBranch2.PREF_STRING:
        aValue = this._prefs.getComplexValue(aName, Ci.nsISupportsString).data;
        break;
      case Ci.nsIPrefBranch2.PREF_BOOL:
        aValue = this._prefs.getBoolPref(aName);
        break;
      case Ci.nsIPrefBranch2.PREF_INT:
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
      case Ci.nsIPrefBranch2.PREF_STRING:
        value = "String";
        break;
      case Ci.nsIPrefBranch2.PREF_BOOL:
        value = "Boolean";
        break;
      case Ci.nsIPrefBranch2.PREF_INT:
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
  this._prefs = new PreferenceBranch("extensions." + this._item.id + ".");
  this._storage = new SessionStorage();
  this._events = new Events();

  var installPref = "install-event-fired";
  if (!this._prefs.has(installPref)) {
    this._prefs.setValue(installPref, true);
    this._firstRun = true;
  }

  this._enabled = false;
  const PREFIX_ITEM_URI = "urn:mozilla:item:";
  const PREFIX_NS_EM = "http://www.mozilla.org/2004/em-rdf#";
  var rdf = Cc["@mozilla.org/rdf/rdf-service;1"].getService(Ci.nsIRDFService);
  var itemResource = rdf.GetResource(PREFIX_ITEM_URI + this._item.id);
  if (itemResource) {
    var extmgr = Cc["@mozilla.org/extensions/manager;1"].getService(Ci.nsIExtensionManager);
    var ds = extmgr.datasource;
    var target = ds.GetTarget(itemResource, rdf.GetResource(PREFIX_NS_EM + "isDisabled"), true);
    if (target && target instanceof Ci.nsIRDFLiteral)
      this._enabled = (target.Value != "true");
  }

  var os = Components.classes["@mozilla.org/observer-service;1"]
                     .getService(Ci.nsIObserverService);
  os.addObserver(this, "em-action-requested", false);

  var self = this;
  gShutdown.push(function(){ self._shutdown(); });
}

//=================================================
// Extension implementation
Extension.prototype = {
  // cleanup observer so we don't leak
  _shutdown: function ext_shutdown() {
    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Ci.nsIObserverService);
    os.removeObserver(this, "em-action-requested");

    this._prefs = null;
    this._storage = null;
    this._events = null;
  },

  // for nsIObserver
  observe: function ext_observe(aSubject, aTopic, aData)
  {
    if ((aSubject instanceof Ci.nsIUpdateItem) && (aSubject.id == this._item.id))
    {
      if (aData == "item-uninstalled")
        this._events.dispatch("uninstall", this._item.id);
      else if (aData == "item-disabled")
        this._events.dispatch("disable", this._item.id);
      else if (aData == "item-enabled")
        this._events.dispatch("enable", this._item.id);
      else if (aData == "item-cancel-action")
        this._events.dispatch("cancel", this._item.id);
      else if (aData == "item-upgraded")
        this._events.dispatch("upgrade", this._item.id);
    }
  },

  get id() {
    return this._item.id;
  },

  get name() {
    return this._item.name;
  },

  get enabled() {
    return this._enabled;
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
function Extensions() {
  this._extmgr = Components.classes["@mozilla.org/extensions/manager;1"]
                           .getService(Ci.nsIExtensionManager);

  this._cache = {};

  var self = this;
  gShutdown.push(function() { self._shutdown(); });
}

//=================================================
// Extensions implementation
Extensions.prototype = {
  _shutdown : function exts_shutdown() {
    this._extmgr = null;
    this._cache = null;
  },

  /*
   * Helper method to check cache before creating a new extension
   */
  _get : function exts_get(aId) {
    if (this._cache.hasOwnProperty(aId))
      return this._cache[aId];

    var newExt = new Extension(this._extmgr.getItemForID(aId));
    this._cache[aId] = newExt;
    return newExt;
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
    var retVal = [];
    var items = this._extmgr.getItemList(Ci.nsIUpdateItem.TYPE_EXTENSION, {});

    for (var i = 0; i < items.length; i++) {
      retVal.push(this._get(items[i].id));
    }

    return retVal;
  },

  has : function exts_has(aId) {
    return this._extmgr.getItemForID(aId) != null;
  },

  get : function exts_get(aId) {
    return this.has(aId) ? this._get(aId) : null;
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
    this._console = null;
    this._storage = null;
    this._prefs = null;
    this._extensions = null;
    this._events = null;

    this._info = Components.classes["@mozilla.org/xre/app-info;1"]
                           .getService(Ci.nsIXULAppInfo);

    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Ci.nsIObserverService);

    os.addObserver(this, "final-ui-startup", false);
    os.addObserver(this, "quit-application-requested", false);
    os.addObserver(this, "xpcom-shutdown", false);
  },

  // get this contractID registered for certain categories via XPCOMUtils
  _xpcom_categories: [
    // make Application a startup observer
    { category: "app-startup", service: true },

    // add Application as a global property for easy access
    { category: "JavaScript global privileged property" }
  ],

  // for nsIClassInfo
  flags : Ci.nsIClassInfo.SINGLETON,
  implementationLanguage : Ci.nsIProgrammingLanguage.JAVASCRIPT,

  getInterfaces : function app_gi(aCount) {
    var interfaces = [Ci.extIApplication, Ci.nsIObserver, Ci.nsIClassInfo];
    aCount.value = interfaces.length;
    return interfaces;
  },

  getHelperForLanguage : function app_ghfl(aCount) {
    return null;
  },

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
      while (gShutdown.length) {
        gShutdown.shift()();
      }

      // release our observers
      var os = Components.classes["@mozilla.org/observer-service;1"]
                         .getService(Ci.nsIObserverService);

      os.removeObserver(this, "final-ui-startup");
      os.removeObserver(this, "quit-application-requested");
      os.removeObserver(this, "xpcom-shutdown");

      this._info = null;
      this._console = null;
      this._prefs = null;
      this._storage = null;
      this._events = null;
      this._extensions = null;
    }
  },

  get console() {
    if (this._console == null)
        this._console = new Console();

    return this._console;
  },

  get storage() {
    if (this._storage == null)
        this._storage = new SessionStorage();

    return this._storage;
  },

  get prefs() {
    if (this._prefs == null)
        this._prefs = new PreferenceBranch("");

    return this._prefs;
  },

  get extensions() {
    if (this._extensions == null)
      this._extensions = new Extensions();

    return this._extensions;
  },

  get events() {
    if (this._events == null)
        this._events = new Events();

    return this._events;
  },

  // helper method for correct quitting/restarting
  _quitWithFlags: function app__quitWithFlags(aFlags) {
    let os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    let cancelQuit = Components.classes["@mozilla.org/supports-PRBool;1"]
                               .createInstance(Components.interfaces.nsISupportsPRBool);
    os.notifyObservers(cancelQuit, "quit-application-requested", null);
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
