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
 * The Original Code is Crossweave.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Griffin <jgriffin@mozilla.com>
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

 /* This is a JavaScript module (JSM) to be imported via 
   Components.utils.import() and acts as a singleton.
   Only the following listed symbols will exposed on import, and only when 
   and where imported. */

var EXPORTED_SYMBOLS = ["Preference"];

const CC = Components.classes;
const CI = Components.interfaces;
const CU = Components.utils;
const WEAVE_PREF_PREFIX = "services.sync.prefs.sync.";

let prefs = CC["@mozilla.org/preferences-service;1"]
           .getService(CI.nsIPrefBranch);

CU.import("resource://tps/logger.jsm");

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
      case CI.nsIPrefBranch.PREF_INT:
        Logger.AssertEqual(typeof(this.value), "number",
          "Wrong type used for preference value");
        prefs.setIntPref(this.name, this.value);
        break;
      case CI.nsIPrefBranch.PREF_STRING:
        Logger.AssertEqual(typeof(this.value), "string",
          "Wrong type used for preference value");
        prefs.setCharPref(this.name, this.value);
        break;
      case CI.nsIPrefBranch.PREF_BOOL:
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
        case CI.nsIPrefBranch.PREF_INT:
          value = prefs.getIntPref(this.name);
          break;
        case CI.nsIPrefBranch.PREF_STRING:
          value = prefs.getCharPref(this.name);
          break;
        case CI.nsIPrefBranch.PREF_BOOL:
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

