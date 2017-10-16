/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AddonManager.jsm");
/* globals AddonManagerPrivate*/
Cu.import("resource://gre/modules/FileUtils.jsm");

const KEY_PROFILEDIR  = "ProfD";
const FILE_DATABASE   = "addons.sqlite";
const LAST_DB_SCHEMA   = 4;

// Add-on properties present in the columns of the database
const PROP_SINGLE = ["id", "type", "name", "version", "creator", "description",
                     "fullDescription", "developerComments", "eula",
                     "homepageURL", "supportURL", "contributionURL",
                     "contributionAmount", "averageRating", "reviewCount",
                     "reviewURL", "totalDownloads", "weeklyDownloads",
                     "dailyUsers", "sourceURI", "repositoryStatus", "size",
                     "updateDate"];

Cu.import("resource://gre/modules/Log.jsm");
const LOGGER_ID = "addons.repository.sqlmigrator";

// Create a new logger for use by the Addons Repository SQL Migrator
// (Requires AddonManager.jsm)
var logger = Log.repository.getLogger(LOGGER_ID);

this.EXPORTED_SYMBOLS = ["AddonRepository_SQLiteMigrator"];


this.AddonRepository_SQLiteMigrator = {

  /**
   * Migrates data from a previous SQLite version of the
   * database to the JSON version.
   *
   * @param structFunctions an object that contains functions
   *                        to create the various objects used
   *                        in the new JSON format
   * @param aCallback       A callback to be called when migration
   *                        finishes, with the results in an array
   * @returns bool          True if a migration will happen (DB was
   *                        found and succesfully opened)
   */
  migrate(aCallback) {
    if (!this._openConnection()) {
      this._closeConnection();
      aCallback([]);
      return false;
    }

    logger.debug("Importing addon repository from previous " + FILE_DATABASE + " storage.");

    this._retrieveStoredData((results) => {
      this._closeConnection();
      let resultArray = Object.keys(results).map(k => results[k]);
      logger.debug(resultArray.length + " addons imported.");
      aCallback(resultArray);
    });

    return true;
  },

  /**
   * Synchronously opens a new connection to the database file.
   *
   * @return bool           Whether the DB was opened successfully.
   */
  _openConnection() {
    delete this.connection;

    let dbfile = FileUtils.getFile(KEY_PROFILEDIR, [FILE_DATABASE], true);
    if (!dbfile.exists())
      return false;

    try {
      this.connection = Services.storage.openUnsharedDatabase(dbfile);
    } catch (e) {
      return false;
    }

    this.connection.executeSimpleSQL("PRAGMA locking_mode = EXCLUSIVE");

    // Any errors in here should rollback
    try {
      this.connection.beginTransaction();

      switch (this.connection.schemaVersion) {
        case 0:
          return false;

        case 1:
          logger.debug("Upgrading database schema to version 2");
          this.connection.executeSimpleSQL("ALTER TABLE screenshot ADD COLUMN width INTEGER");
          this.connection.executeSimpleSQL("ALTER TABLE screenshot ADD COLUMN height INTEGER");
          this.connection.executeSimpleSQL("ALTER TABLE screenshot ADD COLUMN thumbnailWidth INTEGER");
          this.connection.executeSimpleSQL("ALTER TABLE screenshot ADD COLUMN thumbnailHeight INTEGER");
        case 2:
          logger.debug("Upgrading database schema to version 3");
          this.connection.createTable("compatibility_override",
                                      "addon_internal_id INTEGER, " +
                                      "num INTEGER, " +
                                      "type TEXT, " +
                                      "minVersion TEXT, " +
                                      "maxVersion TEXT, " +
                                      "appID TEXT, " +
                                      "appMinVersion TEXT, " +
                                      "appMaxVersion TEXT, " +
                                      "PRIMARY KEY (addon_internal_id, num)");
        case 3:
          logger.debug("Upgrading database schema to version 4");
          this.connection.createTable("icon",
                                      "addon_internal_id INTEGER, " +
                                      "size INTEGER, " +
                                      "url TEXT, " +
                                      "PRIMARY KEY (addon_internal_id, size)");
          this._createIndices();
          this._createTriggers();
          this.connection.schemaVersion = LAST_DB_SCHEMA;
        case LAST_DB_SCHEMA:
          break;
        default:
          return false;
      }
      this.connection.commitTransaction();
    } catch (e) {
      logger.error("Failed to open " + FILE_DATABASE + ". Data import will not happen.", e);
      this.logSQLError(this.connection.lastError, this.connection.lastErrorString);
      this.connection.rollbackTransaction();
      return false;
    }

    return true;
  },

  _closeConnection() {
    for (let key in this.asyncStatementsCache) {
      let stmt = this.asyncStatementsCache[key];
      stmt.finalize();
    }
    this.asyncStatementsCache = {};

    if (this.connection)
      this.connection.asyncClose();

    delete this.connection;
  },

  /**
   * Asynchronously retrieve all add-ons from the database, and pass it
   * to the specified callback
   *
   * @param  aCallback
   *         The callback to pass the add-ons back to
   */
  _retrieveStoredData(aCallback) {
    let addons = {};

    // Retrieve all data from the addon table
    let getAllAddons = () => {
      this.getAsyncStatement("getAllAddons").executeAsync({
        handleResult: aResults => {
          let row = null;
          while ((row = aResults.getNextRow())) {
            let internal_id = row.getResultByName("internal_id");
            addons[internal_id] = this._makeAddonFromAsyncRow(row);
          }
        },

        handleError: this.asyncErrorLogger,

        handleCompletion(aReason) {
          if (aReason != Ci.mozIStorageStatementCallback.REASON_FINISHED) {
            logger.error("Error retrieving add-ons from database. Returning empty results");
            aCallback({});
            return;
          }

          getAllDevelopers();
        }
      });
    };

    // Retrieve all data from the developer table
    let getAllDevelopers = () => {
      this.getAsyncStatement("getAllDevelopers").executeAsync({
        handleResult: aResults => {
          let row = null;
          while ((row = aResults.getNextRow())) {
            let addon_internal_id = row.getResultByName("addon_internal_id");
            if (!(addon_internal_id in addons)) {
              logger.warn("Found a developer not linked to an add-on in database");
              continue;
            }

            let addon = addons[addon_internal_id];
            if (!addon.developers)
              addon.developers = [];

            addon.developers.push(this._makeDeveloperFromAsyncRow(row));
          }
        },

        handleError: this.asyncErrorLogger,

        handleCompletion(aReason) {
          if (aReason != Ci.mozIStorageStatementCallback.REASON_FINISHED) {
            logger.error("Error retrieving developers from database. Returning empty results");
            aCallback({});
            return;
          }

          getAllScreenshots();
        }
      });
    };

    // Retrieve all data from the screenshot table
    let getAllScreenshots = () => {
      this.getAsyncStatement("getAllScreenshots").executeAsync({
        handleResult: aResults => {
          let row = null;
          while ((row = aResults.getNextRow())) {
            let addon_internal_id = row.getResultByName("addon_internal_id");
            if (!(addon_internal_id in addons)) {
              logger.warn("Found a screenshot not linked to an add-on in database");
              continue;
            }

            let addon = addons[addon_internal_id];
            if (!addon.screenshots)
              addon.screenshots = [];
            addon.screenshots.push(this._makeScreenshotFromAsyncRow(row));
          }
        },

        handleError: this.asyncErrorLogger,

        handleCompletion(aReason) {
          if (aReason != Ci.mozIStorageStatementCallback.REASON_FINISHED) {
            logger.error("Error retrieving screenshots from database. Returning empty results");
            aCallback({});
            return;
          }

          getAllCompatOverrides();
        }
      });
    };

    let getAllCompatOverrides = () => {
      this.getAsyncStatement("getAllCompatOverrides").executeAsync({
        handleResult: aResults => {
          let row = null;
          while ((row = aResults.getNextRow())) {
            let addon_internal_id = row.getResultByName("addon_internal_id");
            if (!(addon_internal_id in addons)) {
              logger.warn("Found a compatibility override not linked to an add-on in database");
              continue;
            }

            let addon = addons[addon_internal_id];
            if (!addon.compatibilityOverrides)
              addon.compatibilityOverrides = [];
            addon.compatibilityOverrides.push(this._makeCompatOverrideFromAsyncRow(row));
          }
        },

        handleError: this.asyncErrorLogger,

        handleCompletion(aReason) {
          if (aReason != Ci.mozIStorageStatementCallback.REASON_FINISHED) {
            logger.error("Error retrieving compatibility overrides from database. Returning empty results");
            aCallback({});
            return;
          }

          getAllIcons();
        }
      });
    };

    let getAllIcons = () => {
      this.getAsyncStatement("getAllIcons").executeAsync({
        handleResult: aResults => {
          let row = null;
          while ((row = aResults.getNextRow())) {
            let addon_internal_id = row.getResultByName("addon_internal_id");
            if (!(addon_internal_id in addons)) {
              logger.warn("Found an icon not linked to an add-on in database");
              continue;
            }

            let addon = addons[addon_internal_id];
            let { size, url } = this._makeIconFromAsyncRow(row);
            addon.icons[size] = url;
            if (size == 32)
              addon.iconURL = url;
          }
        },

        handleError: this.asyncErrorLogger,

        handleCompletion(aReason) {
          if (aReason != Ci.mozIStorageStatementCallback.REASON_FINISHED) {
            logger.error("Error retrieving icons from database. Returning empty results");
            aCallback({});
            return;
          }

          let returnedAddons = {};
          for (let id in addons) {
            let addon = addons[id];
            returnedAddons[addon.id] = addon;
          }
          aCallback(returnedAddons);
        }
      });
    };

    // Begin asynchronous process
    getAllAddons();
  },

  // A cache of statements that are used and need to be finalized on shutdown
  asyncStatementsCache: {},

  /**
   * Gets a cached async statement or creates a new statement if it doesn't
   * already exist.
   *
   * @param  aKey
   *         A unique key to reference the statement
   * @return a mozIStorageAsyncStatement for the SQL corresponding to the
   *         unique key
   */
  getAsyncStatement(aKey) {
    if (aKey in this.asyncStatementsCache)
      return this.asyncStatementsCache[aKey];

    let sql = this.queries[aKey];
    try {
      return this.asyncStatementsCache[aKey] = this.connection.createAsyncStatement(sql);
    } catch (e) {
      logger.error("Error creating statement " + aKey + " (" + sql + ")");
      throw Components.Exception("Error creating statement " + aKey + " (" + sql + "): " + e,
                                 e.result);
    }
  },

  // The queries used by the database
  queries: {
    getAllAddons: "SELECT internal_id, id, type, name, version, " +
                  "creator, creatorURL, description, fullDescription, " +
                  "developerComments, eula, homepageURL, supportURL, " +
                  "contributionURL, contributionAmount, averageRating, " +
                  "reviewCount, reviewURL, totalDownloads, weeklyDownloads, " +
                  "dailyUsers, sourceURI, repositoryStatus, size, updateDate " +
                  "FROM addon",

    getAllDevelopers: "SELECT addon_internal_id, name, url FROM developer " +
                      "ORDER BY addon_internal_id, num",

    getAllScreenshots: "SELECT addon_internal_id, url, width, height, " +
                       "thumbnailURL, thumbnailWidth, thumbnailHeight, caption " +
                       "FROM screenshot ORDER BY addon_internal_id, num",

    getAllCompatOverrides: "SELECT addon_internal_id, type, minVersion, " +
                           "maxVersion, appID, appMinVersion, appMaxVersion " +
                           "FROM compatibility_override " +
                           "ORDER BY addon_internal_id, num",

    getAllIcons: "SELECT addon_internal_id, size, url FROM icon " +
                 "ORDER BY addon_internal_id, size",
  },

  /**
   * Make add-on structure from an asynchronous row.
   *
   * @param  aRow
   *         The asynchronous row to use
   * @return The created add-on
   */
  _makeAddonFromAsyncRow(aRow) {
    // This is intentionally not an AddonSearchResult object in order
    // to allow AddonDatabase._parseAddon to parse it, same as if it
    // was read from the JSON database.

    let addon = { icons: {} };

    for (let prop of PROP_SINGLE) {
      addon[prop] = aRow.getResultByName(prop);
    }

    return addon;
  },

  /**
   * Make a developer from an asynchronous row
   *
   * @param  aRow
   *         The asynchronous row to use
   * @return The created developer
   */
  _makeDeveloperFromAsyncRow(aRow) {
    let name = aRow.getResultByName("name");
    let url = aRow.getResultByName("url");
    return new AddonManagerPrivate.AddonAuthor(name, url);
  },

  /**
   * Make a screenshot from an asynchronous row
   *
   * @param  aRow
   *         The asynchronous row to use
   * @return The created screenshot
   */
  _makeScreenshotFromAsyncRow(aRow) {
    let url = aRow.getResultByName("url");
    let width = aRow.getResultByName("width");
    let height = aRow.getResultByName("height");
    let thumbnailURL = aRow.getResultByName("thumbnailURL");
    let thumbnailWidth = aRow.getResultByName("thumbnailWidth");
    let thumbnailHeight = aRow.getResultByName("thumbnailHeight");
    let caption = aRow.getResultByName("caption");
    return new AddonManagerPrivate.AddonScreenshot(url, width, height, thumbnailURL,
                                                   thumbnailWidth, thumbnailHeight, caption);
  },

  /**
   * Make a CompatibilityOverride from an asynchronous row
   *
   * @param  aRow
   *         The asynchronous row to use
   * @return The created CompatibilityOverride
   */
  _makeCompatOverrideFromAsyncRow(aRow) {
    let type = aRow.getResultByName("type");
    let minVersion = aRow.getResultByName("minVersion");
    let maxVersion = aRow.getResultByName("maxVersion");
    let appID = aRow.getResultByName("appID");
    let appMinVersion = aRow.getResultByName("appMinVersion");
    let appMaxVersion = aRow.getResultByName("appMaxVersion");
    return new AddonManagerPrivate.AddonCompatibilityOverride(type,
                                                              minVersion,
                                                              maxVersion,
                                                              appID,
                                                              appMinVersion,
                                                              appMaxVersion);
  },

  /**
   * Make an icon from an asynchronous row
   *
   * @param  aRow
   *         The asynchronous row to use
   * @return An object containing the size and URL of the icon
   */
  _makeIconFromAsyncRow(aRow) {
    let size = aRow.getResultByName("size");
    let url = aRow.getResultByName("url");
    return { size, url };
  },

  /**
   * A helper function to log an SQL error.
   *
   * @param  aError
   *         The storage error code associated with the error
   * @param  aErrorString
   *         An error message
   */
  logSQLError(aError, aErrorString) {
    logger.error("SQL error " + aError + ": " + aErrorString);
  },

  /**
   * A helper function to log any errors that occur during async statements.
   *
   * @param  aError
   *         A mozIStorageError to log
   */
  asyncErrorLogger(aError) {
    logger.error("Async SQL error " + aError.result + ": " + aError.message);
  },

  /**
   * Synchronously creates the triggers in the database.
   */
  _createTriggers() {
    this.connection.executeSimpleSQL("DROP TRIGGER IF EXISTS delete_addon");
    this.connection.executeSimpleSQL("CREATE TRIGGER delete_addon AFTER DELETE " +
      "ON addon BEGIN " +
      "DELETE FROM developer WHERE addon_internal_id=old.internal_id; " +
      "DELETE FROM screenshot WHERE addon_internal_id=old.internal_id; " +
      "DELETE FROM compatibility_override WHERE addon_internal_id=old.internal_id; " +
      "DELETE FROM icon WHERE addon_internal_id=old.internal_id; " +
      "END");
  },

  /**
   * Synchronously creates the indices in the database.
   */
  _createIndices() {
    this.connection.executeSimpleSQL("CREATE INDEX IF NOT EXISTS developer_idx " +
                                     "ON developer (addon_internal_id)");
    this.connection.executeSimpleSQL("CREATE INDEX IF NOT EXISTS screenshot_idx " +
                                     "ON screenshot (addon_internal_id)");
    this.connection.executeSimpleSQL("CREATE INDEX IF NOT EXISTS compatibility_override_idx " +
                                     "ON compatibility_override (addon_internal_id)");
    this.connection.executeSimpleSQL("CREATE INDEX IF NOT EXISTS icon_idx " +
                                     "ON icon (addon_internal_id)");
  }
};
