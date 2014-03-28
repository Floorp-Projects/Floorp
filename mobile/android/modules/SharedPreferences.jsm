// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["SharedPreferences"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

// For adding observers.
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Messaging.jsm");

/**
 * Create an interface to an Android SharedPreferences branch.
 *
 * branch {String} should be a string describing a preferences branch,
 * like "UpdateService" or "background.data", or null to access the
 * default preferences branch for the application.
 */
function SharedPreferences(branch) {
  if (!(this instanceof SharedPreferences)) {
    return new SharedPreferences(branch);
  }
  this._branch = branch || null;
  this._observers = {};
};

SharedPreferences.prototype = Object.freeze({
  _set: function _set(prefs) {
    sendMessageToJava({
      type: "SharedPreferences:Set",
      preferences: prefs,
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

  setIntPref: function setIntPref(prefName, value) {
    this._setOne(prefName, value, "int");
  },

  _get: function _get(prefs, callback) {
    let result = null;
    sendMessageToJava({
      type: "SharedPreferences:Get",
      preferences: prefs,
      branch: this._branch,
    }, (data) => {
      result = data.values;
    });

    let thread = Services.tm.currentThread;
    while (result == null)
      thread.processNextEvent(true);

    return result;
  },

  _getOne: function _getOne(prefName, type) {
    let prefs = [];
    prefs.push({
      name: prefName,
      type: type,
    });
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

    Services.obs.addObserver(this, "SharedPreferences:Changed", false);
    sendMessageToJava({
      type: "SharedPreferences:Observe",
      enable: true,
      branch: this._branch,
    });
  },

  observe: function observe(subject, topic, data) {
    if (topic != "SharedPreferences:Changed") {
      return;
    }

    let msg = JSON.parse(data);
    if (msg.branch != this._branch) {
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

    Services.obs.removeObserver(this, "SharedPreferences:Changed");
    sendMessageToJava({
      type: "SharedPreferences:Observe",
      enable: false,
      branch: this._branch,
    });
  },
});
