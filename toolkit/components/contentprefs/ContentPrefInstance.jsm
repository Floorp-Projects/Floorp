/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Cc = Components.classes;
const Ci = Components.interfaces;

this.EXPORTED_SYMBOLS = ['ContentPrefInstance'];

// This is a wrapper for nsIContentPrefService that alleviates the need to pass
// an nsILoadContext argument to every method. Pass the context to the constructor
// instead and continue on your way in blissful ignorance.

this.ContentPrefInstance = function ContentPrefInstance(aContext) {
  this._contentPrefSvc = Cc["@mozilla.org/content-pref/service;1"].
                           getService(Ci.nsIContentPrefService);
  this._context = aContext;
};

ContentPrefInstance.prototype = {
  getPref: function ContentPrefInstance_init(aName, aGroup, aCallback) {
    return this._contentPrefSvc.getPref(aName, aGroup, this._context, aCallback);
  },

  setPref: function ContentPrefInstance_setPref(aGroup, aName, aValue) {
    return this._contentPrefSvc.setPref(aGroup, aName, aValue, this._context);
  },

  hasPref: function ContentPrefInstance_hasPref(aGroup, aName) {
    return this._contentPrefSvc.hasPref(aGroup, aName, this._context);
  },

  hasCachedPref: function ContentPrefInstance_hasCachedPref(aGroup, aName) {
    return this._contentPrefSvc.hasCachedPref(aGroup, aName, this._context);
  },

  removePref: function ContentPrefInstance_removePref(aGroup, aName) {
    return this._contentPrefSvc.removePref(aGroup, aName, this._context);
  },

  removeGroupedPrefs: function ContentPrefInstance_removeGroupedPrefs() {
    return this._contentPrefSvc.removeGroupedPrefs(this._context);
  },

  removePrefsByName: function ContentPrefInstance_removePrefsByName(aName) {
    return this._contentPrefSvc.removePrefsByName(aName, this._context);
  },

  getPrefs: function ContentPrefInstance_getPrefs(aGroup) {
    return this._contentPrefSvc.getPrefs(aGroup, this._context);
  },

  getPrefsByName: function ContentPrefInstance_getPrefsByName(aName) {
    return this._contentPrefSvc.getPrefsByName(aName, this._context);
  },

  addObserver: function ContentPrefInstance_addObserver(aName, aObserver) {
    return this._contentPrefSvc.addObserver(aName, aObserver);
  },

  removeObserver: function ContentPrefInstance_removeObserver(aName, aObserver) {
    return this._contentPrefSvc.removeObserver(aName, aObserver);
  },

  get grouper() {
    return this._contentPrefSvc.grouper;
  },

  get DBConnection() {
    return this._contentPrefSvc.DBConnection;
  }
};
