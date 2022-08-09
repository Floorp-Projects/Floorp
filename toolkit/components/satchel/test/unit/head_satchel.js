/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint
  "no-unused-vars": ["error", {
    vars: "local",
    args: "none",
  }],
*/

const CURRENT_SCHEMA = 5;
const PR_HOURS = 60 * 60 * 1000000;

var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  FormHistory: "resource://gre/modules/FormHistory.jsm",
  FormHistoryTestUtils: "resource://testing-common/FormHistoryTestUtils.jsm",
  Sqlite: "resource://gre/modules/Sqlite.jsm",
});

do_get_profile();

// Send the profile-after-change notification to the form history component to ensure
// that it has been initialized.
var formHistoryStartup = Cc[
  "@mozilla.org/satchel/form-history-startup;1"
].getService(Ci.nsIObserver);
formHistoryStartup.observe(null, "profile-after-change", null);

function getDBVersion(dbfile) {
  let dbConnection = Services.storage.openDatabase(dbfile);
  let version = dbConnection.schemaVersion;
  dbConnection.close();

  return version;
}

async function getDBSchemaVersion(path) {
  let db = await Sqlite.openConnection({ path });
  try {
    return await db.getSchemaVersion();
  } finally {
    await db.close();
  }
}

function getFormHistoryDBVersion() {
  let profileDir = do_get_profile();
  // Cleanup from any previous tests or failures.
  let dbFile = profileDir.clone();
  dbFile.append("formhistory.sqlite");
  return getDBVersion(dbFile);
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
    handleResult: result => (count = result),
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
  updateFormHistory(
    {
      op: "add",
      fieldname: name,
      value,
      timesUsed: 1,
      firstUsed: now,
      lastUsed: now,
    },
    then
  );
}

function promiseCountEntries(name, value, checkFn = () => {}) {
  return new Promise(resolve => {
    countEntries(name, value, function(result) {
      checkFn(result);
      resolve(result);
    });
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

function promiseUpdate(change) {
  return new Promise((resolve, reject) => {
    FormHistory.update(change, {
      handleError(error) {
        this._error = error;
      },
      handleCompletion(reason) {
        if (reason) {
          reject(this._error);
        } else {
          resolve();
        }
      },
    });
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

/**
 * Copies a test file into the profile folder.
 *
 * @param {string} aFilename
 *        The name of the file to copy.
 * @param {string} aDestFilename
 *        The name of the file to copy.
 * @param {Object} [options.overwriteExisting]
 *        Whether to overwrite an existing file.
 * @returns {string} path to the copied file.
 */
async function copyToProfile(
  aFilename,
  aDestFilename,
  { overwriteExisting = false } = {}
) {
  let curDir = Services.dirsvc.get("CurWorkD", Ci.nsIFile).path;
  let srcPath = PathUtils.join(curDir, aFilename);
  Assert.ok(await IOUtils.exists(srcPath), "Database file found");

  // Ensure that our file doesn't exist already.
  let destPath = PathUtils.join(PathUtils.profileDir, aDestFilename);
  let exists = await IOUtils.exists(destPath);
  if (exists) {
    if (overwriteExisting) {
      await IOUtils.remove(destPath);
    } else {
      throw new Error("The file should not exist");
    }
  }
  await IOUtils.copy(srcPath, destPath);
  info(`Copied ${aFilename} to ${destPath}`);
  return destPath;
}
