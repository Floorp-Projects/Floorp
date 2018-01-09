/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint
  "no-unused-vars": ["error", {
    vars: "local",
    args: "none",
    varsIgnorePattern: "^(Cc|Ci|Cr|Cu|EXPORTED_SYMBOLS)$",
  }],
*/

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;
const CURRENT_SCHEMA = 4;
const PR_HOURS = 60 * 60 * 1000000;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FormHistory.jsm");

do_get_profile();

// Send the profile-after-change notification to the form history component to ensure
// that it has been initialized.
var formHistoryStartup = Cc["@mozilla.org/satchel/form-history-startup;1"]
                         .getService(Ci.nsIObserver);
formHistoryStartup.observe(null, "profile-after-change", null);

function getDBVersion(dbfile) {
  let dbConnection = Services.storage.openDatabase(dbfile);
  let version = dbConnection.schemaVersion;
  dbConnection.close();

  return version;
}

const isGUID = /[A-Za-z0-9\+\/]{16}/;

// Find form history entries.
function searchEntries(terms, params, iter) {
  let results = [];
  FormHistory.search(terms, params, {
    handleResult: result => results.push(result),
    handleError(error) {
      do_throw("Error occurred searching form history: " + error);
    },
    handleCompletion(reason) {
      if (!reason) {
        iter.next(results);
      }
    },
  });
}

// Count the number of entries with the given name and value, and call then(number)
// when done. If name or value is null, then the value of that field does not matter.
function countEntries(name, value, then) {
  let obj = {};
  if (name !== null) {
    obj.fieldname = name;
  }
  if (value !== null) {
    obj.value = value;
  }

  let count = 0;
  FormHistory.count(obj, {
    handleResult: result => count = result,
    handleError(error) {
      do_throw("Error occurred searching form history: " + error);
    },
    handleCompletion(reason) {
      if (!reason) {
        then(count);
      }
    },
  });
}

// Perform a single form history update and call then() when done.
function updateEntry(op, name, value, then) {
  let obj = { op };
  if (name !== null) {
    obj.fieldname = name;
  }
  if (value !== null) {
    obj.value = value;
  }
  updateFormHistory(obj, then);
}

// Add a single form history entry with the current time and call then() when done.
function addEntry(name, value, then) {
  let now = Date.now() * 1000;
  updateFormHistory({
    op: "add",
    fieldname: name,
    value,
    timesUsed: 1,
    firstUsed: now,
    lastUsed: now,
  }, then);
}

function promiseCountEntries(name, value) {
  return new Promise(res => {
    countEntries(name, value, res);
  });
}

function promiseUpdateEntry(op, name, value) {
  return new Promise(res => {
    updateEntry(op, name, value, res);
  });
}

function promiseAddEntry(name, value) {
  return new Promise(res => {
    addEntry(name, value, res);
  });
}

// Wrapper around FormHistory.update which handles errors. Calls then() when done.
function updateFormHistory(changes, then) {
  FormHistory.update(changes, {
    handleError(error) {
      do_throw("Error occurred updating form history: " + error);
    },
    handleCompletion(reason) {
      if (!reason) {
        then();
      }
    },
  });
}

/**
 * Logs info to the console in the standard way (includes the filename).
 *
 * @param {string} aMessage
 *        The message to log to the console.
 */
function do_log_info(aMessage) {
  print("TEST-INFO | " + _TEST_FILE + " | " + aMessage);
}
