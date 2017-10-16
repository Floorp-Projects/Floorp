// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["SharedPreferences"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

// For adding observers.
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Messaging.jsm");

var Scope = Object.freeze({
  APP:          "app",
  PROFILE:      "profile",
  GLOBAL:       "global"
});

/**
 * Public API to getting a SharedPreferencesImpl instance. These scopes mirror GeckoSharedPrefs.
 */
var SharedPreferences = {
  forApp: function() {
    return new SharedPreferencesImpl({ scope: Scope.APP });
  },

  forProfile: function() {
    return new SharedPreferencesImpl({ scope: Scope.PROFILE });
  },

  /**
   * Get SharedPreferences for the named profile; if the profile name is null,
   * returns the preferences for the current profile (just like |forProfile|).
   */
  forProfileName: function(profileName) {
    return new SharedPreferencesImpl({ scope: Scope.PROFILE, profileName: profileName });
  },

  /**
   * Get SharedPreferences for the given Android branch; if the branch is null,
   * returns the default preferences branch for the application, which is the
   * output of |PreferenceManager.getDefaultSharedPreferences|.
   */
  forAndroid: function(branch) {
    return new SharedPreferencesImpl({ scope: Scope.GLOBAL, branch: branch });
  }
};

/**
 * Create an interface to an Android SharedPreferences branch.
 *
 * options {Object} with the following valid keys:
 *   - scope {String} (required) specifies the scope of preferences that should be accessed.
 *   - branch {String} (only when using Scope.GLOBAL) should be a string describing a preferences branch,
 *     like "UpdateService" or "background.data", or null to access the
 *     default preferences branch for the application.
 *   - profileName {String} (optional, only valid when using Scope.PROFILE)
 */
function SharedPreferencesImpl(options = {}) {
  if (!(this instanceof SharedPreferencesImpl)) {
    return new SharedPreferencesImpl(options);
  }

  if (options.scope == null || options.scope == undefined) {
    throw "Shared Preferences must specifiy a scope.";
  }

  this._scope = options.scope;
  this._profileName = options.profileName;
  this._branch = options.branch;
  this._observers = {};
}

SharedPreferencesImpl.prototype = Object.freeze({
  _set: function _set(prefs) {
    EventDispatcher.instance.sendRequest({
      type: "SharedPreferences:Set",
      preferences: prefs,
      scope: this._scope,
      profileName: this._profileName,
      branch: this._branch,
    });
  },

  _setOne: function _setOne(prefName, value, type) {
    let prefs = [];
    prefs.push({
      name: prefName,
      value: value,
      type: type,
    });
    this._set(prefs);
  },

  setBoolPref: function setBoolPref(prefName, value) {
    this._setOne(prefName, value, "bool");
  },

  setCharPref: function setCharPref(prefName, value) {
    this._setOne(prefName, value, "string");
  },

  setSetPref: function setCharPref(prefName, value) {
    this._setOne(prefName, value, "set");
  },

  setIntPref: function setIntPref(prefName, value) {
    this._setOne(prefName, value, "int");
  },

  _get: function _get(prefs, callback) {
    let result = null;

    // Use dispatch instead of sendRequestForResult because callbacks for
    // Gecko thread events are synchronous when used with dispatch(), so we
    // don't have to spin the event loop here to wait for a result.
    EventDispatcher.instance.dispatch("SharedPreferences:Get", {
      preferences: prefs,
      scope: this._scope,
      profileName: this._profileName,
      branch: this._branch,
    }, {
      onSuccess: values => { result = values; },
      onError: msg => { throw new Error("Cannot get preference: " + msg); },
    });

    return result;
  },

  _getOne: function _getOne(prefName, type) {
    let prefs = [{
      name: prefName,
      type: type,
    }];
    let values = this._get(prefs);
    if (values.length != 1) {
      throw new Error("Got too many values: " + values.length);
    }
    return values[0].value;
  },

  getBoolPref: function getBoolPref(prefName) {
    return this._getOne(prefName, "bool");
  },

  getCharPref: function getCharPref(prefName) {
    return this._getOne(prefName, "string");
  },

  getSetPref: function getSetPref(prefName) {
    return this._getOne(prefName, "set");
  },

  getIntPref: function getIntPref(prefName) {
    return this._getOne(prefName, "int");
  },

  /**
   * Invoke `observer` after a change to the preference `domain` in
   * the current branch.
   *
   * `observer` should implement the nsIObserver.observe interface.
   */
  addObserver: function addObserver(domain, observer, holdWeak) {
    if (!domain)
      throw new Error("domain must not be null");
    if (!observer)
      throw new Error("observer must not be null");
    if (holdWeak)
      throw new Error("Weak references not yet implemented.");

    if (!this._observers.hasOwnProperty(domain))
      this._observers[domain] = [];
    if (this._observers[domain].indexOf(observer) > -1)
      return;

    this._observers[domain].push(observer);

    this._updateAndroidListener();
  },

  /**
   * Do not invoke `observer` after a change to the preference
   * `domain` in the current branch.
   */
  removeObserver: function removeObserver(domain, observer) {
    if (!this._observers.hasOwnProperty(domain))
      return;
    let index = this._observers[domain].indexOf(observer);
    if (index < 0)
      return;

    this._observers[domain].splice(index, 1);
    if (this._observers[domain].length < 1)
      delete this._observers[domain];

    this._updateAndroidListener();
  },

  _updateAndroidListener: function _updateAndroidListener() {
    if (this._listening && Object.keys(this._observers).length < 1)
      this._uninstallAndroidListener();
    if (!this._listening && Object.keys(this._observers).length > 0)
      this._installAndroidListener();
  },

  _installAndroidListener: function _installAndroidListener() {
    if (this._listening)
      return;
    this._listening = true;

    EventDispatcher.instance.registerListener(this, "SharedPreferences:Changed");

    EventDispatcher.instance.sendRequest({
      type: "SharedPreferences:Observe",
      enable: true,
      scope: this._scope,
      profileName: this._profileName,
      branch: this._branch,
    });
  },

  onEvent: function _onEvent(event, msg, callback) {
    if (event !== "SharedPreferences:Changed") {
      return;
    }

    if (msg.scope !== this._scope ||
        ((this._scope === Scope.PROFILE) && (msg.profileName !== this._profileName)) ||
        ((this._scope === Scope.GLOBAL) && (msg.branch !== this._branch))) {
      return;
    }

    if (!this._observers.hasOwnProperty(msg.key)) {
      return;
    }

    let observers = this._observers[msg.key];
    for (let obs of observers) {
      obs.observe(obs, msg.key, msg.value);
    }
  },

  _uninstallAndroidListener: function _uninstallAndroidListener() {
    if (!this._listening)
      return;
    this._listening = false;

    EventDispatcher.instance.unregisterListener(this, "SharedPreferences:Changed");

    EventDispatcher.instance.sendRequest({
      type: "SharedPreferences:Observe",
      enable: false,
      scope: this._scope,
      profileName: this._profileName,
      branch: this._branch,
    });
  },
});
