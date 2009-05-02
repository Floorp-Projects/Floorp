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
 * The Original Code is Preferences.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Myk Melez <myk@mozilla.org>
 *   Daniel Aquino <mr.danielaquino@gmail.com>
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

let EXPORTED_SYMBOLS = ["Preferences"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

// The minimum and maximum integers that can be set as preferences.
// The range of valid values is narrower than the range of valid JS values
// because the native preferences code treats integers as NSPR PRInt32s,
// which are 32-bit signed integers on all platforms.
const MAX_INT = Math.pow(2, 31) - 1;
const MIN_INT = -MAX_INT;

function Preferences(args) {
    if (isObject(args)) {
      if (args.branch)
        this._prefBranch = args.branch;
      if (args.site)
        this._site = args.site;
    }
    else if (args)
      this._prefBranch = args;
}

Preferences.prototype = {
  /**
   * Get the value of a pref, if any; otherwise return the default value.
   *
   * @param   prefName  {String|Array}
   *          the pref to get, or an array of prefs to get
   *
   * @param   defaultValue
   *          the default value, if any, for prefs that don't have one
   *
   * @returns the value of the pref, if any; otherwise the default value
   */
  get: function(prefName, defaultValue) {
    if (isArray(prefName))
      return prefName.map(function(v) this.get(v, defaultValue), this);

    if (this._site)
      return this._siteGet(prefName, defaultValue);
    else
      return this._get(prefName, defaultValue);
  },

  _get: function(prefName, defaultValue) {
    switch (this._prefSvc.getPrefType(prefName)) {
      case Ci.nsIPrefBranch.PREF_STRING:
        return this._prefSvc.getComplexValue(prefName, Ci.nsISupportsString).data;

      case Ci.nsIPrefBranch.PREF_INT:
        return this._prefSvc.getIntPref(prefName);

      case Ci.nsIPrefBranch.PREF_BOOL:
        return this._prefSvc.getBoolPref(prefName);

      case Ci.nsIPrefBranch.PREF_INVALID:
        return defaultValue;

      default:
        // This should never happen.
        throw "Error getting pref " + prefName + "; its value's type is " +
              this._prefSvc.getPrefType(prefName) + ", which I don't know " +
              "how to handle.";
    }
  },

  _siteGet: function(prefName, defaultValue) {
    let value = this._contentPrefSvc.getPref(this._site, this._prefBranch + prefName);
    return typeof value != "undefined" ? value : defaultValue;
  },

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
  set: function(prefName, prefValue) {
    if (isObject(prefName)) {
      for (let [name, value] in Iterator(prefName))
        this.set(name, value);
      return;
    }

    if (this._site)
      this._siteSet(prefName, prefValue);
    else
      this._set(prefName, prefValue);
  },

  _set: function(prefName, prefValue) {
    let prefType;
    if (typeof prefValue != "undefined" && prefValue != null)
      prefType = prefValue.constructor.name;

    switch (prefType) {
      case "String":
        {
          let string = Cc["@mozilla.org/supports-string;1"].
                       createInstance(Ci.nsISupportsString);
          string.data = prefValue;
          this._prefSvc.setComplexValue(prefName, Ci.nsISupportsString, string);
        }
        break;

      case "Number":
        // We throw if the number is outside the range, since the result
        // will never be what the consumer wanted to store, but we only warn
        // if the number is non-integer, since the consumer might not mind
        // the loss of precision.
        if (prefValue > MAX_INT || prefValue < MIN_INT)
          throw("you cannot set the " + prefName + " pref to the number " +
                prefValue + ", as number pref values must be in the signed " +
                "32-bit integer range -(2^31-1) to 2^31-1.  To store numbers " +
                "outside that range, store them as strings.");
        this._prefSvc.setIntPref(prefName, prefValue);
        if (prefValue % 1 != 0)
          Cu.reportError("Warning: setting the " + prefName + " pref to the " +
                         "non-integer number " + prefValue + " converted it " +
                         "to the integer number " + this.get(prefName) +
                         "; to retain fractional precision, store non-integer " +
                         "numbers as strings.");
        break;

      case "Boolean":
        this._prefSvc.setBoolPref(prefName, prefValue);
        break;

      default:
        throw "can't set pref " + prefName + " to value '" + prefValue +
              "'; it isn't a String, Number, or Boolean";
    }
  },

  _siteSet: function(prefName, prefValue) {
    this._contentPrefSvc.setPref(this._site, this._prefBranch + prefName, prefValue);
  },

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
  has: function(prefName) {
    if (isArray(prefName))
      return prefName.map(this.has, this);

    if (this._site)
      return this._siteHas(prefName);
    else
      return this._has(prefName);
  },

  _has: function(prefName) {
    return (this._prefSvc.getPrefType(prefName) != Ci.nsIPrefBranch.PREF_INVALID);
  },

  _siteHas: function(prefName) {
    return this._contentPrefSvc.hasPref(this._site, this._prefBranch + prefName);
  },

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
  isSet: function(prefName) {
    if (isArray(prefName))
      return prefName.map(this.isSet, this);

    return (this.has(prefName) && this._prefSvc.prefHasUserValue(prefName));
  },

  /**
   * Whether or not the given pref has a user-set value. Use isSet instead,
   * which is equivalent.
   * @deprecated
   */
  modified: function(prefName) { return this.isSet(prefName) },

  reset: function(prefName) {
    if (isArray(prefName)) {
      prefName.map(function(v) this.reset(v), this);
      return;
    }

    if (this._site)
      this._siteReset(prefName);
    else
      this._reset(prefName);
  },
  
  _reset: function(prefName) {
    try {
      this._prefSvc.clearUserPref(prefName);
    }
    catch(ex) {
      // The pref service throws NS_ERROR_UNEXPECTED when the caller tries
      // to reset a pref that doesn't exist or is already set to its default
      // value.  This interface fails silently in those cases, so callers
      // can unconditionally reset a pref without having to check if it needs
      // resetting first or trap exceptions after the fact.  It passes through
      // other exceptions, however, so callers know about them, since we don't
      // know what other exceptions might be thrown and what they might mean.
      if (ex.result != Cr.NS_ERROR_UNEXPECTED)
        throw ex;
    }
  },

  _siteReset: function(prefName) {
    return this._contentPrefSvc.removePref(this._site, this._prefBranch + prefName);
  },

  /**
   * Lock a pref so it can't be changed.
   *
   * @param   prefName  {String|Array}
   *          the pref to lock, or an array of prefs to lock
   */
  lock: function(prefName) {
    if (isArray(prefName))
      prefName.map(this.lock, this);

    this._prefSvc.lockPref(prefName);
  },

  /**
   * Unlock a pref so it can be changed.
   *
   * @param   prefName  {String|Array}
   *          the pref to lock, or an array of prefs to lock
   */
  unlock: function(prefName) {
    if (isArray(prefName))
      prefName.map(this.unlock, this);

    this._prefSvc.unlockPref(prefName);
  },

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
  locked: function(prefName) {
    if (isArray(prefName))
      return prefName.map(this.locked, this);

    return this._prefSvc.prefIsLocked(prefName);
  },

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
  observe: function(prefName, callback, thisObject) {
    let fullPrefName = this._prefBranch + (prefName || "");

    let observer = new PrefObserver(fullPrefName, callback, thisObject);
    Preferences._prefSvc.addObserver(fullPrefName, observer, true);
    observers.push(observer);

    return observer;
  },

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
  ignore: function(prefName, callback, thisObject) {
    let fullPrefName = this._prefBranch + (prefName || "");

    // This seems fairly inefficient, but I'm not sure how much better we can
    // make it.  We could index by fullBranch, but we can't index by callback
    // or thisObject, as far as I know, since the keys to JavaScript hashes
    // (a.k.a. objects) can apparently only be primitive values.
    let [observer] = observers.filter(function(v) v.prefName   == fullPrefName &&
                                                  v.callback   == callback &&
                                                  v.thisObject == thisObject);

    if (observer) {
      Preferences._prefSvc.removeObserver(fullPrefName, observer);
      observers.splice(observers.indexOf(observer), 1);
    }
  },

  resetBranch: function(prefBranch) {
    try {
      this._prefSvc.resetBranch(prefBranch);
    }
    catch(ex) {
      // The current implementation of nsIPrefBranch in Mozilla
      // doesn't implement resetBranch, so we do it ourselves.
      if (ex.result == Cr.NS_ERROR_NOT_IMPLEMENTED)
        this.reset(this._prefSvc.getChildList(prefBranch, []));
      else
        throw ex;
    }
  },

  /**
   * The branch of the preferences tree to which this instance provides access.
   * @private
   */
  _prefBranch: "",

  site: function(site) {
    if (!(site instanceof Ci.nsIURI))
      site = this._ioSvc.newURI("http://" + site, null, null);
    return new Preferences({ branch: this._prefBranch, site: site });
  },

  /**
   * Preferences Service
   * @private
   */
  get _prefSvc() {
    let prefSvc = Cc["@mozilla.org/preferences-service;1"].
                  getService(Ci.nsIPrefService).
                  getBranch(this._prefBranch).
                  QueryInterface(Ci.nsIPrefBranch2);
    this.__defineGetter__("_prefSvc", function() prefSvc);
    return this._prefSvc;
  },

  /**
   * IO Service
   * @private
   */
  get _ioSvc() {
    let ioSvc = Cc["@mozilla.org/network/io-service;1"].
                getService(Ci.nsIIOService);
    this.__defineGetter__("_ioSvc", function() ioSvc);
    return this._ioSvc;
  },

  /**
   * Site Preferences Service
   * @private
   */
  get _contentPrefSvc() {
    let contentPrefSvc = Cc["@mozilla.org/content-pref/service;1"].
                         getService(Ci.nsIContentPrefService);
    this.__defineGetter__("_contentPrefSvc", function() contentPrefSvc);
    return this._contentPrefSvc;
  }

};

// Give the constructor the same prototype as its instances, so users can access
// preferences directly via the constructor without having to create an instance
// first.
Preferences.__proto__ = Preferences.prototype;

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
let observers = [];

function PrefObserver(prefName, callback, thisObject) {
  this.prefName = prefName;
  this.callback = callback;
  this.thisObject = thisObject;
}

PrefObserver.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsISupportsWeakReference]),

  observe: function(subject, topic, data) {
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
    }
    else // typeof this.callback == "object" (nsIObserver)
      this.callback.observe(subject, topic, data);
  }
};

function isArray(val) {
  // We can't check for |val.constructor == Array| here, since the value
  // might be from a different context whose Array constructor is not the same
  // as ours, so instead we match based on the name of the constructor.
  return (typeof val != "undefined" && val != null && typeof val == "object" &&
          val.constructor.name == "Array");
}

function isObject(val) {
  // We can't check for |val.constructor == Object| here, since the value
  // might be from a different context whose Object constructor is not the same
  // as ours, so instead we match based on the name of the constructor.
  return (typeof val != "undefined" && val != null && typeof val == "object" &&
          val.constructor.name == "Object");
}
