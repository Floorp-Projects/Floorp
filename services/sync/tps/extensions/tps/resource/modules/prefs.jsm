/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* This is a JavaScript module (JSM) to be imported via
   Components.utils.import() and acts as a singleton.
   Only the following listed symbols will exposed on import, and only when
   and where imported. */

var EXPORTED_SYMBOLS = ["Preference"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

const WEAVE_PREF_PREFIX = "services.sync.prefs.sync.";

let prefs = Cc["@mozilla.org/preferences-service;1"]
            .getService(Ci.nsIPrefBranch);

Cu.import("resource://tps/logger.jsm");

/**
 * Preference class constructor
 *
 * Initializes instance properties.
 */
function Preference (props) {
  Logger.AssertTrue("name" in props && "value" in props,
    "Preference must have both name and value");

  this.name = props.name;
  this.value = props.value;
}

/**
 * Preference instance methods
 */
Preference.prototype = {
  /**
   * Modify
   *
   * Sets the value of the preference this.name to this.value.
   * Throws on error.
   *
   * @return nothing
   */
  Modify: function() {
    // Determine if this pref is actually something Weave even looks at.
    let weavepref = WEAVE_PREF_PREFIX + this.name;
    try {
      let syncPref = prefs.getBoolPref(weavepref);
      if (!syncPref)
        prefs.setBoolPref(weavepref, true);
    }
    catch(e) {
      Logger.AssertTrue(false, "Weave doesn't sync pref " + this.name);
    }

    // Modify the pref; throw an exception if the pref type is different
    // than the value type specified in the test.
    let prefType = prefs.getPrefType(this.name);
    switch (prefType) {
      case Ci.nsIPrefBranch.PREF_INT:
        Logger.AssertEqual(typeof(this.value), "number",
          "Wrong type used for preference value");
        prefs.setIntPref(this.name, this.value);
        break;
      case Ci.nsIPrefBranch.PREF_STRING:
        Logger.AssertEqual(typeof(this.value), "string",
          "Wrong type used for preference value");
        prefs.setCharPref(this.name, this.value);
        break;
      case Ci.nsIPrefBranch.PREF_BOOL:
        Logger.AssertEqual(typeof(this.value), "boolean",
          "Wrong type used for preference value");
        prefs.setBoolPref(this.name, this.value);
        break;
    }
  },

  /**
   * Find
   *
   * Verifies that the preference this.name has the value
   * this.value. Throws on error, or if the pref's type or value
   * doesn't match.
   *
   * @return nothing
   */
  Find: function() {
    // Read the pref value.
    let value;
    try {
      let prefType = prefs.getPrefType(this.name);
      switch(prefType) {
        case Ci.nsIPrefBranch.PREF_INT:
          value = prefs.getIntPref(this.name);
          break;
        case Ci.nsIPrefBranch.PREF_STRING:
          value = prefs.getCharPref(this.name);
          break;
        case Ci.nsIPrefBranch.PREF_BOOL:
          value = prefs.getBoolPref(this.name);
          break;
      }
    }
    catch (e) {
      Logger.AssertTrue(false, "Error accessing pref " + this.name);
    }

    // Throw an exception if the current and expected values aren't of
    // the same type, or don't have the same values.
    Logger.AssertEqual(typeof(value), typeof(this.value),
      "Value types don't match");
    Logger.AssertEqual(value, this.value, "Preference values don't match");
  },
};

