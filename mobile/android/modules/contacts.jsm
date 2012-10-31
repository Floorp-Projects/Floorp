/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let EXPORTED_SYMBOLS = ["Contacts"];

const Cu = Components.utils;

let Contacts = {
  _providers: [],
  _contacts: [],

  _load: function _load() {
    this._contacts = [];

    this._providers.forEach(function(provider) {
      this._contacts = this._contacts.concat(provider.getContacts());
    }, this)
  },

  init: function init() {
    // Not much to do for now
    this._load();
  },

  refresh: function refresh() {
    // Pretty simple for now
    this._load();
  },

  addProvider: function(aProvider) {
    this._providers.push(aProvider);
    this.refresh();
  },

  find: function find(aMatch) {
    let results = [];

    if (!this._contacts.length)
      return results;

    for (let field in aMatch) {
      // Simple string-only partial matching
      let match = aMatch[field];
      this._contacts.forEach(function(aContact) {
        if (field in aContact && aContact[field].indexOf(match) != -1)
          results.push(aContact);
      });
    }
    return results;
  }
};
