/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Large preferences should not be set in the child process.
// Non-string preferences are not tested here, because their behavior
// should not be affected by this filtering.

var Ci = Components.interfaces;
var Cc = Components.classes;

function isParentProcess() {
    let appInfo = Cc["@mozilla.org/xre/app-info;1"];
    return (!appInfo || appInfo.getService(Ci.nsIXULRuntime).processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT);
}

function makeBuffer(length) {
    let string = "x";
    while (string.length < length) {
      string = string + string;
    }
    if (string.length > length) {
      string = string.substring(length - string.length);
    }
    return string;
}

// from prefapi.h
const MAX_ADVISABLE_PREF_LENGTH = 4 * 1024;

const largeString = makeBuffer(MAX_ADVISABLE_PREF_LENGTH + 1);
const smallString = makeBuffer(4);

const testValues = [
  {name: "None", value: undefined},
  {name: "Small", value: smallString},
  {name: "Large", value: largeString},
];

function prefName(def, user) {
  return "Test.IPC.default" + def.name + "User" + user.name;
}

function expectedPrefValue(def, user) {
  if (user.value) {
    return user.value;
  }
  return def.value;
}

function run_test() {
  let pb = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
  let ps = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefService);
  let defaultBranch = ps.getDefaultBranch("");

  let isParent = isParentProcess();
  if (isParent) {
    // Set all combinations of none, small and large, for default and user prefs.
    for (let def of testValues) {
      for (let user of testValues) {
        let currPref = prefName(def, user);
        if (def.value) {
          defaultBranch.setCharPref(currPref, def.value);
        }
        if (user.value) {
          pb.setCharPref(currPref, user.value);
        }
      }
    }

    run_test_in_child("test_large_pref.js");
  }

  // Check that each preference is set or not set, as appropriate.
  for (let def of testValues) {
    for (let user of testValues) {
      if (!def.value && !user.value) {
        continue;
      }
      let pref_name = prefName(def, user);
      if (isParent || (def.name != "Large" && user.name != "Large")) {
        do_check_eq(pb.getCharPref(pref_name), expectedPrefValue(def, user));
      } else {
        // This is the child, and either the default or user value is
        // large, so the preference should not be set.
        let prefExists;
        try {
          pb.getCharPref(pref_name);
          prefExists = true;
        } catch(e) {
          prefExists = false;
        }
        ok(!prefExists,
           "Pref " + pref_name + " should not be set in the child");
      }
    }
  }
}
