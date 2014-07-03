/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cc, Ci, Cu, CC} = require("chrome");
const protocol = require("devtools/server/protocol");
const {Arg, method, RetVal} = protocol;
const {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});

Cu.import("resource://gre/modules/Services.jsm");

exports.register = function(handle) {
  handle.addGlobalActor(PreferenceActor, "preferenceActor");
};

exports.unregister = function(handle) {
};

let PreferenceActor = protocol.ActorClass({
  typeName: "preference",

  getBoolPref: method(function(name) {
    return Services.prefs.getBoolPref(name);
  }, {
    request: { value: Arg(0) },
    response: { value: RetVal("boolean") }
  }),

  getCharPref: method(function(name) {
    return Services.prefs.getCharPref(name);
  }, {
    request: { value: Arg(0) },
    response: { value: RetVal("string") }
  }),

  getIntPref: method(function(name) {
    return Services.prefs.getIntPref(name);
  }, {
    request: { value: Arg(0) },
    response: { value: RetVal("number") }
  }),

  getAllPrefs: method(function() {
    let prefs = {};
    Services.prefs.getChildList("").forEach(function(name, index) {
      // append all key/value pairs into a huge json object.
      try {
        let value;
        switch (Services.prefs.getPrefType(name)) {
          case Ci.nsIPrefBranch.PREF_STRING:
            value = Services.prefs.getCharPref(name);
            break;
          case Ci.nsIPrefBranch.PREF_INT:
            value = Services.prefs.getIntPref(name);
            break;
          case Ci.nsIPrefBranch.PREF_BOOL:
            value = Services.prefs.getBoolPref(name);
            break;
          default:
        }
        prefs[name] = {
          value: value,
          hasUserValue: Services.prefs.prefHasUserValue(name)
        };
      } catch (e) {
        // pref exists but has no user or default value
      }
    });
    return prefs;
  }, {
    request: {},
    response: { value: RetVal("json") }
  }),

  setBoolPref: method(function(name, value) {
    Services.prefs.setBoolPref(name, value);
    Services.prefs.savePrefFile(null);
  }, {
    request: { name: Arg(0), value: Arg(1) },
    response: {}
  }),

  setCharPref: method(function(name, value) {
    Services.prefs.setCharPref(name, value);
    Services.prefs.savePrefFile(null);
  }, {
    request: { name: Arg(0), value: Arg(1) },
    response: {}
  }),

  setIntPref: method(function(name, value) {
    Services.prefs.setIntPref(name, value);
    Services.prefs.savePrefFile(null);
  }, {
    request: { name: Arg(0), value: Arg(1) },
    response: {}
  }),

  clearUserPref: method(function(name) {
    Services.prefs.clearUserPref(name);
    Services.prefs.savePrefFile(null);
  }, {
    request: { name: Arg(0) },
    response: {}
  }),
});

let PreferenceFront = protocol.FrontClass(PreferenceActor, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client);
    this.actorID = form.preferenceActor;
    this.manage(this);
  },
});

const _knownPreferenceFronts = new WeakMap();

exports.getPreferenceFront = function(client, form) {
  if (_knownPreferenceFronts.has(client))
    return _knownPreferenceFronts.get(client);

  let front = new PreferenceFront(client, form);
  _knownPreferenceFronts.set(client, front);
  return front;
};
