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

ChromeUtils.defineESModuleGetters(this, {
  FormHistory: "resource://gre/modules/FormHistory.sys.mjs",
  FormHistoryTestUtils:
    "resource://testing-common/FormHistoryTestUtils.sys.mjs",
  Sqlite: "resource://gre/modules/Sqlite.sys.mjs",
});

do_get_profile();

// Send the profile-after-change notification to the form history component to ensure
// that it has been initialized.
var formHistoryStartup = Cc[
  "@mozilla.org/satchel/form-history-startup;1"
].getService(Ci.nsIObserver);
formHistoryStartup.observe(null, "profile-after-change", null);

async function getDBVersion(dbfile) {
  let dbConnection = await Sqlite.openConnection({ path: dbfile.path });
  let version = await dbConnection.getSchemaVersion();
  await dbConnection.close();

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
  FormHistory.search(terms, params).then(
    results => iter.next(results),
    error => do_throw("Error occurred searching form history: " + error)
  );
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

  FormHistory.count(obj).then(
    count => {
      then(count);
    },
    error => {
      do_throw("Error occurred searching form history: " + error);
    }
  );
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
    countEntries(name, value, function (result) {
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
  FormHistory.update(changes).then(then, error => {
    do_throw("Error occurred updating form history: " + error);
  });
}

function promiseUpdate(change) {
  return FormHistory.update(change);
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
 * @param {object} [options]
 * @param {object} [options.overwriteExisting]
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
