/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * FormHistory
 *
 * Used to store values that have been entered into forms which may later
 * be used to automatically fill in the values when the form is visited again.
 *
 * async search(terms, queryData)
 *   Look up values that have been previously stored.
 *     terms - array of terms to return data for
 *     queryData - object that contains the query terms
 *       The query object contains properties for each search criteria to match, where the value
 *       of the property specifies the value that term must have. For example,
 *       { term1: value1, term2: value2 }
 *   Resolves to an array containing the found results. Each element in
 *   the array is an object containing a property for each search term
 *   specified by 'terms'.
 *   Rejects in case of errors.
 * async count(queryData)
 *   Find the number of stored entries that match the given criteria.
 *     queryData - array of objects that indicate the query. See the search method for details.
 *   Resolves to the number of found entries.
 *   Rejects in case of errors.
 * async update(changes)
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
 *   Resolves once the operation is complete.
 *   Rejects in case of errors.
 * async getAutoCompeteResults(searchString, params, callback)
 *   Retrieve an array of form history values suitable for display in an autocomplete list.
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
 *       source
 *     callback - callback that is invoked for each result, the second argument
 *                is a function that can be used to cancel the operation.
 *                Each result is an object with four properties:
 *                  text, textLowerCase, frecency, totalScore
 *   Resolves with an array of results, once the operation is complete.
 *   Rejects in case of errors.
 *
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
 */

export let FormHistory;

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Sqlite: "resource://gre/modules/Sqlite.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

const DB_SCHEMA_VERSION = 5;
const DAY_IN_MS = 86400000; // 1 day in milliseconds
const MAX_SEARCH_TOKENS = 10;
const DB_FILENAME = "formhistory.sqlite";

var supportsDeletedTable = AppConstants.platform == "android";

const wait = ms => new Promise(res => lazy.setTimeout(res, ms));

var Prefs = {
  _initialized: false,

  get(name) {
    this.ensureInitialized();
    return this[`_${name}`];
  },

  ensureInitialized() {
    if (this._initialized) {
      return;
    }

    this._initialized = true;

    this._prefBranch = Services.prefs.getBranch("browser.formfill.");
    this._prefBranch.addObserver("", this, true);

    this._agedWeight = this._prefBranch.getIntPref("agedWeight");
    this._boundaryWeight = this._prefBranch.getIntPref("boundaryWeight");
    this._bucketSize = this._prefBranch.getIntPref("bucketSize");
    this._debug = this._prefBranch.getBoolPref("debug");
    this._enabled = this._prefBranch.getBoolPref("enable");
    this._expireDays = this._prefBranch.getIntPref("expire_days");
    this._maxTimeGroupings = this._prefBranch.getIntPref("maxTimeGroupings");
    this._prefixWeight = this._prefBranch.getIntPref("prefixWeight");
    this._timeGroupingSize =
      this._prefBranch.getIntPref("timeGroupingSize") * 1000 * 1000;
  },

  observe(_subject, topic, data) {
    if (topic == "nsPref:changed") {
      let prefName = data;
      log(`got change to ${prefName} preference`);

      switch (prefName) {
        case "agedWeight":
          this._agedWeight = this._prefBranch.getIntPref(prefName);
          break;
        case "boundaryWeight":
          this._boundaryWeight = this._prefBranch.getIntPref(prefName);
          break;
        case "bucketSize":
          this._bucketSize = this._prefBranch.getIntPref(prefName);
          break;
        case "debug":
          this._debug = this._prefBranch.getBoolPref(prefName);
          break;
        case "enable":
          this._enabled = this._prefBranch.getBoolPref(prefName);
          break;
        case "expire_days":
          this._expireDays = this._prefBranch.getIntPref("expire_days");
          break;
        case "maxTimeGroupings":
          this._maxTimeGroupings = this._prefBranch.getIntPref(prefName);
          break;
        case "prefixWeight":
          this._prefixWeight = this._prefBranch.getIntPref(prefName);
          break;
        case "timeGroupingSize":
          this._timeGroupingSize =
            this._prefBranch.getIntPref(prefName) * 1000 * 1000;
          break;
        default:
          log(`Oops! Pref ${prefName} not handled, change ignored.`);
          break;
      }
    }
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),
};

function log(aMessage) {
  if (Prefs.get("debug")) {
    Services.console.logStringMessage("FormHistory: " + aMessage);
  }
}

function sendNotification(aType, aData) {
  if (typeof aData == "string") {
    const strWrapper = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString
    );
    strWrapper.data = aData;
    aData = strWrapper;
  } else if (typeof aData == "number") {
    const intWrapper = Cc["@mozilla.org/supports-PRInt64;1"].createInstance(
      Ci.nsISupportsPRInt64
    );
    intWrapper.data = aData;
    aData = intWrapper;
  } else if (aData) {
    throw Components.Exception(
      `Invalid type ${typeof aType} passed to sendNotification`,
      Cr.NS_ERROR_ILLEGAL_VALUE
    );
  }

  Services.obs.notifyObservers(aData, "satchel-storage-changed", aType);
}

/**
 * Current database schema
 */

const dbSchema = {
  tables: {
    moz_formhistory: {
      id: "INTEGER PRIMARY KEY",
      fieldname: "TEXT NOT NULL",
      value: "TEXT NOT NULL",
      timesUsed: "INTEGER",
      firstUsed: "INTEGER",
      lastUsed: "INTEGER",
      guid: "TEXT",
    },
    moz_deleted_formhistory: {
      id: "INTEGER PRIMARY KEY",
      timeDeleted: "INTEGER",
      guid: "TEXT",
    },
    moz_sources: {
      id: "INTEGER PRIMARY KEY",
      source: "TEXT NOT NULL",
    },
    moz_history_to_sources: {
      history_id: "INTEGER",
      source_id: "INTEGER",
      SQL: `
        PRIMARY KEY (history_id, source_id),
        FOREIGN KEY (history_id) REFERENCES moz_formhistory(id) ON DELETE CASCADE,
        FOREIGN KEY (source_id) REFERENCES moz_sources(id) ON DELETE CASCADE
      `,
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
  "firstUsed",
  "guid",
  "lastUsed",
  "source",
  "timesUsed",
  "value",
];

const searchFilters = [
  "firstUsedStart",
  "firstUsedEnd",
  "lastUsedStart",
  "lastUsedEnd",
  "source",
];

function validateOpData(aData, aDataType) {
  let thisValidFields = validFields;
  // A special case to update the GUID - in this case there can be a 'newGuid'
  // field and of the normally valid fields, only 'guid' is accepted.
  if (aDataType == "Update" && "newGuid" in aData) {
    thisValidFields = ["guid", "newGuid"];
  }
  for (const field in aData) {
    if (field != "op" && !thisValidFields.includes(field)) {
      throw Components.Exception(
        `${aDataType} query contains an unrecognized field: ${field}`,
        Cr.NS_ERROR_ILLEGAL_VALUE
      );
    }
  }
  return aData;
}

function validateSearchData(aData, aDataType) {
  for (const field in aData) {
    if (
      field != "op" &&
      !validFields.includes(field) &&
      !searchFilters.includes(field)
    ) {
      throw Components.Exception(
        `${aDataType} query contains an unrecognized field: ${field}`,
        Cr.NS_ERROR_ILLEGAL_VALUE
      );
    }
  }
}

function makeQueryPredicates(aQueryData, delimiter = " AND ") {
  const params = {};
  const queryTerms = Object.keys(aQueryData)
    .filter(field => aQueryData[field] !== undefined)
    .map(field => {
      params[field] = aQueryData[field];
      switch (field) {
        case "firstUsedStart":
          return "firstUsed >= :" + field;
        case "firstUsedEnd":
          return "firstUsed <= :" + field;
        case "lastUsedStart":
          return "lastUsed >= :" + field;
        case "lastUsedEnd":
          return "lastUsed <= :" + field;
        case "source":
          return `EXISTS(
            SELECT 1 FROM moz_history_to_sources
            JOIN moz_sources s ON s.id = source_id
            WHERE source = :${field}
              AND history_id = moz_formhistory.id
          )`;
      }
      return field + " = :" + field;
    })
    .join(delimiter);
  return { queryTerms, params };
}

function generateGUID() {
  // string like: "{f60d9eac-9421-4abc-8491-8e8322b063d4}"
  const uuid = Services.uuid.generateUUID().toString();
  let raw = ""; // A string with the low bytes set to random values
  let bytes = 0;
  for (let i = 1; bytes < 12; i += 2) {
    // Skip dashes
    if (uuid[i] == "-") {
      i++;
    }
    const hexVal = parseInt(uuid[i] + uuid[i + 1], 16);
    raw += String.fromCharCode(hexVal);
    bytes++;
  }
  return btoa(raw);
}

var Migrators = {
  // Bug 506402 - Adds deleted form history table.
  async dbAsyncMigrateToVersion4(conn) {
    const tableName = "moz_deleted_formhistory";
    const tableExists = await conn.tableExists(tableName);
    if (!tableExists) {
      await createTable(conn, tableName);
    }
  },

  // Bug 1654862 - Adds sources and moz_history_to_sources tables.
  async dbAsyncMigrateToVersion5(conn) {
    if (!(await conn.tableExists("moz_sources"))) {
      for (const tableName of ["moz_history_to_sources", "moz_sources"]) {
        await createTable(conn, tableName);
      }
    }
  },
};

/**
 * @typedef {object} InsertQueryData
 * @property {object} updatedChange
 *           A change requested by FormHistory.
 * @property {string} query
 *           The insert query string.
 */

/**
 * Prepares a query and some default parameters when inserting an entry
 * to the database.
 *
 * @param {object} change
 *        The change requested by FormHistory.
 * @param {number} now
 *        The current timestamp in microseconds.
 * @returns {InsertQueryData}
 *          The query information needed to pass along to the database.
 */
function prepareInsertQuery(change, now) {
  const params = {};
  for (const key of new Set([
    ...Object.keys(change),
    // These must always be NOT NULL.
    "firstUsed",
    "lastUsed",
    "timesUsed",
  ])) {
    switch (key) {
      case "fieldname":
      case "guid":
      case "value":
        params[key] = change[key];
        break;
      case "firstUsed":
      case "lastUsed":
        params[key] = change[key] || now;
        break;
      case "timesUsed":
        params[key] = change[key] || 1;
        break;
      default:
      // Skip unnecessary properties.
    }
  }

  return {
    query: `
      INSERT INTO moz_formhistory
        (fieldname, value, timesUsed, firstUsed, lastUsed, guid)
      VALUES (:fieldname, :value, :timesUsed, :firstUsed, :lastUsed, :guid)`,
    params,
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
    const fieldnameSet = this._inProgress.get(fieldname);
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
    for (const [fieldname, value] of fieldnamesAndValues) {
      const fieldnameSet = this._inProgress.get(fieldname);
      if (fieldnameSet?.delete(value) && fieldnameSet.size == 0) {
        this._inProgress.delete(fieldname);
      }
    }
  },
};

function getAddSourceToGuidQueries(source, guid) {
  return [
    {
      query: `INSERT OR IGNORE INTO moz_sources (source) VALUES (:source)`,
      params: { source },
    },
    {
      query: `
        INSERT OR IGNORE INTO moz_history_to_sources (history_id, source_id)
        VALUES(
          (SELECT id FROM moz_formhistory WHERE guid = :guid),
          (SELECT id FROM moz_sources WHERE source = :source)
        )
      `,
      params: { guid, source },
    },
  ];
}

/**
 * Constructs and executes database statements from a pre-processed list of
 * inputted changes.
 *
 * @param {Array.<object>} aChanges changes to form history
 */
// XXX This should be split up and the complexity reduced.
// eslint-disable-next-line complexity
async function updateFormHistoryWrite(aChanges) {
  log("updateFormHistoryWrite  " + aChanges.length);

  // pass 'now' down so that every entry in the batch has the same timestamp
  const now = Date.now() * 1000;
  let queries = [];
  const notifications = [];
  const adds = [];
  const conn = await FormHistory.db;

  for (const change of aChanges) {
    const operation = change.op;
    delete change.op;
    switch (operation) {
      case "remove": {
        log("Remove from form history  " + change);
        const { queryTerms, params } = makeQueryPredicates(change);

        // If source is defined, we only remove the source relation, if the
        // consumer intends to remove the value from everywhere, then they
        // should not pass source. This gives full control to the caller.
        if (change.source) {
          await conn.executeCached(
            `DELETE FROM moz_history_to_sources
              WHERE source_id = (
                SELECT id FROM moz_sources WHERE source = :source
              )
              AND history_id = (
                SELECT id FROM moz_formhistory WHERE ${queryTerms}
              )
            `,
            params
          );
          break;
        }

        // Fetch the GUIDs we are going to delete.
        try {
          let query = "SELECT guid FROM moz_formhistory";
          if (queryTerms) {
            query += " WHERE " + queryTerms;
          }

          await conn.executeCached(query, params, row => {
            notifications.push([
              "formhistory-remove",
              row.getResultByName("guid"),
            ]);
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
            query += change.guid
              ? " VALUES (:guid, :timeDeleted)"
              : " SELECT guid, :timeDeleted FROM moz_formhistory WHERE " +
                queryTerms;
            queries.push({
              query,
              params: Object.assign({ timeDeleted: now }, params),
            });
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

        queries.push({ query, params });
        // Expire orphan sources.
        queries.push({
          query: `
            DELETE FROM moz_sources WHERE id NOT IN (
              SELECT DISTINCT source_id FROM moz_history_to_sources
            )`,
        });
        break;
      }
      case "update": {
        log("Update form history " + change);
        const guid = change.guid;
        delete change.guid;
        // a special case for updating the GUID - the new value can be
        // specified in newGuid.
        if (change.newGuid) {
          change.guid = change.newGuid;
          delete change.newGuid;
        }

        let query = "UPDATE moz_formhistory SET ";
        let { queryTerms, params } = makeQueryPredicates(change, ", ");
        if (!queryTerms) {
          throw Components.Exception(
            "Update query must define fields to modify.",
            Cr.NS_ERROR_ILLEGAL_VALUE
          );
        }
        query += queryTerms + " WHERE guid = :existing_guid";
        queries.push({
          query,
          params: Object.assign({ existing_guid: guid }, params),
        });

        notifications.push(["formhistory-update", guid]);

        // Source is ignored for "update" operations, since it's not really
        // common to change the source of a value, and anyway currently this is
        // mostly used to update guids.
        break;
      }
      case "bump": {
        log("Bump form history " + change);
        if (change.guid) {
          const query =
            "UPDATE moz_formhistory " +
            "SET timesUsed = timesUsed + 1, lastUsed = :lastUsed WHERE guid = :guid";
          const queryParams = {
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
          const { query, params } = prepareInsertQuery(change, now);
          queries.push({ query, params });
          notifications.push(["formhistory-add", params.guid]);
        }

        if (change.source) {
          queries = queries.concat(
            getAddSourceToGuidQueries(change.source, change.guid)
          );
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

        const { query, params } = prepareInsertQuery(change, now);
        queries.push({ query, params });

        notifications.push(["formhistory-add", params.guid]);

        if (change.source) {
          queries = queries.concat(
            getAddSourceToGuidQueries(change.source, change.guid)
          );
        }
        break;
      }
      default: {
        // We should've already guaranteed that change.op is one of the above
        throw Components.Exception(
          "Invalid operation " + operation,
          Cr.NS_ERROR_ILLEGAL_VALUE
        );
      }
    }
  }

  try {
    await conn.executeTransaction(async () => {
      for (const { query, params } of queries) {
        await conn.executeCached(query, params);
      }
    });
    for (const [notification, param] of notifications) {
      // We're either sending a GUID or nothing at all.
      sendNotification(notification, param);
    }
  } finally {
    InProgressInserts.clear(adds);
  }
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
 * @returns {Promise} resolved once the work is complete
 */
async function expireOldEntriesDeletion(aExpireTime, aBeginningCount) {
  log(`expireOldEntriesDeletion(${aExpireTime},${aBeginningCount})`);

  await FormHistory.update([
    {
      op: "remove",
      lastUsedEnd: aExpireTime,
    },
  ]);
  await expireOldEntriesVacuum(aExpireTime, aBeginningCount);
}

/**
 * Counts number of entries removed and shrinks database as necessary.
 *
 * @param {number} aExpireTime expiration timestamp
 * @param {number} aBeginningCount number of entries at first
 */
async function expireOldEntriesVacuum(aExpireTime, aBeginningCount) {
  const count = await FormHistory.count({});
  if (aBeginningCount - count > 500) {
    log("expireOldEntriesVacuum");
    const conn = await FormHistory.db;
    await conn.executeCached("VACUUM");
  }
  sendNotification("formhistory-expireoldentries", aExpireTime);
}

async function createTable(conn, tableName) {
  const table = dbSchema.tables[tableName];
  const columns = Object.keys(table)
    .filter(col => col != "SQL")
    .map(col => [col, table[col]].join(" "))
    .join(", ");
  const no_rowid = Object.keys(table).includes("id") ? "" : "WITHOUT ROWID";
  log(`Creating table ${tableName} with ${columns}`);
  await conn.execute(
    `CREATE TABLE ${tableName} (
      ${columns}
      ${table.SQL ? "," + table.SQL : ""}
    ) ${no_rowid}`
  );
}

/**
 * Database creation and access. Used by FormHistory and some of the
 * utility functions, but is not exposed to the outside world.
 *
 * @class
 */
var DB = {
  // Once we establish a database connection, we have to hold a reference
  // to it so that it won't get GC'd.
  _instance: null,
  // MAX_ATTEMPTS is how many times we'll try to establish a connection
  // or migrate a database before giving up.
  MAX_ATTEMPTS: 4,

  /** String representing where the FormHistory database is on the filesystem */
  get path() {
    return PathUtils.join(PathUtils.profileDir, DB_FILENAME);
  },

  /**
   * Sets up and returns a connection to the FormHistory database. The
   * connection also registers itself with AsyncShutdown so that the
   * connection is closed on when the profile-before-change observer
   * notification is fired.
   *
   * @returns {Promise<OpenedConnection>}
   *        A {@link toolkit/modules/Sqlite.sys.mjs} connection to the database.
   * @throws
   *        If connecting to the database, or migrating the database
   *        failed after MAX_ATTEMPTS attempts, this will reject
   *        with the Sqlite.sys.mjs error.
   */
  get conn() {
    delete this.conn;
    const conn = (async () => {
      try {
        this._instance = await this._establishConn();
      } catch (e) {
        log("Failed to establish database connection: " + e);
        throw e;
      }

      return this._instance;
    })();

    return (this.conn = conn);
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
   * @returns {Promise<OpenedConnection>}
   *        A {@link toolkit/modules/Sqlite.sys.mjs} connection to the database.
   * @throws
   *        If connecting to the database, or migrating the database
   *        failed after MAX_ATTEMPTS attempts, this will reject
   *        with the Sqlite.sys.mjs error.
   */
  async _establishConn(attemptNum = 0) {
    log(`Establishing database connection - attempt # ${attemptNum}`);
    let conn;
    try {
      conn = await lazy.Sqlite.openConnection({ path: this.path });
      lazy.Sqlite.shutdown.addBlocker("Closing FormHistory database.", () =>
        conn.close()
      );
    } catch (e) {
      // retrying.
      // If error is a db corruption error, backup the database and create a new one.
      // Else, use an exponential backoff algorithm to restart up to MAX_ATTEMPTS times.
      if (attemptNum < this.MAX_ATTEMPTS) {
        log(`Establishing connection failed due with error ${e.result}`);

        if (e.result === Cr.NS_ERROR_FILE_CORRUPTED) {
          log("Corrupt database, resetting database");
          await this._failover(conn);
        } else {
          if (conn) {
            await conn.close();
          }
          // retrying with an exponential backoff
          await wait(2 ** attemptNum * 10);
        }

        return this._establishConn(++attemptNum);
      }

      if (conn) {
        await conn.close();
      }
      log("Establishing connection failed too many times. Giving up.");
      throw e;
    }

    try {
      // Enable foreign keys support.
      await conn.execute("PRAGMA foreign_keys = ON");

      const dbVersion = parseInt(await conn.getSchemaVersion(), 10);

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
          throw Components.Exception(
            "DB is missing expected columns",
            Cr.NS_ERROR_FILE_CORRUPTED
          );
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
        throw Components.Exception(
          "DB version is unsupported.",
          Cr.NS_ERROR_FILE_CORRUPTED
        );
      }

      if (dbVersion == 0) {
        // Case 4: New database
        await conn.executeTransaction(async () => {
          log("Creating DB -- tables");
          for (const name in dbSchema.tables) {
            await createTable(conn, name);
          }

          log("Creating DB -- indices");
          for (const name in dbSchema.indices) {
            const index = dbSchema.indices[name];
            const statement = `CREATE INDEX IF NOT EXISTS ${name} ON ${
              index.table
            }(${index.columns.join(", ")})`;
            await conn.execute(statement);
          }
        });
      } else {
        // Case 5: Old database requiring a migration
        await conn.executeTransaction(async () => {
          for (let v = dbVersion + 1; v <= DB_SCHEMA_VERSION; v++) {
            log(`Upgrading to version ${v}...`);
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
    const backupFile = this.path + ".corrupt";
    const uniquePath = await IOUtils.createUniqueFile(
      PathUtils.parent(backupFile),
      PathUtils.filename(backupFile),
      0o600
    );
    await IOUtils.copy(this.path, uniquePath);
    await IOUtils.remove(this.path);
    log("Completed DB cleanup.");
  },

  /**
   * Tests that a database connection contains the tables that we expect.
   *
   * @async
   * @param {SqliteConnection | null} conn
   *        The connection to the database that we're testing.
   * @returns {Promise<boolean>} true if all expected columns are present.
   */
  async _expectedColumnsPresent(conn) {
    for (const name in dbSchema.tables) {
      const table = dbSchema.tables[name];
      const columns = Object.keys(table).filter(col => col != "SQL");
      const query = `SELECT ${columns.join(", ")} FROM ${name}`;
      try {
        await conn.execute(query, null, (_row, cancel) => {
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

FormHistory = {
  get db() {
    return DB.conn;
  },

  get enabled() {
    return Prefs.get("enabled");
  },

  async search(aSelectTerms, aSearchData, aRowFunc) {
    // if no terms selected, select everything
    if (!aSelectTerms) {
      // Source is not a valid column in moz_formhistory.
      aSelectTerms = validFields.filter(f => f != "source");
    }

    validateSearchData(aSearchData, "Search");

    let query = `SELECT ${aSelectTerms.join(", ")} FROM moz_formhistory`;
    const { queryTerms, params } = makeQueryPredicates(aSearchData);
    if (queryTerms) {
      query += " WHERE " + queryTerms;
    }

    const allResults = [];

    const conn = await this.db;
    await conn.executeCached(query, params, row => {
      const result = {};
      for (const field of aSelectTerms) {
        result[field] = row.getResultByName(field);
      }
      aRowFunc?.(result);
      allResults.push(result);
    });

    return allResults;
  },

  async count(aSearchData) {
    validateSearchData(aSearchData, "Count");

    let query = "SELECT COUNT(*) AS numEntries FROM moz_formhistory";
    const { queryTerms, params } = makeQueryPredicates(aSearchData);
    if (queryTerms) {
      query += " WHERE " + queryTerms;
    }

    const conn = await this.db;
    const rows = await conn.executeCached(query, params);
    return rows[0].getResultByName("numEntries");
  },

  async update(aChanges) {
    function validIdentifier(change) {
      // The identifier is only valid if one of either the guid
      // or the (fieldname/value) are set (so an X-OR)
      return Boolean(change.guid) != Boolean(change.fieldname && change.value);
    }

    if (!("length" in aChanges)) {
      aChanges = [aChanges];
    }

    const isRemoveOperation = aChanges.every(change => change?.op == "remove");
    if (!this.enabled && !isRemoveOperation) {
      throw new Error(
        "Form history is disabled, only remove operations are allowed"
      );
    }

    for (const change of aChanges) {
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
              Cr.NS_ERROR_ILLEGAL_VALUE
            );
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
              Cr.NS_ERROR_ILLEGAL_VALUE
            );
          }
          break;
        case "add":
          if (change.fieldname && change.value) {
            validateOpData(change, "Add");
          } else {
            throw Components.Exception(
              "update op='add' must have a fieldname and a value.",
              Cr.NS_ERROR_ILLEGAL_VALUE
            );
          }
          break;
        default:
          throw Components.Exception(
            "update does not recognize op='" + change.op + "'",
            Cr.NS_ERROR_ILLEGAL_VALUE
          );
      }

      const results = await FormHistory.search(["guid"], {
        fieldname: change.fieldname,
        value: change.value,
      });
      if (results.length > 1) {
        const error =
          "Database contains multiple entries with the same fieldname/value pair.";
        log(error);
        throw new Error(error);
      }
      change.guid = results[0]?.guid;
    }

    await updateFormHistoryWrite(aChanges);
  },

  /**
   * Gets results for the autocomplete widget.
   *
   * @param {string} searchString The string to search for.
   * @param {object} params zero or more filter properties:
   *   - fieldname
   *   - source
   * @param {Function} [isCancelled] optional function that can return true
   *   to cancel result retrieval
   * @returns {Promise<Array>}
   *   An array of results. If the search was canceled it will be an empty array.
   */
  async getAutoCompleteResults(searchString, params, isCancelled) {
    // only do substring matching when the search string contains more than one character
    let searchTokens;
    let where = "";
    let boundaryCalc = "";

    params = {
      agedWeight: Prefs.get("agedWeight"),
      bucketSize: Prefs.get("bucketSize"),
      expiryDate:
        1000 * (Date.now() - Prefs.get("expireDays") * 24 * 60 * 60 * 1000),
      maxTimeGroupings: Prefs.get("maxTimeGroupings"),
      timeGroupingSize: Prefs.get("timeGroupingSize"),
      prefixWeight: Prefs.get("prefixWeight"),
      boundaryWeight: Prefs.get("boundaryWeight"),
      ...params,
    };

    if (searchString.length >= 1) {
      params.valuePrefix = searchString.replaceAll("/", "//") + "%";
    }

    if (searchString.length > 1) {
      searchTokens = searchString.split(/\s+/);

      // build up the word boundary and prefix match bonus calculation
      boundaryCalc =
        "MAX(1, :prefixWeight * (value LIKE :valuePrefix ESCAPE '/') + (";
      // for each word, calculate word boundary weights for the SELECT clause and
      // add word to the WHERE clause of the query
      let tokenCalc = [];
      let searchTokenCount = Math.min(searchTokens.length, MAX_SEARCH_TOKENS);
      for (let i = 0; i < searchTokenCount; i++) {
        let escapedToken = searchTokens[i].replaceAll("/", "//");
        params["tokenBegin" + i] = escapedToken + "%";
        params["tokenBoundary" + i] = "% " + escapedToken + "%";
        params["tokenContains" + i] = "%" + escapedToken + "%";

        tokenCalc.push(
          `(value LIKE :tokenBegin${i} ESCAPE '/') + (value LIKE :tokenBoundary${i} ESCAPE '/')`
        );
        where += `AND (value LIKE :tokenContains${i} ESCAPE '/') `;
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

    if (params.source) {
      where += `AND EXISTS(
        SELECT 1 FROM moz_history_to_sources
        JOIN moz_sources s ON s.id = source_id
        WHERE source = :source
          AND history_id = moz_formhistory.id
      )`;
    }

    /* Three factors in the frecency calculation for an entry (in order of use in calculation):
     * 1) average number of times used - items used more are ranked higher
     * 2) how recently it was last used - items used recently are ranked higher
     * 3) additional weight for aged entries surviving expiry - these entries are relevant
     *    since they have been used multiple times over a large time span so rank them higher
     * The score is then divided by the bucket size and we round the result so that entries
     * with a very similar frecency are bucketed together with an alphabetical sort. This is
     * to reduce the amount of moving around by entries while typing.
     */

    const query =
      "/* do not warn (bug 496471): can't use an index */ " +
      "SELECT value, guid, " +
      "ROUND( " +
      "timesUsed / MAX(1.0, (lastUsed - firstUsed) / :timeGroupingSize) * " +
      "MAX(1.0, :maxTimeGroupings - (:now - lastUsed) / :timeGroupingSize) * " +
      "MAX(1.0, :agedWeight * (firstUsed < :expiryDate)) / " +
      ":bucketSize " +
      ", 3) AS frecency, " +
      boundaryCalc +
      " AS boundaryBonuses " +
      "FROM moz_formhistory " +
      "WHERE fieldname=:fieldname " +
      where +
      "ORDER BY ROUND(frecency * boundaryBonuses) DESC, UPPER(value) ASC";

    let results = [];
    const conn = await this.db;
    await conn.executeCached(query, params, (row, cancel) => {
      if (isCancelled?.()) {
        cancel();
        results = [];
        return;
      }

      const value = row.getResultByName("value");
      const guid = row.getResultByName("guid");
      const frecency = row.getResultByName("frecency");
      const entry = {
        text: value,
        guid,
        textLowerCase: value.toLowerCase(),
        frecency,
        totalScore: Math.round(
          frecency * row.getResultByName("boundaryBonuses")
        ),
      };
      results.push(entry);
    });
    return results;
  },

  // This is used only so that the test can verify deleted table support.
  get _supportsDeletedTable() {
    return supportsDeletedTable;
  },
  set _supportsDeletedTable(val) {
    supportsDeletedTable = val;
  },

  // The remaining methods are called by FormHistoryStartup.js
  async expireOldEntries() {
    log("expireOldEntries");

    // Determine how many days of history we're supposed to keep.
    // Calculate expireTime in microseconds
    const expireTime =
      (Date.now() - Prefs.get("expireDays") * DAY_IN_MS) * 1000;

    sendNotification("formhistory-beforeexpireoldentries", expireTime);

    const count = await FormHistory.count({});
    await expireOldEntriesDeletion(expireTime, count);
  },
};

// Prevent add-ons from redefining this API
Object.freeze(FormHistory);
