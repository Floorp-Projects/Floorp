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
 * The Original Code is Special Powers code
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Clint Talbert cmtalbert@gmail.com
 *   Joel Maher joel.maher@gmail.com
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
 * ***** END LICENSE BLOCK *****/
/* This code is loaded in every child process that is started by mochitest in
 * order to be used as a replacement for UniversalXPConnect
 */

var Ci = Components.interfaces;
var Cc = Components.classes;

Components.utils.import("resource://mochikit/MockFilePicker.jsm");

function SpecialPowersAPI() { 
  this._consoleListeners = [];
  this._encounteredCrashDumpFiles = [];
  this._unexpectedCrashDumpFiles = { };
  this._crashDumpDir = null;
  this._mfl = null;
  this._prefEnvUndoStack = [];
  this._pendingPrefs = [];
  this._applyingPrefs = false;
  this._fm = null;
  this._cb = null;
}

function bindDOMWindowUtils(aWindow) {
  if (!aWindow)
    return

  var util = aWindow.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                   .getInterface(Components.interfaces.nsIDOMWindowUtils);
  // This bit of magic brought to you by the letters
  // B Z, and E, S and the number 5.
  //
  // Take all of the properties on the nsIDOMWindowUtils-implementing
  // object, and rebind them onto a new object with a stub that uses
  // apply to call them from this privileged scope. This way we don't
  // have to explicitly stub out new methods that appear on
  // nsIDOMWindowUtils.
  var proto = Object.getPrototypeOf(util);
  var target = {};
  function rebind(desc, prop) {
    if (prop in desc && typeof(desc[prop]) == "function") {
      var oldval = desc[prop];
      try {
        desc[prop] = function() { return oldval.apply(util, arguments); };
      } catch (ex) {
        dump("WARNING: Special Powers failed to rebind function: " + desc + "::" + prop + "\n");
      }
    }
  }
  for (var i in proto) {
    var desc = Object.getOwnPropertyDescriptor(proto, i);
    rebind(desc, "get");
    rebind(desc, "set");
    rebind(desc, "value");
    Object.defineProperty(target, i, desc);
  }
  return target;
}

function Observer(specialPowers, aTopic, aCallback, aIsPref) {
  this._sp = specialPowers;
  this._topic = aTopic;
  this._callback = aCallback;
  this._isPref = aIsPref;
}

Observer.prototype = {
  _sp: null,
  _topic: null,
  _callback: null,
  _isPref: false,

  observe: function(aSubject, aTopic, aData) {
    if ((!this._isPref && aTopic == this._topic) ||
        (this._isPref && aTopic == "nsPref:changed")) {
      if (aData == this._topic) {
       this.cleanup();
        /* The callback must execute asynchronously after all the preference observers have run */
        content.window.setTimeout(this._callback, 0);
        content.window.setTimeout(this._sp._finishPrefEnv, 0);
      }
    }
  },

  cleanup: function() {
    if (this._isPref) {
      var os = Cc["@mozilla.org/preferences-service;1"].getService()
               .QueryInterface(Ci.nsIPrefBranch2);
      os.removeObserver(this._topic, this);
    } else {
      var os = Cc["@mozilla.org/observer-service;1"]
              .getService(Ci.nsIObserverService)
              .QueryInterface(Ci.nsIObserverService);
      os.removeObserver(this, this._topic);
    }
  },
};

SpecialPowersAPI.prototype = {

  get MockFilePicker() {
    return MockFilePicker
  },

  getDOMWindowUtils: function(aWindow) {
    if (aWindow == this.window && this.DOMWindowUtils != null)
      return this.DOMWindowUtils;

    return bindDOMWindowUtils(aWindow);
  },

  removeExpectedCrashDumpFiles: function(aExpectingProcessCrash) {
    var success = true;
    if (aExpectingProcessCrash) {
      var message = {
        op: "delete-crash-dump-files",
        filenames: this._encounteredCrashDumpFiles 
      };
      if (!this._sendSyncMessage("SPProcessCrashService", message)[0]) {
        success = false;
      }
    }
    this._encounteredCrashDumpFiles.length = 0;
    return success;
  },

  findUnexpectedCrashDumpFiles: function() {
    var self = this;
    var message = {
      op: "find-crash-dump-files",
      crashDumpFilesToIgnore: this._unexpectedCrashDumpFiles
    };
    var crashDumpFiles = this._sendSyncMessage("SPProcessCrashService", message)[0];
    crashDumpFiles.forEach(function(aFilename) {
      self._unexpectedCrashDumpFiles[aFilename] = true;
    });
    return crashDumpFiles;
  },

  /*
   * Take in a list of prefs and put the original value on the _prefEnvUndoStack so we can undo
   * preferences that we set.  Note, that this is a stack of values to revert to, not
   * what we have set.
   *
   * prefs: {set|clear: [[pref, value], [pref, value, Iid], ...], set|clear: [[pref, value], ...], ...}
   * ex: {'set': [['foo.bar', 2], ['browser.magic', '0xfeedface']], 'remove': [['bad.pref']] }
   *
   * In the scenario where our prefs specify the same pref more than once, we do not guarantee
   * the behavior.  
   *
   * If a preference is not changing a value, we will ignore it.
   *
   * TODO: complex values for original cleanup?
   *
   */
  pushPrefEnv: function(inPrefs, callback) {
    var prefs = Components.classes["@mozilla.org/preferences-service;1"].
                           getService(Components.interfaces.nsIPrefBranch);

    var pref_string = [];
    pref_string[prefs.PREF_INT] = "INT";
    pref_string[prefs.PREF_BOOL] = "BOOL";
    pref_string[prefs.PREF_STRING] = "STRING";

    var pendingActions = [];
    var cleanupActions = [];

    for (var action in inPrefs) { /* set|clear */
      for (var idx in inPrefs[action]) {
        var aPref = inPrefs[action][idx];
        var prefName = aPref[0];
        var prefValue = null;
        var prefIid = null;
        var prefType = prefs.PREF_INVALID;
        var originalValue = null;

        if (aPref.length == 3) {
          prefValue = aPref[1];
          prefIid = aPref[2];
        } else if (aPref.length == 2) {
          prefValue = aPref[1];
        }

        /* If pref is not found or invalid it doesn't exist. */
        if (prefs.getPrefType(prefName) != prefs.PREF_INVALID) {
          prefType = pref_string[prefs.getPrefType(prefName)];
          if ((prefs.prefHasUserValue(prefName) && action == 'clear') ||
              (action == 'set'))
            originalValue = this._getPref(prefName, prefType);
        } else if (action == 'set') {
          /* prefName doesn't exist, so 'clear' is pointless */
          if (aPref.length == 3) {
            prefType = "COMPLEX";
          } else if (aPref.length == 2) {
            if (typeof(prefValue) == "boolean") 
              prefType = "BOOL";
            else if (typeof(prefValue) == "number")
              prefType = "INT";
            else if (typeof(prefValue) == "string")
              prefType = "CHAR";
          }
        }

        /* PREF_INVALID: A non existing pref which we are clearing or invalid values for a set */
        if (prefType == prefs.PREF_INVALID)
          continue;

        /* We are not going to set a pref if the value is the same */
        if (originalValue == prefValue)
          continue;

        pendingActions.push({'action': action, 'type': prefType, 'name': prefName, 'value': prefValue, 'Iid': prefIid});

        /* Push original preference value or clear into cleanup array */
        var cleanupTodo = {'action': action, 'type': prefType, 'name': prefName, 'value': originalValue, 'Iid': prefIid};
        if (originalValue == null) {
          cleanupTodo.action = 'clear';
        } else {
          cleanupTodo.action = 'set';
        }
        cleanupActions.push(cleanupTodo);
      }
    }

    if (pendingActions.length > 0) {
      this._prefEnvUndoStack.push(cleanupActions);
      this._pendingPrefs.push([pendingActions, callback]);
      this._applyPrefs();
    } else {
      content.window.setTimeout(callback, 0);
    }
  },

  popPrefEnv: function(callback) {
    if (this._prefEnvUndoStack.length > 0) {
      /* Each pop will have a valid block of preferences */
      this._pendingPrefs.push([this._prefEnvUndoStack.pop(), callback]);
      this._applyPrefs();
    } else {
      content.window.setTimeout(callback, 0);
    }
  },

  flushPrefEnv: function(callback) {
    while (this._prefEnvUndoStack.length > 1)
      this.popPrefEnv(null);

    this.popPrefEnv(callback);
  },

  /*
    Iterate through one atomic set of pref actions and perform sets/clears as appropriate. 
    All actions performed must modify the relevant pref.
  */
  _applyPrefs: function() {
    if (this._applyingPrefs || this._pendingPrefs.length <= 0) {
      return;
    }

    /* Set lock and get prefs from the _pendingPrefs queue */
    this._applyingPrefs = true;
    var transaction = this._pendingPrefs.shift();
    var pendingActions = transaction[0];
    var callback = transaction[1];

    var lastPref = pendingActions[pendingActions.length-1];
    this._addObserver(lastPref.name, callback, true);

    for (var idx in pendingActions) {
      var pref = pendingActions[idx];
      if (pref.action == 'set') {
        this._setPref(pref.name, pref.type, pref.value, pref.Iid);
      } else if (pref.action == 'clear') {
        this.clearUserPref(pref.name);
      }
    }
  },

  _addObserver: function(aTopic, aCallback, aIsPref) {
    var observer = new Observer(this, aTopic, aCallback, aIsPref);

    if (aIsPref) {
      var os = Cc["@mozilla.org/preferences-service;1"].getService()
               .QueryInterface(Ci.nsIPrefBranch2);	
      os.addObserver(aTopic, observer, false);
    } else {
      var os = Cc["@mozilla.org/observer-service;1"]
              .getService(Ci.nsIObserverService)
              .QueryInterface(Ci.nsIObserverService);
      os.addObserver(observer, aTopic, false);
    }
  },

  /* called from the observer when we get a pref:changed.  */
  _finishPrefEnv: function() {
    /*
      Any subsequent pref environment pushes that occurred while waiting 
      for the preference update are pending, and will now be executed.
    */
    this.wrappedJSObject.SpecialPowers._applyingPrefs = false;
    this.wrappedJSObject.SpecialPowers._applyPrefs();
  },

  addObserver: function(obs, notification, weak) {
    var obsvc = Cc['@mozilla.org/observer-service;1']
                   .getService(Ci.nsIObserverService);
    obsvc.addObserver(obs, notification, weak);
  },
  removeObserver: function(obs, notification) {
    var obsvc = Cc['@mozilla.org/observer-service;1']
                   .getService(Ci.nsIObserverService);
    obsvc.removeObserver(obs, notification);
  },
   
  can_QI: function(obj) {
    return obj.QueryInterface !== undefined;
  },
  do_QueryInterface: function(obj, iface) {
    return obj.QueryInterface(Ci[iface]); 
  },

  // Mimic the get*Pref API
  getBoolPref: function(aPrefName) {
    return (this._getPref(aPrefName, 'BOOL'));
  },
  getIntPref: function(aPrefName) {
    return (this._getPref(aPrefName, 'INT'));
  },
  getCharPref: function(aPrefName) {
    return (this._getPref(aPrefName, 'CHAR'));
  },
  getComplexValue: function(aPrefName, aIid) {
    return (this._getPref(aPrefName, 'COMPLEX', aIid));
  },

  // Mimic the set*Pref API
  setBoolPref: function(aPrefName, aValue) {
    return (this._setPref(aPrefName, 'BOOL', aValue));
  },
  setIntPref: function(aPrefName, aValue) {
    return (this._setPref(aPrefName, 'INT', aValue));
  },
  setCharPref: function(aPrefName, aValue) {
    return (this._setPref(aPrefName, 'CHAR', aValue));
  },
  setComplexValue: function(aPrefName, aIid, aValue) {
    return (this._setPref(aPrefName, 'COMPLEX', aValue, aIid));
  },

  // Mimic the clearUserPref API
  clearUserPref: function(aPrefName) {
    var msg = {'op':'clear', 'prefName': aPrefName, 'prefType': ""};
    this._sendSyncMessage('SPPrefService', msg);
  },

  // Private pref functions to communicate to chrome
  _getPref: function(aPrefName, aPrefType, aIid) {
    var msg = {};
    if (aIid) {
      // Overloading prefValue to handle complex prefs
      msg = {'op':'get', 'prefName': aPrefName, 'prefType':aPrefType, 'prefValue':[aIid]};
    } else {
      msg = {'op':'get', 'prefName': aPrefName,'prefType': aPrefType};
    }
    var val = this._sendSyncMessage('SPPrefService', msg);

    if (val == null || val[0] == null)
      throw "Error getting pref";
    return val[0];
  },
  _setPref: function(aPrefName, aPrefType, aValue, aIid) {
    var msg = {};
    if (aIid) {
      msg = {'op':'set','prefName':aPrefName, 'prefType': aPrefType, 'prefValue': [aIid,aValue]};
    } else {
      msg = {'op':'set', 'prefName': aPrefName, 'prefType': aPrefType, 'prefValue': aValue};
    }
    return(this._sendSyncMessage('SPPrefService', msg)[0]);
  },

  //XXX: these APIs really ought to be removed, they're not e10s-safe.
  // (also they're pretty Firefox-specific)
  _getTopChromeWindow: function(window) {
    return window.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIWebNavigation)
                 .QueryInterface(Ci.nsIDocShellTreeItem)
                 .rootTreeItem
                 .QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIDOMWindow)
                 .QueryInterface(Ci.nsIDOMChromeWindow);
  },
  _getDocShell: function(window) {
    return window.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIWebNavigation)
                 .QueryInterface(Ci.nsIDocShell);
  },
  _getMUDV: function(window) {
    return this._getDocShell(window).contentViewer
               .QueryInterface(Ci.nsIMarkupDocumentViewer);
  },
  _getAutoCompletePopup: function(window) {
    return this._getTopChromeWindow(window).document
                                           .getElementById("PopupAutoComplete");
  },
  addAutoCompletePopupEventListener: function(window, listener) {
    this._getAutoCompletePopup(window).addEventListener("popupshowing",
                                                        listener,
                                                        false);
  },
  removeAutoCompletePopupEventListener: function(window, listener) {
    this._getAutoCompletePopup(window).removeEventListener("popupshowing",
                                                           listener,
                                                           false);
  },
  isBackButtonEnabled: function(window) {
    return !this._getTopChromeWindow(window).document
                                      .getElementById("Browser:Back")
                                      .hasAttribute("disabled");
  },

  addChromeEventListener: function(type, listener, capture, allowUntrusted) {
    addEventListener(type, listener, capture, allowUntrusted);
  },
  removeChromeEventListener: function(type, listener, capture) {
    removeEventListener(type, listener, capture);
  },

  addErrorConsoleListener: function(listener) {
    var consoleListener = {
      userListener: listener,
      observe: function(consoleMessage) {
        this.userListener(consoleMessage.message);
      }
    };

    Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService)
                                       .registerListener(consoleListener);

    this._consoleListeners.push(consoleListener);
  },

  removeErrorConsoleListener: function(listener) {
    for (var index in this._consoleListeners) {
      var consoleListener = this._consoleListeners[index];
      if (consoleListener.userListener == listener) {
        Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService)
                                           .unregisterListener(consoleListener);
        this._consoleListeners = this._consoleListeners.splice(index, 1);
        break;
      }
    }
  },

  getFullZoom: function(window) {
    return this._getMUDV(window).fullZoom;
  },
  setFullZoom: function(window, zoom) {
    this._getMUDV(window).fullZoom = zoom;
  },
  getTextZoom: function(window) {
    return this._getMUDV(window).textZoom;
  },
  setTextZoom: function(window, zoom) {
    this._getMUDV(window).textZoom = zoom;
  },

  createSystemXHR: function() {
    return Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
             .createInstance(Ci.nsIXMLHttpRequest);
  },

  snapshotWindow: function (win, withCaret) {
    var el = this.window.document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    el.width = win.innerWidth;
    el.height = win.innerHeight;
    var ctx = el.getContext("2d");
    var flags = 0;

    ctx.drawWindow(win, win.scrollX, win.scrollY,
                   win.innerWidth, win.innerHeight,
                   "rgb(255,255,255)",
                   withCaret ? ctx.DRAWWINDOW_DRAW_CARET : 0);
    return el;
  },

  gc: function() {
    this.DOMWindowUtils.garbageCollect();
  },

  forceGC: function() {
    Components.utils.forceGC();
  },

  hasContentProcesses: function() {
    try {
      var rt = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime);
      return rt.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
    } catch (e) {
      return true;
    }
  },

  _xpcomabi: null,

  get XPCOMABI() {
    if (this._xpcomabi != null)
      return this._xpcomabi;

    var xulRuntime = Cc["@mozilla.org/xre/app-info;1"]
                        .getService(Components.interfaces.nsIXULAppInfo)
                        .QueryInterface(Components.interfaces.nsIXULRuntime);

    this._xpcomabi = xulRuntime.XPCOMABI;
    return this._xpcomabi;
  },

  executeSoon: function(aFunc) {
    var tm = Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager);
    tm.mainThread.dispatch({
      run: function() {
        aFunc();
      }
    }, Ci.nsIThread.DISPATCH_NORMAL);
  },

  _os: null,

  get OS() {
    if (this._os != null)
      return this._os;

    var xulRuntime = Cc["@mozilla.org/xre/app-info;1"]
                        .getService(Components.interfaces.nsIXULAppInfo)
                        .QueryInterface(Components.interfaces.nsIXULRuntime);

    this._os = xulRuntime.OS;
    return this._os;
  },

  addSystemEventListener: function(target, type, listener, useCapture) {
    Cc["@mozilla.org/eventlistenerservice;1"].
      getService(Ci.nsIEventListenerService).
      addSystemEventListener(target, type, listener, useCapture);
  },
  removeSystemEventListener: function(target, type, listener, useCapture) {
    Cc["@mozilla.org/eventlistenerservice;1"].
      getService(Ci.nsIEventListenerService).
      removeSystemEventListener(target, type, listener, useCapture);
  },

  setLogFile: function(path) {
    this._mfl = new MozillaFileLogger(path);
  },

  log: function(data) {
    this._mfl.log(data);
  },

  closeLogFile: function() {
    this._mfl.close();
  },

  addCategoryEntry: function(category, entry, value, persists, replace) {
    Components.classes["@mozilla.org/categorymanager;1"].
      getService(Components.interfaces.nsICategoryManager).
      addCategoryEntry(category, entry, value, persists, replace);
  },

  getNodePrincipal: function(aNode) {
      return aNode.nodePrincipal;
  },

  getNodeBaseURIObject: function(aNode) {
      return aNode.baseURIObject;
  },

  getDocumentURIObject: function(aDocument) {
      return aDocument.documentURIObject;
  },

  copyString: function(str) {
    Components.classes["@mozilla.org/widget/clipboardhelper;1"].
      getService(Components.interfaces.nsIClipboardHelper).
      copyString(str);
  },

  // :jdm gets credit for this.  ex: getPrivilegedProps(window, 'location.href');
  getPrivilegedProps: function(obj, props) {
    parts = props.split('.');

    for (var i = 0; i < parts.length; i++) {
      var p = parts[i];
      if (obj[p]) {
        obj = obj[p];
      } else {
        return null;
      }
    }
    return obj;
  },

  get focusManager() {
    if (this._fm != null)
      return this._fm;

    this._fm = Components.classes["@mozilla.org/focus-manager;1"].
                        getService(Components.interfaces.nsIFocusManager);

    return this._fm;
  },

  getFocusedElementForWindow: function(targetWindow, aDeep, childTargetWindow) {
    this.focusManager.getFocusedElementForWindow(targetWindow, aDeep, childTargetWindow);
  },

  activeWindow: function() {
    return this.focusManager.activeWindow;
  },

  focusedWindow: function() {
    return this.focusManager.focusedWindow;
  },

  focus: function(window) {
    window.focus();
  },
  
  getClipboardData: function(flavor) {  
    if (this._cb == null)
      this._cb = Components.classes["@mozilla.org/widget/clipboard;1"].
                            getService(Components.interfaces.nsIClipboard);

    var xferable = Components.classes["@mozilla.org/widget/transferable;1"].
                   createInstance(Components.interfaces.nsITransferable);
    xferable.addDataFlavor(flavor);
    this._cb.getData(xferable, this._cb.kGlobalClipboard);
    var data = {};
    try {
      xferable.getTransferData(flavor, data, {});
    } catch (e) {}
    data = data.value || null;
    if (data == null)
      return "";
      
    return data.QueryInterface(Components.interfaces.nsISupportsString).data;
  },

  clipboardCopyString: function(preExpectedVal) {  
    var cbHelperSvc = Components.classes["@mozilla.org/widget/clipboardhelper;1"].
                      getService(Components.interfaces.nsIClipboardHelper);
    cbHelperSvc.copyString(preExpectedVal);
  },

  snapshotWindow: function (win, withCaret) {
    var el = this.window.document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    el.width = win.innerWidth;
    el.height = win.innerHeight;
    var ctx = el.getContext("2d");
    var flags = 0;

    ctx.drawWindow(win, win.scrollX, win.scrollY,
                   win.innerWidth, win.innerHeight,
                   "rgb(255,255,255)",
                   withCaret ? ctx.DRAWWINDOW_DRAW_CARET : 0);
    return el;
  },
  
  swapFactoryRegistration: function(cid, contractID, newFactory, oldFactory) {  
    var componentRegistrar = Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar);

    var unregisterFactory = newFactory;
    var registerFactory = oldFactory;
    
    if (cid == null) {
      if (contractID != null) {
        cid = componentRegistrar.contractIDToCID(contractID);
        oldFactory = Components.manager.getClassObject(Components.classes[contractID],
                                                            Components.interfaces.nsIFactory);
      } else {
        return {'error': "trying to register a new contract ID: Missing contractID"};
      }

      unregisterFactory = oldFactory;
      registerFactory = newFactory;
    }
    componentRegistrar.unregisterFactory(cid,
                                         unregisterFactory);

    // Restore the original factory.
    componentRegistrar.registerFactory(cid,
                                       "",
                                       contractID,
                                       registerFactory);
    return {'cid':cid, 'originalFactory':oldFactory};
  },
  
  _getElement: function(aWindow, id) {
    return ((typeof(id) == "string") ?
        aWindow.document.getElementById(id) : id); 
  },
  
  dispatchEvent: function(aWindow, target, event) {
    var el = this._getElement(aWindow, target);
    return el.dispatchEvent(event);
  },
};

