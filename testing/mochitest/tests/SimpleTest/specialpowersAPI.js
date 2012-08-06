/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
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
  //
  // Note that this will be a chrome object that is (possibly) exposed to
  // content. Make sure to define __exposedProps__ for each property to make
  // sure that it gets through the security membrane.
  var proto = Object.getPrototypeOf(util);
  var target = { __exposedProps__: {} };
  function rebind(desc, prop) {
    if (prop in desc && typeof(desc[prop]) == "function") {
      var oldval = desc[prop];
      try {
        desc[prop] = function() {
          return oldval.apply(util, arguments);
        };
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
    target.__exposedProps__[i] = 'rw';
  }
  return target;
}

function isWrappable(x) {
  if (typeof x === "object")
    return x !== null;
  return typeof x === "function";
};

function isWrapper(x) {
  return isWrappable(x) && (typeof x.SpecialPowers_wrappedObject !== "undefined");
};

function unwrapIfWrapped(x) {
  return isWrapper(x) ? unwrapPrivileged(x) : x;
};

function isXrayWrapper(x) {
  try {
    return /XrayWrapper/.exec(x.toString());
  } catch(e) {
    // The toString() implementation could theoretically throw. But it never
    // throws for Xray, so we can just assume non-xray in that case.
    return false;
  }
}

function callGetOwnPropertyDescriptor(obj, name) {
  // Quickstubbed getters and setters are propertyOps, and don't get reified
  // until someone calls __lookupGetter__ or __lookupSetter__ on them (note
  // that there are special version of those functions for quickstubs, so
  // apply()ing Object.prototype.__lookupGetter__ isn't good enough). Try to
  // trigger reification before calling Object.getOwnPropertyDescriptor.
  //
  // See bug 764315.
  try {
    obj.__lookupGetter__(name);
    obj.__lookupSetter__(name);
  } catch(e) { }
  return Object.getOwnPropertyDescriptor(obj, name);
}

// We can't call apply() directy on Xray-wrapped functions, so we have to be
// clever.
function doApply(fun, invocant, args) {
  return Function.prototype.apply.call(fun, invocant, args);
}

function wrapPrivileged(obj) {

  // Primitives pass straight through.
  if (!isWrappable(obj))
    return obj;

  // No double wrapping.
  if (isWrapper(obj))
    throw "Trying to double-wrap object!";

  // Make our core wrapper object.
  var handler = new SpecialPowersHandler(obj);

  // If the object is callable, make a function proxy.
  if (typeof obj === "function") {
    var callTrap = function() {
      // The invocant and arguments may or may not be wrappers. Unwrap them if necessary.
      var invocant = unwrapIfWrapped(this);
      var unwrappedArgs = Array.prototype.slice.call(arguments).map(unwrapIfWrapped);

      return wrapPrivileged(doApply(obj, invocant, unwrappedArgs));
    };
    var constructTrap = function() {
      // The arguments may or may not be wrappers. Unwrap them if necessary.
      var unwrappedArgs = Array.prototype.slice.call(arguments).map(unwrapIfWrapped);

      // Constructors are tricky, because we can't easily call apply on them.
      // As a workaround, we create a wrapper constructor with the same
      // |prototype| property. ES semantics dictate that the return value from
      // |new| is the return value of the |new|-ed function i.f.f. the returned
      // value is an object. We can thus mimic the behavior of |new|-ing the
      // underlying constructor just be passing along its return value in our
      // constructor.
      var FakeConstructor = function() {
        return doApply(obj, this, unwrappedArgs);
      };
      FakeConstructor.prototype = obj.prototype;

      return wrapPrivileged(new FakeConstructor());
    };

    return Proxy.createFunction(handler, callTrap, constructTrap);
  }

  // Otherwise, just make a regular object proxy.
  return Proxy.create(handler);
};

function unwrapPrivileged(x) {

  // We don't wrap primitives, so sometimes we have a primitive where we'd
  // expect to have a wrapper. The proxy pretends to be the type that it's
  // emulating, so we can just as easily check isWrappable() on a proxy as
  // we can on an unwrapped object.
  if (!isWrappable(x))
    return x;

  // If we have a wrappable type, make sure it's wrapped.
  if (!isWrapper(x))
    throw "Trying to unwrap a non-wrapped object!";

  // Unwrap.
  return x.SpecialPowers_wrappedObject;
};

function crawlProtoChain(obj, fn) {
  var rv = fn(obj);
  if (rv !== undefined)
    return rv;
  if (Object.getPrototypeOf(obj))
    return crawlProtoChain(Object.getPrototypeOf(obj), fn);
};

/*
 * We want to waive the __exposedProps__ security check for SpecialPowers-wrapped
 * objects. We do this by creating a proxy singleton that just always returns 'rw'
 * for any property name.
 */
function ExposedPropsWaiverHandler() {
  // NB: XPConnect denies access if the relevant member of __exposedProps__ is not
  // enumerable.
  var _permit = { value: 'rw', writable: false, configurable: false, enumerable: true };
  return {
    getOwnPropertyDescriptor: function(name) { return _permit; },
    getPropertyDescriptor: function(name) { return _permit; },
    getOwnPropertyNames: function() { throw Error("Can't enumerate ExposedPropsWaiver"); },
    getPropertyNames: function() { throw Error("Can't enumerate ExposedPropsWaiver"); },
    enumerate: function() { throw Error("Can't enumerate ExposedPropsWaiver"); },
    defineProperty: function(name) { throw Error("Can't define props on ExposedPropsWaiver"); },
    delete: function(name) { throw Error("Can't delete props from ExposedPropsWaiver"); }
  };
};
ExposedPropsWaiver = Proxy.create(ExposedPropsWaiverHandler());

function SpecialPowersHandler(obj) {
  this.wrappedObject = obj;
};

// Allow us to transitively maintain the membrane by wrapping descriptors
// we return.
SpecialPowersHandler.prototype.doGetPropertyDescriptor = function(name, own) {

  // Handle our special API.
  if (name == "SpecialPowers_wrappedObject")
    return { value: this.wrappedObject, writeable: false, configurable: false, enumerable: false };

  // Handle __exposedProps__.
  if (name == "__exposedProps__")
    return { value: ExposedPropsWaiver, writable: false, configurable: false, enumerable: false };

  // In general, we want Xray wrappers for content DOM objects, because waiving
  // Xray gives us Xray waiver wrappers that clamp the principal when we cross
  // compartment boundaries. However, Xray adds some gunk to toString(), which
  // has the potential to confuse consumers that aren't expecting Xray wrappers.
  // Since toString() is a non-privileged method that returns only strings, we
  // can just waive Xray for that case.
  var obj = name == 'toString' ? XPCNativeWrapper.unwrap(this.wrappedObject)
                               : this.wrappedObject;

  //
  // Call through to the wrapped object.
  //
  // Note that we have several cases here, each of which requires special handling.
  //
  var desc;

  // Case 1: Own Properties.
  //
  // This one is easy, thanks to Object.getOwnPropertyDescriptor().
  if (own)
    desc = callGetOwnPropertyDescriptor(obj, name);

  // Case 2: Not own, not Xray-wrapped.
  //
  // Here, we can just crawl the prototype chain, calling
  // Object.getOwnPropertyDescriptor until we find what we want.
  //
  // NB: Make sure to check this.wrappedObject here, rather than obj, because
  // we may have waived Xray on obj above.
  else if (!isXrayWrapper(this.wrappedObject))
    desc = crawlProtoChain(obj, function(o) {return callGetOwnPropertyDescriptor(o, name);});

  // Case 3: Not own, Xray-wrapped.
  //
  // This one is harder, because we Xray wrappers are flattened and don't have
  // a prototype. Xray wrappers are proxies themselves, so we'd love to just call
  // through to XrayWrapper<Base>::getPropertyDescriptor(). Unfortunately though,
  // we don't have any way to do that. :-(
  //
  // So we first try with a call to getOwnPropertyDescriptor(). If that fails,
  // we make up a descriptor, using some assumptions about what kinds of things
  // tend to live on the prototypes of Xray-wrapped objects.
  else {
    desc = Object.getOwnPropertyDescriptor(obj, name);
    if (!desc) {
      var getter = Object.prototype.__lookupGetter__.call(obj, name);
      var setter = Object.prototype.__lookupSetter__.call(obj, name);
      if (getter || setter)
        desc = {get: getter, set: setter, configurable: true, enumerable: true};
      else if (name in obj)
        desc = {value: obj[name], writable: false, configurable: true, enumerable: true};
    }
  }

  // Bail if we've got nothing.
  if (typeof desc === 'undefined')
    return undefined;

  // When accessors are implemented as JSPropertyOps rather than JSNatives (ie,
  // QuickStubs), the js engine does the wrong thing and treats it as a value
  // descriptor rather than an accessor descriptor. Jorendorff suggested this
  // little hack to work around it. See bug 520882.
  if (desc && 'value' in desc && desc.value === undefined)
    desc.value = obj[name];

  // A trapping proxy's properties must always be configurable, but sometimes
  // this we get non-configurable properties from Object.getOwnPropertyDescriptor().
  // Tell a white lie.
  desc.configurable = true;

  // Transitively maintain the wrapper membrane.
  function wrapIfExists(key) { if (key in desc) desc[key] = wrapPrivileged(desc[key]); };
  wrapIfExists('value');
  wrapIfExists('get');
  wrapIfExists('set');

  return desc;
};

SpecialPowersHandler.prototype.getOwnPropertyDescriptor = function(name) {
  return this.doGetPropertyDescriptor(name, true);
};

SpecialPowersHandler.prototype.getPropertyDescriptor = function(name) {
  return this.doGetPropertyDescriptor(name, false);
};

function doGetOwnPropertyNames(obj, props) {

  // Insert our special API. It's not enumerable, but getPropertyNames()
  // includes non-enumerable properties.
  var specialAPI = 'SpecialPowers_wrappedObject';
  if (props.indexOf(specialAPI) == -1)
    props.push(specialAPI);

  // Do the normal thing.
  var flt = function(a) { return props.indexOf(a) == -1; };
  props = props.concat(Object.getOwnPropertyNames(obj).filter(flt));

  // If we've got an Xray wrapper, include the expandos as well.
  if ('wrappedJSObject' in obj)
    props = props.concat(Object.getOwnPropertyNames(obj.wrappedJSObject)
                         .filter(flt));

  return props;
}

SpecialPowersHandler.prototype.getOwnPropertyNames = function() {
  return doGetOwnPropertyNames(this.wrappedObject, []);
};

SpecialPowersHandler.prototype.getPropertyNames = function() {

  // Manually walk the prototype chain, making sure to add only property names
  // that haven't been overridden.
  //
  // There's some trickiness here with Xray wrappers. Xray wrappers don't have
  // a prototype, so we need to unwrap them if we want to get all of the names
  // with Object.getOwnPropertyNames(). But we don't really want to unwrap the
  // base object, because that will include expandos that are inaccessible via
  // our implementation of get{,Own}PropertyDescriptor(). So we unwrap just
  // before accessing the prototype. This ensures that we get Xray vision on
  // the base object, and no Xray vision for the rest of the way up.
  var obj = this.wrappedObject;
  var props = [];
  while (obj) {
    props = doGetOwnPropertyNames(obj, props);
    obj = Object.getPrototypeOf(XPCNativeWrapper.unwrap(obj));
  }
  return props;
};

SpecialPowersHandler.prototype.defineProperty = function(name, desc) {
  return Object.defineProperty(this.wrappedObject, name, desc);
};

SpecialPowersHandler.prototype.delete = function(name) {
  return delete this.wrappedObject[name];
};

SpecialPowersHandler.prototype.fix = function() { return undefined; /* Throws a TypeError. */ };

// Per the ES5 spec this is a derived trap, but it's fundamental in spidermonkey
// for some reason. See bug 665198.
SpecialPowersHandler.prototype.enumerate = function() {
  var t = this;
  var filt = function(name) { return t.getPropertyDescriptor(name).enumerable; };
  return this.getPropertyNames().filter(filt);
};

SpecialPowersAPI.prototype = {

  /*
   * Privileged object wrapping API
   *
   * Usage:
   *   var wrapper = SpecialPowers.wrap(obj);
   *   wrapper.privilegedMethod(); wrapper.privilegedProperty;
   *   obj === SpecialPowers.unwrap(wrapper);
   *
   * These functions provide transparent access to privileged objects using
   * various pieces of deep SpiderMagic. Conceptually, a wrapper is just an
   * object containing a reference to the underlying object, where all method
   * calls and property accesses are transparently performed with the System
   * Principal. Moreover, objects obtained from the wrapper (including properties
   * and method return values) are wrapped automatically. Thus, after a single
   * call to SpecialPowers.wrap(), the wrapper layer is transitively maintained.
   *
   * Known Issues:
   *
   *  - The wrapping function does not preserve identity, so
   *    SpecialPowers.wrap(foo) !== SpecialPowers.wrap(foo). See bug 718543.
   *
   *  - The wrapper cannot see expando properties on unprivileged DOM objects.
   *    That is to say, the wrapper uses Xray delegation.
   *
   *  - The wrapper sometimes guesses certain ES5 attributes for returned
   *    properties. This is explained in a comment in the wrapper code above,
   *    and shouldn't be a problem.
   */
  wrap: function(obj) { return isWrapper(obj) ? obj : wrapPrivileged(obj); },
  unwrap: unwrapIfWrapped,
  isWrapper: isWrapper,

  get MockFilePicker() {
    return MockFilePicker
  },

  getDOMWindowUtils: function(aWindow) {
    if (aWindow == this.window.get() && this.DOMWindowUtils != null)
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
   * ex: {'set': [['foo.bar', 2], ['browser.magic', '0xfeedface']], 'clear': [['bad.pref']] }
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
      // The callback needs to be delayed twice. One delay is because the pref
      // service doesn't guarantee the order it calls its observers in, so it
      // may notify the observer holding the callback before the other
      // observers have been notified and given a chance to make the changes
      // that the callback checks for. The second delay is because pref
      // observers often defer making their changes by posting an event to the
      // event loop.
      function delayedCallback() {
        function delayAgain() {
          content.window.setTimeout(callback, 0);
        }
        content.window.setTimeout(delayAgain, 0);
      }
      this._prefEnvUndoStack.push(cleanupActions);
      this._pendingPrefs.push([pendingActions, delayedCallback]);
      this._applyPrefs();
    } else {
      content.window.setTimeout(callback, 0);
    }
  },

  popPrefEnv: function(callback) {
    if (this._prefEnvUndoStack.length > 0) {
      // See pushPrefEnv comment regarding delay.
      function delayedCallback() {
        function delayAgain() {
          content.window.setTimeout(callback, 0);
        }
        content.window.setTimeout(delayAgain, 0);
      }
      let cb = callback ? delayedCallback : null; 
      /* Each pop will have a valid block of preferences */
      this._pendingPrefs.push([this._prefEnvUndoStack.pop(), cb]);
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

    var pb = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
    var self = this;
    pb.addObserver(lastPref.name, function prefObs(subject, topic, data) {
      pb.removeObserver(lastPref.name, prefObs);

      content.window.setTimeout(callback, 0);
      content.window.setTimeout(function () {
        self._applyingPrefs = false;
        // Now apply any prefs that may have been queued while we were applying
        self._applyPrefs();
      }, 0);
    }, false);

    for (var idx in pendingActions) {
      var pref = pendingActions[idx];
      if (pref.action == 'set') {
        this._setPref(pref.name, pref.type, pref.value, pref.Iid);
      } else if (pref.action == 'clear') {
        this.clearUserPref(pref.name);
      }
    }
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
  getFormFillController: function(window) {
    return Components.classes["@mozilla.org/satchel/form-fill-controller;1"]
                     .getService(Components.interfaces.nsIFormFillController);
  },
  attachFormFillControllerTo: function(window) {
    this.getFormFillController()
        .attachToBrowser(this._getDocShell(window),
                         this._getAutoCompletePopup(window));
  },
  detachFormFillControllerFrom: function(window) {
    this.getFormFillController().detachFromBrowser(this._getDocShell(window));
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
        var fileName;
        try {
          fileName = consoleMessage.QueryInterface(Ci.nsIScriptError)
                                   .sourceName;
        } catch (e) {
        }
        this.userListener(consoleMessage.message, fileName);
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
    return this.wrap(Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest));
  },

  snapshotWindow: function (win, withCaret) {
    var el = this.window.get().document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
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

  exactGC: function(win, callback) {
    var self = this;
    let count = 0;

    function doPreciseGCandCC() {
      function scheduledGCCallback() {
        self.getDOMWindowUtils(win).cycleCollect();

        if (++count < 2) {
          doPreciseGCandCC();
        } else {
          callback();
        }
      }

      Components.utils.schedulePreciseGC(scheduledGCCallback);
    }

    doPreciseGCandCC();
  },

  isMainProcess: function() {
    try {
      return Cc["@mozilla.org/xre/app-info;1"].
               getService(Ci.nsIXULRuntime).
               processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
    } catch (e) { }
    return true;
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

  getDOMRequestService: function() {
    var serv = Cc["@mozilla.org/dom/dom-request-service;1"].
      getService(Ci.nsIDOMRequestService);
    var res = { __exposedProps__: {} };
    var props = ["createRequest", "fireError", "fireSuccess"];
    for (i in props) {
      let prop = props[i];
      res[prop] = function() { return serv[prop].apply(serv, arguments) };
      res.__exposedProps__[prop] = "r";
    }
    return res;
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

  copyString: function(str, doc) {
    Components.classes["@mozilla.org/widget/clipboardhelper;1"].
      getService(Components.interfaces.nsIClipboardHelper).
      copyString(str, doc);
  },

  openDialog: function(win, args) {
    return win.openDialog.apply(win, args);
  },

  // :jdm gets credit for this.  ex: getPrivilegedProps(window, 'location.href');
  getPrivilegedProps: function(obj, props) {
    var parts = props.split('.');

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
    xferable.init(this._getDocShell(content.window)
                      .QueryInterface(Components.interfaces.nsILoadContext));
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

  clipboardCopyString: function(preExpectedVal, doc) {
    var cbHelperSvc = Components.classes["@mozilla.org/widget/clipboardhelper;1"].
                      getService(Components.interfaces.nsIClipboardHelper);
    cbHelperSvc.copyString(preExpectedVal, doc);
  },

  supportsSelectionClipboard: function() {
    if (this._cb == null) {
      this._cb = Components.classes["@mozilla.org/widget/clipboard;1"].
                            getService(Components.interfaces.nsIClipboard);
    }
    return this._cb.supportsSelectionClipboard();
  },

  snapshotWindow: function (win, withCaret) {
    var el = this.window.get().document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
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

  get isDebugBuild() {
    delete this.isDebugBuild;
    var debug = Cc["@mozilla.org/xpcom/debug;1"].getService(Ci.nsIDebug2);
    return this.isDebugBuild = debug.isDebugBuild;
  },

  /**
   * Get the message manager associated with an <iframe mozbrowser>.
   */
  getBrowserFrameMessageManager: function(aFrameElement) {
    return this.wrap(aFrameElement.QueryInterface(Ci.nsIFrameLoaderOwner)
                                  .frameLoader
                                  .messageManager);
  },
  
  setFullscreenAllowed: function(document) {
    var pm = Cc["@mozilla.org/permissionmanager;1"].getService(Ci.nsIPermissionManager);
    pm.addFromPrincipal(document.nodePrincipal, "fullscreen", Ci.nsIPermissionManager.ALLOW_ACTION);
    var obsvc = Cc['@mozilla.org/observer-service;1']
                   .getService(Ci.nsIObserverService);
    obsvc.notifyObservers(document, "fullscreen-approved", null);
  },
  
  removeFullscreenAllowed: function(document) {
    var pm = Cc["@mozilla.org/permissionmanager;1"].getService(Ci.nsIPermissionManager);
    pm.removeFromPrincipal(document.nodePrincipal, "fullscreen");
  },

  _getURI: function(urlOrDocument) {
    if (typeof(urlOrDocument) == "string") {
      return Cc["@mozilla.org/network/io-service;1"].
               getService(Ci.nsIIOService).
               newURI(url, null, null);
    }
    // Assume document.
    return this.getDocumentURIObject(urlOrDocument);
  },

  addPermission: function(type, allow, urlOrDocument) {
    let uri = this._getURI(urlOrDocument);

    let permission = allow ?
                     Ci.nsIPermissionManager.ALLOW_ACTION :
                     Ci.nsIPermissionManager.DENY_ACTION;

    var msg = {
      'op': "add",
      'type': type,
      'url': uri.spec,
      'permission': permission
    };

    this._sendSyncMessage('SPPermissionManager', msg);
  },

  removePermission: function(type, urlOrDocument) {
    let uri = this._getURI(urlOrDocument);

    var msg = {
      'op': "remove",
      'type': type,
      'url': uri.spec
    };

    this._sendSyncMessage('SPPermissionManager', msg);
  },

  getMozFullPath: function(file) {
    return file.mozFullPath;
  }
};
