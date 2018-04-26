/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * FormHistory
 *
 * Used to store values that have been entered into forms which may later
 * be used to automatically fill in the values when the form is visited again.
 *
 * search(terms, queryData, callback)
 *   Look up values that have been previously stored.
 *     terms - array of terms to return data for
 *     queryData - object that contains the query terms
 *       The query object contains properties for each search criteria to match, where the value
 *       of the property specifies the value that term must have. For example,
 *       { term1: value1, term2: value2 }
 *     callback - callback that is called when results are available or an error occurs.
 *       The callback is passed a result array containing each found entry. Each element in
 *       the array is an object containing a property for each search term specified by 'terms'.
 * count(queryData, callback)
 *   Find the number of stored entries that match the given criteria.
 *     queryData - array of objects that indicate the query. See the search method for details.
 *     callback - callback that is called when results are available or an error occurs.
 *       The callback is passed the number of found entries.
 * update(changes, callback)
 *    Write data to form history storage.
 *      changes - an array of changes to be made. If only one change is to be made, it
 *                may be passed as an object rather than a one-element array.
 *        Each change object is of the form:
 *          { op: operation, term1: value1, term2: value2, ... }
 *        Valid operations are:
 *          add - add a new entry
 *          update - update an existing entry
 *          remove - remove an entry
 *          bump - update the last accessed time on an entry
 *        The terms specified allow matching of one or more specific entries. If no terms
 *        are specified then all entries are matched. This means that { op: "remove" } is
 *        used to remove all entries and clear the form history.
 *      callback - callback that is called when results have been stored.
 * getAutoCompeteResults(searchString, params, callback)
 *   Retrieve an array of form history values suitable for display in an autocomplete list.
 *   Returns an mozIStoragePendingStatement that can be used to cancel the operation if
 *   needed.
 *     searchString - the string to search for, typically the entered value of a textbox
 *     params - zero or more filter arguments:
 *       fieldname - form field name
 *       agedWeight
 *       bucketSize
 *       expiryDate
 *       maxTimeGroundings
 *       timeGroupingSize
 *       prefixWeight
 *       boundaryWeight
 *     callback - callback that is called with the array of results. Each result in the array
 *                is an object with four arguments:
 *                  text, textLowerCase, frecency, totalScore
 * schemaVersion
 *   This property holds the version of the database schema
 *
 * Terms:
 *  guid - entry identifier. For 'add', a guid will be generated.
 *  fieldname - form field name
 *  value - form value
 *  timesUsed - the number of times the entry has been accessed
 *  firstUsed - the time the the entry was first created
 *  lastUsed - the time the entry was last accessed
 *  firstUsedStart - search for entries created after or at this time
 *  firstUsedEnd - search for entries created before or at this time
 *  lastUsedStart - search for entries last accessed after or at this time
 *  lastUsedEnd - search for entries last accessed before or at this time
 *  newGuid - a special case valid only for 'update' and allows the guid for
 *            an existing record to be updated. The 'guid' term is the only
 *            other term which can be used (ie, you can not also specify a
 *            fieldname, value etc) and indicates the guid of the existing
 *            record that should be updated.
 *
 * In all of the above methods, the callback argument should be an object with
 * handleResult(result), handleFailure(error) and handleCompletion(reason) functions.
 * For search and getAutoCompeteResults, result is an object containing the desired
 * properties. For count, result is the integer count. For, update, handleResult is
 * not called. For handleCompletion, reason is either 0 if successful or 1 if
 * an error occurred.
 */

var EXPORTED_SYMBOLS = ["FormHistory"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "uuidService",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");
ChromeUtils.defineModuleGetter(this, "OS",
                               "resource://gre/modules/osfile.jsm");
ChromeUtils.defineModuleGetter(this, "Sqlite",
                               "resource://gre/modules/Sqlite.jsm");


const DB_SCHEMA_VERSION = 4;
const DAY_IN_MS  = 86400000; // 1 day in milliseconds
const MAX_SEARCH_TOKENS = 10;
const NOOP = function noop() {};
const DB_FILENAME = "formhistory.sqlite";

var supportsDeletedTable = AppConstants.platform == "android";

var Prefs = {
  initialized: false,

  get debug() { this.ensureInitialized(); return this._debug; },
  get enabled() { this.ensureInitialized(); return this._enabled; },
  get expireDays() { this.ensureInitialized(); return this._expireDays; },

  ensureInitialized() {
    if (this.initialized) {
      return;
    }

    this.initialized = true;

    this._debug = Services.prefs.getBoolPref("browser.formfill.debug");
    this._enabled = Services.prefs.getBoolPref("browser.formfill.enable");
    this._expireDays = Services.prefs.getIntPref("browser.formfill.expire_days");
  },
};

function log(aMessage) {
  if (Prefs.debug) {
    Services.console.logStringMessage("FormHistory: " + aMessage);
  }
}

function sendNotification(aType, aData) {
  if (typeof aData == "string") {
    let strWrapper = Cc["@mozilla.org/supports-string;1"]
                     .createInstance(Ci.nsISupportsString);
    strWrapper.data = aData;
    aData = strWrapper;
  } else if (typeof aData == "number") {
    let intWrapper = Cc["@mozilla.org/supports-PRInt64;1"]
                     .createInstance(Ci.nsISupportsPRInt64);
    intWrapper.data = aData;
    aData = intWrapper;
  } else if (aData) {
    throw Components.Exception("Invalid type " + (typeof aType) + " passed to sendNotification",
                               Cr.NS_ERROR_ILLEGAL_VALUE);
  }

  Services.obs.notifyObservers(aData, "satchel-storage-changed", aType);
}

/**
 * Current database schema
 */

const dbSchema = {
  tables: {
    moz_formhistory: {
      "id": "INTEGER PRIMARY KEY",
      "fieldname": "TEXT NOT NULL",
      "value": "TEXT NOT NULL",
      "timesUsed": "INTEGER",
      "firstUsed": "INTEGER",
      "lastUsed": "INTEGER",
      "guid": "TEXT",
    },
    moz_deleted_formhistory: {
      "id": "INTEGER PRIMARY KEY",
      "timeDeleted": "INTEGER",
      "guid": "TEXT",
    },
  },
  indices: {
    moz_formhistory_index: {
      table: "moz_formhistory",
      columns: ["fieldname"],
    },
    moz_formhistory_lastused_index: {
      table: "moz_formhistory",
      columns: ["lastUsed"],
    },
    moz_formhistory_guid_index: {
      table: "moz_formhistory",
      columns: ["guid"],
    },
  },
};

/**
 * Validating and processing API querying data
 */

const validFields = [
  "fieldname",
  "value",
  "timesUsed",
  "firstUsed",
  "lastUsed",
  "guid",
];

const searchFilters = [
  "firstUsedStart",
  "firstUsedEnd",
  "lastUsedStart",
  "lastUsedEnd",
];

function validateOpData(aData, aDataType) {
  let thisValidFields = validFields;
  // A special case to update the GUID - in this case there can be a 'newGuid'
  // field and of the normally valid fields, only 'guid' is accepted.
  if (aDataType == "Update" && "newGuid" in aData) {
    thisValidFields = ["guid", "newGuid"];
  }
  for (let field in aData) {
    if (field != "op" && !thisValidFields.includes(field)) {
      throw Components.Exception(
        aDataType + " query contains an unrecognized field: " + field,
        Cr.NS_ERROR_ILLEGAL_VALUE);
    }
  }
  return aData;
}

function validateSearchData(aData, aDataType) {
  for (let field in aData) {
    if (field != "op" && !validFields.includes(field) && !searchFilters.includes(field)) {
      throw Components.Exception(
        aDataType + " query contains an unrecognized field: " + field,
        Cr.NS_ERROR_ILLEGAL_VALUE);
    }
  }
}

function makeQueryPredicates(aQueryData, delimiter = " AND ") {
  return Object.keys(aQueryData).map(field => {
    switch (field) {
      case "firstUsedStart": {
        return "firstUsed >= :" + field;
      }
      case "firstUsedEnd": {
        return "firstUsed <= :" + field;
      }
      case "lastUsedStart": {
        return "lastUsed >= :" + field;
      }
      case "lastUsedEnd": {
        return "lastUsed <= :" + field;
      }
    }

    return field + " = :" + field;
  }).join(delimiter);
}

function generateGUID() {
  // string like: "{f60d9eac-9421-4abc-8491-8e8322b063d4}"
  let uuid = uuidService.generateUUID().toString();
  let raw = ""; // A string with the low bytes set to random values
  let bytes = 0;
  for (let i = 1; bytes < 12; i += 2) {
    // Skip dashes
    if (uuid[i] == "-") {
      i++;
    }
    let hexVal = parseInt(uuid[i] + uuid[i + 1], 16);
    raw += String.fromCharCode(hexVal);
    bytes++;
  }
  return btoa(raw);
}

var Migrators = {
  /*
   * Updates the DB schema to v3 (bug 506402).
   * Adds deleted form history table.
   */
  async dbAsyncMigrateToVersion4(conn) {
    const TABLE_NAME = "moz_deleted_formhistory";
    let tableExists = await conn.tableExists(TABLE_NAME);
    if (!tableExists) {
      let table = dbSchema.tables[TABLE_NAME];
      let tSQL = Object.keys(table).map(col => [col, table[col]].join(" ")).join(", ");
      await conn.execute(`CREATE TABLE ${TABLE_NAME} (${tSQL})`);
    }
  },
};

/**
 * @typedef {Object} InsertQueryData
 * @property {Object} updatedChange
 *           A change requested by FormHistory.
 * @property {String} query
 *           The insert query string.
 */

/**
 * Prepares a query and some default parameters when inserting an entry
 * to the database.
 *
 * @param {Object} change
 *        The change requested by FormHistory.
 * @param {number} now
 *        The current timestamp in microseconds.
 * @returns {InsertQueryData}
 *          The query information needed to pass along to the database.
 */
function prepareInsertQuery(change, now) {
  let updatedChange = Object.assign({}, change);
  let query = "INSERT INTO moz_formhistory " +
              "(fieldname, value, timesUsed, firstUsed, lastUsed, guid) " +
              "VALUES (:fieldname, :value, :timesUsed, :firstUsed, :lastUsed, :guid)";
  updatedChange.timesUsed = updatedChange.timesUsed || 1;
  updatedChange.firstUsed = updatedChange.firstUsed || now;
  updatedChange.lastUsed = updatedChange.lastUsed || now;

  return {
    updatedChange,
    query,
  };
}

// There is a fieldname / value uniqueness constraint that's at this time
// only enforced at this level. This Map maps fieldnames => values that
// are in the process of being inserted into the database so that we know
// not to try to insert the same ones on top. Attempts to do so will be
// ignored.
var InProgressInserts = {
  _inProgress: new Map(),

  add(fieldname, value) {
    let fieldnameSet = this._inProgress.get(fieldname);
    if (!fieldnameSet) {
      this._inProgress.set(fieldname, new Set([value]));
      return true;
    }

    if (!fieldnameSet.has(value)) {
      fieldnameSet.add(value);
      return true;
    }

    return false;
  },

  clear(fieldnamesAndValues) {
    for (let [fieldname, value] of fieldnamesAndValues) {
      let fieldnameSet = this._inProgress.get(fieldname);
      if (fieldnameSet &&
          fieldnameSet.delete(value) &&
          fieldnameSet.size == 0) {
        this._inProgress.delete(fieldname);
      }
    }
  },
};

/**
 * Constructs and executes database statements from a pre-processed list of
 * inputted changes.
 *
 * @param {Array.<Object>} aChanges changes to form history
 * @param {Object} aPreparedHandlers
 */
// XXX This should be split up and the complexity reduced.
// eslint-disable-next-line complexity
async function updateFormHistoryWrite(aChanges, aPreparedHandlers) {
  log("updateFormHistoryWrite  " + aChanges.length);

  // pass 'now' down so that every entry in the batch has the same timestamp
  let now = Date.now() * 1000;
  let queries = [];
  let notifications = [];
  let adds = [];
  let conn = await FormHistory.db;

  for (let change of aChanges) {
    let operation = change.op;
    delete change.op;
    switch (operation) {
      case "remove": {
        log("Remove from form history  " + change);
        let queryTerms = makeQueryPredicates(change);

        // Fetch the GUIDs we are going to delete.
        try {
          let query = "SELECT guid FROM moz_formhistory";
          if (queryTerms) {
            query += " WHERE " + queryTerms;
          }

          await conn.executeCached(query, change, row => {
            notifications.push(["formhistory-remove", row.getResultByName("guid")]);
          });
        } catch (e) {
          log("Error getting guids from moz_formhistory: " + e);
        }

        if (supportsDeletedTable) {
          log("Moving to deleted table " + change);
          let query = "INSERT INTO moz_deleted_formhistory (guid, timeDeleted)";

          // TODO: Add these items to the deleted items table once we've sorted
          //       out the issues from bug 756701
          if (change.guid || queryTerms) {
            query +=
              change.guid ? " VALUES (:guid, :timeDeleted)"
                          : " SELECT guid, :timeDeleted FROM moz_formhistory WHERE " + queryTerms;
            change.timeDeleted = now;
            queries.push({ query, params: Object.assign({}, change) });
          }

          if ("timeDeleted" in change) {
            delete change.timeDeleted;
          }
        }

        let query = "DELETE FROM moz_formhistory";
        if (queryTerms) {
          log("removeEntries");
          query += " WHERE " + queryTerms;
        } else {
          log("removeAllEntries");
          // Not specifying any fields means we should remove all entries. We
          // won't need to modify the query in this case.
        }

        queries.push({ query, params: change });
        break;
      }
      case "update": {
        log("Update form history " + change);
        let guid = change.guid;
        delete change.guid;
        // a special case for updating the GUID - the new value can be
        // specified in newGuid.
        if (change.newGuid) {
          change.guid = change.newGuid;
          delete change.newGuid;
        }

        let query = "UPDATE moz_formhistory SET ";
        let queryTerms = makeQueryPredicates(change, ", ");
        if (!queryTerms) {
          throw Components.Exception("Update query must define fields to modify.",
                                     Cr.NS_ERROR_ILLEGAL_VALUE);
        }
        query += queryTerms + " WHERE guid = :existing_guid";
        change.existing_guid = guid;
        queries.push({ query, params: change });
        notifications.push(["formhistory-update", guid]);
        break;
      }
      case "bump": {
        log("Bump form history " + change);
        if (change.guid) {
          let query = "UPDATE moz_formhistory " +
                      "SET timesUsed = timesUsed + 1, lastUsed = :lastUsed WHERE guid = :guid";
          let queryParams = {
            lastUsed: now,
            guid: change.guid,
          };

          queries.push({ query, params: queryParams });
          notifications.push(["formhistory-update", change.guid]);
        } else {
          if (!InProgressInserts.add(change.fieldname, change.value)) {
            // This updateFormHistoryWrite call, or a previous one, is already
            // going to add this fieldname / value pair, so we can ignore this.
            continue;
          }
          adds.push([change.fieldname, change.value]);
          change.guid = generateGUID();
          let { query, updatedChange } = prepareInsertQuery(change, now);
          queries.push({ query, params: updatedChange });
          notifications.push(["formhistory-add", updatedChange.guid]);
        }
        break;
      }
      case "add": {
        if (!InProgressInserts.add(change.fieldname, change.value)) {
          // This updateFormHistoryWrite call, or a previous one, is already
          // going to add this fieldname / value pair, so we can ignore this.
          continue;
        }
        adds.push([change.fieldname, change.value]);

        log("Add to form history " + change);
        if (!change.guid) {
          change.guid = generateGUID();
        }

        let { query, updatedChange } = prepareInsertQuery(change, now);
        queries.push({ query, params: updatedChange });
        notifications.push(["formhistory-add", updatedChange.guid]);
        break;
      }
      default: {
        // We should've already guaranteed that change.op is one of the above
        throw Components.Exception("Invalid operation " + operation,
                                   Cr.NS_ERROR_ILLEGAL_VALUE);
      }
    }
  }

  try {
    await runUpdateQueries(conn, queries);
    for (let [notification, param] of notifications) {
      // We're either sending a GUID or nothing at all.
      sendNotification(notification, param);
    }

    aPreparedHandlers.handleCompletion(0);
  } catch (e) {
    aPreparedHandlers.handleError(e);
    aPreparedHandlers.handleCompletion(1);
  } finally {
    InProgressInserts.clear(adds);
  }
}

/**
 * Runs queries for an update operation to the database. This
 * is separated out from updateFormHistoryWrite to avoid shutdown
 * leaks where the handlers passed to updateFormHistoryWrite would
 * leak from the closure around the executeTransaction function.
 *
 * @param {SqliteConnection} conn the database connection
 * @param {Object} queries query string and param pairs generated
 *                 by updateFormHistoryWrite
 */
async function runUpdateQueries(conn, queries) {
  await conn.executeTransaction(async () => {
    for (let { query, params } of queries) {
      await conn.executeCached(query, params);
    }
  });
}

/**
 * Functions that expire entries in form history and shrinks database
 * afterwards as necessary initiated by expireOldEntries.
 */

/**
 * Removes entries from database.
 *
 * @param {number} aExpireTime expiration timestamp
 * @param {number} aBeginningCount numer of entries at first
 */
function expireOldEntriesDeletion(aExpireTime, aBeginningCount) {
  log("expireOldEntriesDeletion(" + aExpireTime + "," + aBeginningCount + ")");

  FormHistory.update([{
    op: "remove",
    lastUsedEnd: aExpireTime,
  }], {
    handleCompletion() {
      expireOldEntriesVacuum(aExpireTime, aBeginningCount);
    },
    handleError(aError) {
      log("expireOldEntriesDeletionFailure");
    },
  });
}

/**
 * Counts number of entries removed and shrinks database as necessary.
 *
 * @param {number} aExpireTime expiration timestamp
 * @param {number} aBeginningCount number of entries at first
 */
function expireOldEntriesVacuum(aExpireTime, aBeginningCount) {
  FormHistory.count({}, {
    handleResult(aEndingCount) {
      if (aBeginningCount - aEndingCount > 500) {
        log("expireOldEntriesVacuum");

        FormHistory.db.then(async conn => {
          try {
            await conn.executeCached("VACUUM");
          } catch (e) {
            log("expireVacuumError");
          }
        });
      }

      sendNotification("formhistory-expireoldentries", aExpireTime);
    },
    handleError(aError) {
      log("expireEndCountFailure");
    },
  });
}


/**
 * Database creation and access. Used by FormHistory and some of the
 * utility functions, but is not exposed to the outside world.
 * @class
 */
var DB = {
  // Once we establish a database connection, we have to hold a reference
  // to it so that it won't get GC'd.
  _instance: null,
  // MAX_ATTEMPTS is how many times we'll try to establish a connection
  // or migrate a database before giving up.
  MAX_ATTEMPTS: 2,

  /** String representing where the FormHistory database is on the filesystem */
  get path() {
    return OS.Path.join(OS.Constants.Path.profileDir, DB_FILENAME);
  },

  /**
   * Sets up and returns a connection to the FormHistory database. The
   * connection also registers itself with AsyncShutdown so that the
   * connection is closed on when the profile-before-change observer
   * notification is fired.
   *
   * @returns {Promise}
   * @resolves An Sqlite.jsm connection to the database.
   * @rejects  If connecting to the database, or migrating the database
   *           failed after MAX_ATTEMPTS attempts (where each attempt
   *           backs up and deletes the old database), this will reject
   *           with the Sqlite.jsm error.
   */
  get conn() {
    delete this.conn;
    let conn = new Promise(async (resolve, reject) => {
      try {
        this._instance = await this._establishConn();
      } catch (e) {
        log("Failed to establish database connection: " + e);
        reject(e);
        return;
      }

      resolve(this._instance);
    });

    return this.conn = conn;
  },

  // Private functions

  /**
   * Tries to connect to the Sqlite database at this.path, and then
   * migrates the database as necessary. If any of the steps to do this
   * fail, this function should re-enter itself with an incremented
   * attemptNum so that another attempt can be made after backing up
   * and deleting the old database.
   *
   * @async
   * @param {number} attemptNum
   *        The optional number of the attempt that is being made to connect
   *        to the database. Defaults to 0.
   * @returns {Promise}
   * @resolves An Sqlite.jsm connection to the database.
   * @rejects  After MAX_ATTEMPTS, this will reject with the Sqlite.jsm
   *           error.
   */
  async _establishConn(attemptNum = 0) {
    log(`Establishing database connection - attempt # ${attemptNum}`);
    let conn;
    try {
      conn = await Sqlite.openConnection({ path: this.path });
      Sqlite.shutdown.addBlocker(
        "Closing FormHistory database.", () => conn.close());
    } catch (e) {
      // Bug 1423729 - We should check the reason for the connection failure,
      // in case this is due to the disk being full or the database file being
      // inaccessible due to third-party software (like anti-virus software).
      // In that case, we should probably fail right away.
      if (attemptNum < this.MAX_ATTEMPTS) {
        log("Establishing connection failed.");
        await this._failover(conn);
        return this._establishConn(++attemptNum);
      }

      if (conn) {
        await conn.close();
      }
      log("Establishing connection failed too many times. Giving up.");
      throw e;
    }

    try {
      let dbVersion = parseInt(await conn.getSchemaVersion(), 10);

      // Case 1: Database is up to date and we're ready to go.
      if (dbVersion == DB_SCHEMA_VERSION) {
        return conn;
      }

      // Case 2: Downgrade
      if (dbVersion > DB_SCHEMA_VERSION) {
        log("Downgrading to version " + DB_SCHEMA_VERSION);
        // User's DB is newer. Sanity check that our expected columns are
        // present, and if so mark the lower version and merrily continue
        // on. If the columns are borked, something is wrong so blow away
        // the DB and start from scratch. [Future incompatible upgrades
        // should switch to a different table or file.]
        if (!(await this._expectedColumnsPresent(conn))) {
          throw Components.Exception("DB is missing expected columns",
                                     Cr.NS_ERROR_FILE_CORRUPTED);
        }

        // Change the stored version to the current version. If the user
        // runs the newer code again, it will see the lower version number
        // and re-upgrade (to fixup any entries the old code added).
        await conn.setSchemaVersion(DB_SCHEMA_VERSION);
        return conn;
      }

      // Case 3: Very old database that cannot be migrated.
      //
      // When FormHistory is released, we will no longer support the various
      // schema versions prior to this release that nsIFormHistory2 once did.
      // We'll throw an NS_ERROR_FILE_CORRUPTED, which should cause us to wipe
      // out this DB and create a new one (unless this is our MAX_ATTEMPTS
      // attempt).
      if (dbVersion > 0 && dbVersion < 3) {
        throw Components.Exception("DB version is unsupported.",
                                   Cr.NS_ERROR_FILE_CORRUPTED);
      }

      if (dbVersion == 0) {
        // Case 4: New database
        await conn.executeTransaction(async () => {
          log("Creating DB -- tables");
          for (let name in dbSchema.tables) {
            let table = dbSchema.tables[name];
            let tSQL = Object.keys(table).map(col => [col, table[col]].join(" ")).join(", ");
            log("Creating table " + name + " with " + tSQL);
            await conn.execute(`CREATE TABLE ${name} (${tSQL})`);
          }

          log("Creating DB -- indices");
          for (let name in dbSchema.indices) {
            let index = dbSchema.indices[name];
            let statement = "CREATE INDEX IF NOT EXISTS " + name + " ON " + index.table +
                            "(" + index.columns.join(", ") + ")";
            await conn.execute(statement);
          }
        });
      } else {
        // Case 5: Old database requiring a migration
        await conn.executeTransaction(async () => {
          for (let v = dbVersion + 1; v <= DB_SCHEMA_VERSION; v++) {
            log("Upgrading to version " + v + "...");
            await Migrators["dbAsyncMigrateToVersion" + v](conn);
          }
        });
      }

      await conn.setSchemaVersion(DB_SCHEMA_VERSION);

      return conn;
    } catch (e) {
      if (e.result != Cr.NS_ERROR_FILE_CORRUPTED) {
        throw e;
      }

      if (attemptNum < this.MAX_ATTEMPTS) {
        log("Setting up database failed.");
        await this._failover(conn);
        return this._establishConn(++attemptNum);
      }

      if (conn) {
        await conn.close();
      }

      log("Setting up database failed too many times. Giving up.");

      throw e;
    }
  },

  /**
   * Closes a connection to the database, then backs up the database before
   * deleting it.
   *
   * @async
   * @param {SqliteConnection | null} conn
   *        The connection to the database that we failed to establish or
   *        migrate.
   * @throws If any file operations fail.
   */
  async _failover(conn) {
    log("Cleaning up DB file - close & remove & backup.");
    if (conn) {
      await conn.close();
    }
    let backupFile = this.path + ".corrupt";
    let { file, path: uniquePath } =
      await OS.File.openUnique(backupFile, { humanReadable: true });
    await file.close();
    await OS.File.copy(this.path, uniquePath);
    await OS.File.remove(this.path);
    log("Completed DB cleanup.");
  },

  /**
   * Tests that a database connection contains the tables that we expect.
   *
   * @async
   * @param {SqliteConnection | null} conn
   *        The connection to the database that we're testing.
   * @returns {Promise}
   * @resolves true if all expected columns are present.
   */
  async _expectedColumnsPresent(conn) {
    for (let name in dbSchema.tables) {
      let table = dbSchema.tables[name];
      let query = "SELECT " +
                  Object.keys(table).join(", ") +
                  " FROM " + name;
      try {
        await conn.execute(query, null, (row, cancel) => {
          // One row is enough to let us know this worked.
          cancel();
        });
      } catch (e) {
        return false;
      }
    }

    log("Verified that expected columns are present in DB.");
    return true;
  },
};

this.FormHistory = {
  get db() {
    return DB.conn;
  },

  get enabled() {
    return Prefs.enabled;
  },

  _prepareHandlers(handlers) {
    let defaultHandlers = {
      handleResult: NOOP,
      handleError: NOOP,
      handleCompletion: NOOP,
    };

    if (!handlers) {
      return defaultHandlers;
    }

    if (handlers.handleResult) {
      defaultHandlers.handleResult = handlers.handleResult;
    }
    if (handlers.handleError) {
      defaultHandlers.handleError = handlers.handleError;
    }
    if (handlers.handleCompletion) {
      defaultHandlers.handleCompletion = handlers.handleCompletion;
    }

    return defaultHandlers;
  },

  search(aSelectTerms, aSearchData, aRowFuncOrHandlers) {
    // if no terms selected, select everything
    if (!aSelectTerms) {
      aSelectTerms = validFields;
    }
    validateSearchData(aSearchData, "Search");

    let query = "SELECT " + aSelectTerms.join(", ") + " FROM moz_formhistory";
    let queryTerms = makeQueryPredicates(aSearchData);
    if (queryTerms) {
      query += " WHERE " + queryTerms;
    }

    let handlers;

    if (typeof aRowFuncOrHandlers == "function") {
      handlers = this._prepareHandlers();
      handlers.handleResult = aRowFuncOrHandlers;
    } else if (typeof aRowFuncOrHandlers == "object") {
      handlers = this._prepareHandlers(aRowFuncOrHandlers);
    }

    let allResults = [];

    return new Promise((resolve, reject) => {
      this.db.then(async conn => {
        try {
          await conn.executeCached(query, aSearchData, row => {
            let result = {};
            for (let field of aSelectTerms) {
              result[field] = row.getResultByName(field);
            }

            if (handlers) {
              handlers.handleResult(result);
            } else {
              allResults.push(result);
            }
          });
          if (handlers) {
            handlers.handleCompletion(0);
          }
          resolve(allResults);
        } catch (e) {
          if (handlers) {
            handlers.handleError(e);
            handlers.handleCompletion(1);
          }
          reject(e);
        }
      });
    });
  },

  count(aSearchData, aHandlers) {
    validateSearchData(aSearchData, "Count");

    let query = "SELECT COUNT(*) AS numEntries FROM moz_formhistory";
    let queryTerms = makeQueryPredicates(aSearchData);
    if (queryTerms) {
      query += " WHERE " + queryTerms;
    }

    let handlers = this._prepareHandlers(aHandlers);

    return new Promise((resolve, reject) => {
      this.db.then(async conn => {
        try {
          let rows = await conn.executeCached(query, aSearchData);
          let count = rows[0].getResultByName("numEntries");
          handlers.handleResult(count);
          handlers.handleCompletion(0);
          resolve(count);
        } catch (e) {
          handlers.handleError(e);
          handlers.handleCompletion(1);
          reject(e);
        }
      });
    });
  },

  update(aChanges, aHandlers) {
    // Used to keep track of how many searches have been started. When that number
    // are finished, updateFormHistoryWrite can be called.
    let numSearches = 0;
    let completedSearches = 0;
    let searchFailed = false;

    function validIdentifier(change) {
      // The identifier is only valid if one of either the guid
      // or the (fieldname/value) are set (so an X-OR)
      return Boolean(change.guid) != Boolean(change.fieldname && change.value);
    }

    if (!("length" in aChanges)) {
      aChanges = [aChanges];
    }

    let handlers = this._prepareHandlers(aHandlers);

    let isRemoveOperation = aChanges.every(change => change && change.op && change.op == "remove");
    if (!Prefs.enabled && !isRemoveOperation) {
      handlers.handleError({
        message: "Form history is disabled, only remove operations are allowed",
        result: Ci.mozIStorageError.MISUSE,
      });
      handlers.handleCompletion(1);
      return;
    }

    for (let change of aChanges) {
      switch (change.op) {
        case "remove":
          validateSearchData(change, "Remove");
          continue;
        case "update":
          if (validIdentifier(change)) {
            validateOpData(change, "Update");
            if (change.guid) {
              continue;
            }
          } else {
            throw Components.Exception(
              "update op='update' does not correctly reference a entry.",
              Cr.NS_ERROR_ILLEGAL_VALUE);
          }
          break;
        case "bump":
          if (validIdentifier(change)) {
            validateOpData(change, "Bump");
            if (change.guid) {
              continue;
            }
          } else {
            throw Components.Exception(
              "update op='bump' does not correctly reference a entry.",
              Cr.NS_ERROR_ILLEGAL_VALUE);
          }
          break;
        case "add":
          if (change.fieldname && change.value) {
            validateOpData(change, "Add");
          } else {
            throw Components.Exception(
              "update op='add' must have a fieldname and a value.",
              Cr.NS_ERROR_ILLEGAL_VALUE);
          }
          break;
        default:
          throw Components.Exception(
            "update does not recognize op='" + change.op + "'",
            Cr.NS_ERROR_ILLEGAL_VALUE);
      }

      numSearches++;
      let changeToUpdate = change;
      FormHistory.search(
        ["guid"],
        {
          fieldname: change.fieldname,
          value: change.value,
        }, {
          foundResult: false,
          handleResult(aResult) {
            if (this.foundResult) {
              log("Database contains multiple entries with the same fieldname/value pair.");
              handlers.handleError({
                message:
                  "Database contains multiple entries with the same fieldname/value pair.",
                result: 19, // Constraint violation
              });

              searchFailed = true;
              return;
            }

            this.foundResult = true;
            changeToUpdate.guid = aResult.guid;
          },

          handleError(aError) {
            handlers.handleError(aError);
          },

          handleCompletion(aReason) {
            completedSearches++;
            if (completedSearches == numSearches) {
              if (!aReason && !searchFailed) {
                updateFormHistoryWrite(aChanges, handlers);
              } else {
                handlers.handleCompletion(1);
              }
            }
          },
        });
    }

    if (numSearches == 0) {
      // We don't have to wait for any statements to return.
      updateFormHistoryWrite(aChanges, handlers);
    }
  },

  getAutoCompleteResults(searchString, params, aHandlers) {
    // only do substring matching when the search string contains more than one character
    let searchTokens;
    let where = "";
    let boundaryCalc = "";

    if (searchString.length >= 1) {
      params.valuePrefix = searchString + "%";
    }

    if (searchString.length > 1) {
      searchTokens = searchString.split(/\s+/);

      // build up the word boundary and prefix match bonus calculation
      boundaryCalc = "MAX(1, :prefixWeight * (value LIKE :valuePrefix ESCAPE '/') + (";
      // for each word, calculate word boundary weights for the SELECT clause and
      // add word to the WHERE clause of the query
      let tokenCalc = [];
      let searchTokenCount = Math.min(searchTokens.length, MAX_SEARCH_TOKENS);
      for (let i = 0; i < searchTokenCount; i++) {
        let escapedToken = searchTokens[i];
        params["tokenBegin" + i] = escapedToken + "%";
        params["tokenBoundary" + i] = "% " + escapedToken + "%";
        params["tokenContains" + i] = "%" + escapedToken + "%";

        tokenCalc.push("(value LIKE :tokenBegin" + i + " ESCAPE '/') + " +
                            "(value LIKE :tokenBoundary" + i + " ESCAPE '/')");
        where += "AND (value LIKE :tokenContains" + i + " ESCAPE '/') ";
      }
      // add more weight if we have a traditional prefix match and
      // multiply boundary bonuses by boundary weight
      boundaryCalc += tokenCalc.join(" + ") + ") * :boundaryWeight)";
    } else if (searchString.length == 1) {
      where = "AND (value LIKE :valuePrefix ESCAPE '/') ";
      boundaryCalc = "1";
      delete params.prefixWeight;
      delete params.boundaryWeight;
    } else {
      where = "";
      boundaryCalc = "1";
      delete params.prefixWeight;
      delete params.boundaryWeight;
    }

    params.now = Date.now() * 1000; // convert from ms to microseconds

    let handlers = this._prepareHandlers(aHandlers);

    /* Three factors in the frecency calculation for an entry (in order of use in calculation):
     * 1) average number of times used - items used more are ranked higher
     * 2) how recently it was last used - items used recently are ranked higher
     * 3) additional weight for aged entries surviving expiry - these entries are relevant
     *    since they have been used multiple times over a large time span so rank them higher
     * The score is then divided by the bucket size and we round the result so that entries
     * with a very similar frecency are bucketed together with an alphabetical sort. This is
     * to reduce the amount of moving around by entries while typing.
     */

    let query = "/* do not warn (bug 496471): can't use an index */ " +
                "SELECT value, guid, " +
                "ROUND( " +
                    "timesUsed / MAX(1.0, (lastUsed - firstUsed) / :timeGroupingSize) * " +
                    "MAX(1.0, :maxTimeGroupings - (:now - lastUsed) / :timeGroupingSize) * " +
                    "MAX(1.0, :agedWeight * (firstUsed < :expiryDate)) / " +
                    ":bucketSize " +
                ", 3) AS frecency, " +
                boundaryCalc + " AS boundaryBonuses " +
                "FROM moz_formhistory " +
                "WHERE fieldname=:fieldname " + where +
                "ORDER BY ROUND(frecency * boundaryBonuses) DESC, UPPER(value) ASC";

    let cancelled = false;

    let cancellableQuery = {
      cancel() {
        cancelled = true;
      },
    };

    this.db.then(async conn => {
      try {
        await conn.executeCached(query, params, (row, cancel) => {
          if (cancelled) {
            cancel();
            return;
          }

          let value = row.getResultByName("value");
          let guid = row.getResultByName("guid");
          let frecency = row.getResultByName("frecency");
          let entry = {
            text: value,
            guid,
            textLowerCase: value.toLowerCase(),
            frecency,
            totalScore: Math.round(frecency * row.getResultByName("boundaryBonuses")),
          };
          handlers.handleResult(entry);
        });
        handlers.handleCompletion(0);
      } catch (e) {
        handlers.handleError(e);
        handlers.handleCompletion(1);
      }
    });

    return cancellableQuery;
  },

  // This is used only so that the test can verify deleted table support.
  get _supportsDeletedTable() {
    return supportsDeletedTable;
  },
  set _supportsDeletedTable(val) {
    supportsDeletedTable = val;
  },

  // The remaining methods are called by FormHistoryStartup.js
  updatePrefs() {
    Prefs.initialized = false;
  },

  expireOldEntries() {
    log("expireOldEntries");

    // Determine how many days of history we're supposed to keep.
    // Calculate expireTime in microseconds
    let expireTime = (Date.now() - Prefs.expireDays * DAY_IN_MS) * 1000;

    sendNotification("formhistory-beforeexpireoldentries", expireTime);

    FormHistory.count({}, {
      handleResult(aBeginningCount) {
        expireOldEntriesDeletion(expireTime, aBeginningCount);
      },
      handleError(aError) {
        log("expireStartCountFailure");
      },
    });
  },
};

// Prevent add-ons from redefining this API
Object.freeze(FormHistory);
