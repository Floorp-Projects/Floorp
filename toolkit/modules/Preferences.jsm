/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["Preferences"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// The minimum and maximum integers that can be set as preferences.
// The range of valid values is narrower than the range of valid JS values
// because the native preferences code treats integers as NSPR PRInt32s,
// which are 32-bit signed integers on all platforms.
const MAX_INT = 0x7FFFFFFF; // Math.pow(2, 31) - 1
const MIN_INT = -0x80000000;

this.Preferences =
  function Preferences(args) {
    this._cachedPrefBranch = null;
    if (isObject(args)) {
      if (args.branch)
        this._branchStr = args.branch;
      if (args.defaultBranch)
        this._defaultBranch = args.defaultBranch;
      if (args.privacyContext)
        this._privacyContext = args.privacyContext;
    } else if (args)
      this._branchStr = args;
  };

/**
 * Get the value of a pref, if any; otherwise return the default value.
 *
 * @param   prefName  {String|Array}
 *          the pref to get, or an array of prefs to get
 *
 * @param   defaultValue
 *          the default value, if any, for prefs that don't have one
 *
 * @param   valueType
 *          the XPCOM interface of the pref's complex value type, if any
 *
 * @returns the value of the pref, if any; otherwise the default value
 */
Preferences.get = function(prefName, defaultValue, valueType = Ci.nsISupportsString) {
  if (Array.isArray(prefName))
    return prefName.map(v => this.get(v, defaultValue));

  return this._get(prefName, defaultValue, valueType);
};

Preferences._get = function(prefName, defaultValue, valueType) {
  switch (this._prefBranch.getPrefType(prefName)) {
    case Ci.nsIPrefBranch.PREF_STRING:
      return this._prefBranch.getComplexValue(prefName, valueType).data;

    case Ci.nsIPrefBranch.PREF_INT:
      return this._prefBranch.getIntPref(prefName);

    case Ci.nsIPrefBranch.PREF_BOOL:
      return this._prefBranch.getBoolPref(prefName);

    case Ci.nsIPrefBranch.PREF_INVALID:
      return defaultValue;

    default:
      // This should never happen.
      throw new Error(`Error getting pref ${prefName}; its value's type is ` +
                      `${this._prefBranch.getPrefType(prefName)}, which I don't ` +
                      `know how to handle.`);
  }
};

/**
 * Set a preference to a value.
 *
 * You can set multiple prefs by passing an object as the only parameter.
 * In that case, this method will treat the properties of the object
 * as preferences to set, where each property name is the name of a pref
 * and its corresponding property value is the value of the pref.
 *
 * @param   prefName  {String|Object}
 *          the name of the pref to set; or an object containing a set
 *          of prefs to set
 *
 * @param   prefValue {String|Number|Boolean}
 *          the value to which to set the pref
 *
 * Note: Preferences cannot store non-integer numbers or numbers outside
 * the signed 32-bit range -(2^31-1) to 2^31-1, If you have such a number,
 * store it as a string by calling toString() on the number before passing
 * it to this method, i.e.:
 *   Preferences.set("pi", 3.14159.toString())
 *   Preferences.set("big", Math.pow(2, 31).toString()).
 */
Preferences.set = function(prefName, prefValue) {
  if (isObject(prefName)) {
    for (let [name, value] of Object.entries(prefName))
      this.set(name, value);
    return;
  }

  this._set(prefName, prefValue);
};

Preferences._set = function(prefName, prefValue) {
  let prefType;
  if (typeof prefValue != "undefined" && prefValue != null)
    prefType = prefValue.constructor.name;

  switch (prefType) {
    case "String":
      this._prefBranch.setStringPref(prefName, prefValue);
      break;

    case "Number":
      // We throw if the number is outside the range, since the result
      // will never be what the consumer wanted to store, but we only warn
      // if the number is non-integer, since the consumer might not mind
      // the loss of precision.
      if (prefValue > MAX_INT || prefValue < MIN_INT)
        throw new Error(
              `you cannot set the ${prefName} pref to the number ` +
              `${prefValue}, as number pref values must be in the signed ` +
              `32-bit integer range -(2^31-1) to 2^31-1.  To store numbers ` +
              `outside that range, store them as strings.`);
      this._prefBranch.setIntPref(prefName, prefValue);
      if (prefValue % 1 != 0)
        Cu.reportError("Warning: setting the " + prefName + " pref to the " +
                       "non-integer number " + prefValue + " converted it " +
                       "to the integer number " + this.get(prefName) +
                       "; to retain fractional precision, store non-integer " +
                       "numbers as strings.");
      break;

    case "Boolean":
      this._prefBranch.setBoolPref(prefName, prefValue);
      break;

    default:
      throw new Error(`can't set pref ${prefName} to value '${prefValue}'; ` +
                      `it isn't a String, Number, or Boolean`);
  }
};

/**
 * Whether or not the given pref has a value.  This is different from isSet
 * because it returns true whether the value of the pref is a default value
 * or a user-set value, while isSet only returns true if the value
 * is a user-set value.
 *
 * @param   prefName  {String|Array}
 *          the pref to check, or an array of prefs to check
 *
 * @returns {Boolean|Array}
 *          whether or not the pref has a value; or, if the caller provided
 *          an array of pref names, an array of booleans indicating whether
 *          or not the prefs have values
 */
Preferences.has = function(prefName) {
  if (Array.isArray(prefName))
    return prefName.map(this.has, this);

  return (this._prefBranch.getPrefType(prefName) != Ci.nsIPrefBranch.PREF_INVALID);
};

/**
 * Whether or not the given pref has a user-set value.  This is different
 * from |has| because it returns true only if the value of the pref is a user-
 * set value, while |has| returns true if the value of the pref is a default
 * value or a user-set value.
 *
 * @param   prefName  {String|Array}
 *          the pref to check, or an array of prefs to check
 *
 * @returns {Boolean|Array}
 *          whether or not the pref has a user-set value; or, if the caller
 *          provided an array of pref names, an array of booleans indicating
 *          whether or not the prefs have user-set values
 */
Preferences.isSet = function(prefName) {
  if (Array.isArray(prefName))
    return prefName.map(this.isSet, this);

  return (this.has(prefName) && this._prefBranch.prefHasUserValue(prefName));
},

/**
 * Whether or not the given pref has a user-set value. Use isSet instead,
 * which is equivalent.
 * @deprecated
 */
Preferences.modified = function(prefName) { return this.isSet(prefName); },

Preferences.reset = function(prefName) {
  if (Array.isArray(prefName)) {
    prefName.map(v => this.reset(v));
    return;
  }

  this._prefBranch.clearUserPref(prefName);
};

/**
 * Lock a pref so it can't be changed.
 *
 * @param   prefName  {String|Array}
 *          the pref to lock, or an array of prefs to lock
 */
Preferences.lock = function(prefName) {
  if (Array.isArray(prefName))
    prefName.map(this.lock, this);

  this._prefBranch.lockPref(prefName);
};

/**
 * Unlock a pref so it can be changed.
 *
 * @param   prefName  {String|Array}
 *          the pref to lock, or an array of prefs to lock
 */
Preferences.unlock = function(prefName) {
  if (Array.isArray(prefName))
    prefName.map(this.unlock, this);

  this._prefBranch.unlockPref(prefName);
};

/**
 * Whether or not the given pref is locked against changes.
 *
 * @param   prefName  {String|Array}
 *          the pref to check, or an array of prefs to check
 *
 * @returns {Boolean|Array}
 *          whether or not the pref has a user-set value; or, if the caller
 *          provided an array of pref names, an array of booleans indicating
 *          whether or not the prefs have user-set values
 */
Preferences.locked = function(prefName) {
  if (Array.isArray(prefName))
    return prefName.map(this.locked, this);

  return this._prefBranch.prefIsLocked(prefName);
};

/**
 * Start observing a pref.
 *
 * The callback can be a function or any object that implements nsIObserver.
 * When the callback is a function and thisObject is provided, it gets called
 * as a method of thisObject.
 *
 * @param   prefName    {String}
 *          the name of the pref to observe
 *
 * @param   callback    {Function|Object}
 *          the code to notify when the pref changes;
 *
 * @param   thisObject  {Object}  [optional]
 *          the object to use as |this| when calling a Function callback;
 *
 * @returns the wrapped observer
 */
Preferences.observe = function(prefName, callback, thisObject) {
  let fullPrefName = this._branchStr + (prefName || "");

  let observer = new PrefObserver(fullPrefName, callback, thisObject);
  Preferences._prefBranch.addObserver(fullPrefName, observer, true);
  observers.push(observer);

  return observer;
};

/**
 * Stop observing a pref.
 *
 * You must call this method with the same prefName, callback, and thisObject
 * with which you originally registered the observer.  However, you don't have
 * to call this method on the same exact instance of Preferences; you can call
 * it on any instance.  For example, the following code first starts and then
 * stops observing the "foo.bar.baz" preference:
 *
 *   let observer = function() {...};
 *   Preferences.observe("foo.bar.baz", observer);
 *   new Preferences("foo.bar.").ignore("baz", observer);
 *
 * @param   prefName    {String}
 *          the name of the pref being observed
 *
 * @param   callback    {Function|Object}
 *          the code being notified when the pref changes
 *
 * @param   thisObject  {Object}  [optional]
 *          the object being used as |this| when calling a Function callback
 */
Preferences.ignore = function(prefName, callback, thisObject) {
  let fullPrefName = this._branchStr + (prefName || "");

  // This seems fairly inefficient, but I'm not sure how much better we can
  // make it.  We could index by fullBranch, but we can't index by callback
  // or thisObject, as far as I know, since the keys to JavaScript hashes
  // (a.k.a. objects) can apparently only be primitive values.
  let [observer] = observers.filter(v => v.prefName == fullPrefName &&
                                         v.callback == callback &&
                                         v.thisObject == thisObject);

  if (observer) {
    Preferences._prefBranch.removeObserver(fullPrefName, observer);
    observers.splice(observers.indexOf(observer), 1);
  } else {
    Cu.reportError(`Attempt to stop observing a preference "${prefName}" that's not being observed`);
  }
};

Preferences.resetBranch = function(prefBranch = "") {
  try {
    this._prefBranch.resetBranch(prefBranch);
  } catch (ex) {
    // The current implementation of nsIPrefBranch in Mozilla
    // doesn't implement resetBranch, so we do it ourselves.
    if (ex.result == Cr.NS_ERROR_NOT_IMPLEMENTED)
      this.reset(this._prefBranch.getChildList(prefBranch, []));
    else
      throw ex;
  }
},

/**
 * A string identifying the branch of the preferences tree to which this
 * instance provides access.
 * @private
 */
Preferences._branchStr = "";

/**
 * The cached preferences branch object this instance encapsulates, or null.
 * Do not use!  Use _prefBranch below instead.
 * @private
 */
Preferences._cachedPrefBranch = null;

/**
 * The preferences branch object for this instance.
 * @private
 */
Object.defineProperty(Preferences, "_prefBranch",
{
  get: function _prefBranch() {
    if (!this._cachedPrefBranch) {
      let prefSvc = Services.prefs;
      this._cachedPrefBranch = this._defaultBranch ?
                               prefSvc.getDefaultBranch(this._branchStr) :
                               prefSvc.getBranch(this._branchStr);
    }
    return this._cachedPrefBranch;
  },
  enumerable: true,
  configurable: true
});

// Constructor-based access (Preferences.get(...) and set) is preferred over
// instance-based access (new Preferences().get(...) and set) and when using the
// root preferences branch, as it's desirable not to allocate the extra object.
// But both forms are acceptable.
Preferences.prototype = Preferences;

/**
 * A cache of pref observers.
 *
 * We use this to remove observers when a caller calls Preferences::ignore.
 *
 * All Preferences instances share this object, because we want callers to be
 * able to remove an observer using a different Preferences object than the one
 * with which they added it.  That means we have to identify the observers
 * in this object by their complete pref name, not just their name relative to
 * the root branch of the Preferences object with which they were created.
 */
var observers = [];

function PrefObserver(prefName, callback, thisObject) {
  this.prefName = prefName;
  this.callback = callback;
  this.thisObject = thisObject;
}

PrefObserver.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsISupportsWeakReference]),

  observe(subject, topic, data) {
    // The pref service only observes whole branches, but we only observe
    // individual preferences, so we check here that the pref that changed
    // is the exact one we're observing (and not some sub-pref on the branch).
    if (data != this.prefName)
      return;

    if (typeof this.callback == "function") {
      let prefValue = Preferences.get(this.prefName);

      if (this.thisObject)
        this.callback.call(this.thisObject, prefValue);
      else
        this.callback(prefValue);
    } else // typeof this.callback == "object" (nsIObserver)
      this.callback.observe(subject, topic, data);
  }
};

function isObject(val) {
  // We can't check for |val.constructor == Object| here, since the value
  // might be from a different context whose Object constructor is not the same
  // as ours, so instead we match based on the name of the constructor.
  return (typeof val != "undefined" && val != null && typeof val == "object" &&
          val.constructor.name == "Object");
}
