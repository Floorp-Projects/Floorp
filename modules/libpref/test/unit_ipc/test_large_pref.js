/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Large preferences should not be set in the child process.
// Non-string preferences are not tested here, because their behavior
// should not be affected by this filtering.
//
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

function isParentProcess() {
  return Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
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
  { name: "None", value: undefined },
  { name: "Small", value: smallString },
  { name: "Large", value: largeString },
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
  const pb = Services.prefs;
  let defaultBranch = pb.getDefaultBranch("");

  let isParent = isParentProcess();
  if (isParent) {
    // Preferences with large values will still appear in the shared memory
    // snapshot that we share with all processes. They should not, however, be
    // sent with the list of changes on top of the snapshot.
    //
    // So, make sure we've generated the initial snapshot before we set the
    // preference values by launching a child process with an empty test.
    sendCommand("");

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
        Assert.equal(pb.getCharPref(pref_name), expectedPrefValue(def, user));
      } else {
        // This is the child, and either the default or user value is
        // large, so the preference should not be set.
        let prefExists;
        try {
          let val = pb.getCharPref(pref_name);
          prefExists = val.length > 128;
        } catch (e) {
          prefExists = false;
        }
        ok(
          !prefExists,
          "Pref " + pref_name + " should not be set in the child"
        );
      }
    }
  }
}
