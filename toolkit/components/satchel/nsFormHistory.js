/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Deprecated",
                                  "resource://gre/modules/Deprecated.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");

const DB_VERSION = 4;
const DAY_IN_MS  = 86400000; // 1 day in milliseconds

function FormHistory() {
    Deprecated.warning(
        "nsIFormHistory2 is deprecated and will be removed in a future version",
        "https://bugzilla.mozilla.org/show_bug.cgi?id=879118");
    this.init();
}

FormHistory.prototype = {
    classID          : Components.ID("{0c1bb408-71a2-403f-854a-3a0659829ded}"),
    QueryInterface   : XPCOMUtils.generateQI([Ci.nsIFormHistory2,
                                              Ci.nsIObserver,
                                              Ci.nsIMessageListener,
                                              Ci.nsISupportsWeakReference,
                                              ]),

    debug          : true,
    enabled        : true,

    // The current database schema.
    dbSchema : {
        tables : {
            moz_formhistory: {
                "id"        : "INTEGER PRIMARY KEY",
                "fieldname" : "TEXT NOT NULL",
                "value"     : "TEXT NOT NULL",
                "timesUsed" : "INTEGER",
                "firstUsed" : "INTEGER",
                "lastUsed"  : "INTEGER",
                "guid"      : "TEXT"
            },
            moz_deleted_formhistory: {
                "id"          : "INTEGER PRIMARY KEY",
                "timeDeleted" : "INTEGER",
                "guid"        : "TEXT"
            }
        },
        indices : {
            moz_formhistory_index : {
                table   : "moz_formhistory",
                columns : ["fieldname"]
            },
            moz_formhistory_lastused_index : {
                table   : "moz_formhistory",
                columns : ["lastUsed"]
            },
            moz_formhistory_guid_index : {
                table   : "moz_formhistory",
                columns : ["guid"]
            },
        }
    },
    dbStmts      : null,  // Database statements for memoization
    dbFile       : null,

    _uuidService: null,
    get uuidService() {
        if (!this._uuidService)
            this._uuidService = Cc["@mozilla.org/uuid-generator;1"].
                                getService(Ci.nsIUUIDGenerator);
        return this._uuidService;
    },

    log : function log(message) {
        if (!this.debug)
            return;
        dump("FormHistory: " + message + "\n");
        Services.console.logStringMessage("FormHistory: " + message);
    },


    init : function init() {
        this.updatePrefs();

        this.dbStmts = {};

        // Add observer
        Services.obs.addObserver(this, "profile-before-change", true);
    },

    /* ---- nsIFormHistory2 interfaces ---- */


    get hasEntries() {
        return (this.countAllEntries() > 0);
    },


    addEntry : function addEntry(name, value) {
        if (!this.enabled)
            return;

        this.log("addEntry for " + name + "=" + value);

        let now = Date.now() * 1000; // microseconds

        let [id, guid] = this.getExistingEntryID(name, value);
        let stmt;

        if (id != -1) {
            // Update existing entry.
            let query = "UPDATE moz_formhistory SET timesUsed = timesUsed + 1, lastUsed = :lastUsed WHERE id = :id";
            let params = {
                            lastUsed : now,
                            id
                         };

            try {
                stmt = this.dbCreateStatement(query, params);
                stmt.execute();
                this.sendStringNotification("modifyEntry", name, value, guid);
            } catch (e) {
                this.log("addEntry (modify) failed: " + e);
                throw e;
            } finally {
                if (stmt) {
                    stmt.reset();
                }
            }

        } else {
            // Add new entry.
            guid = this.generateGUID();

            let query = "INSERT INTO moz_formhistory (fieldname, value, timesUsed, firstUsed, lastUsed, guid) " +
                            "VALUES (:fieldname, :value, :timesUsed, :firstUsed, :lastUsed, :guid)";
            let params = {
                            fieldname : name,
                            value,
                            timesUsed : 1,
                            firstUsed : now,
                            lastUsed  : now,
                            guid
                        };

            try {
                stmt = this.dbCreateStatement(query, params);
                stmt.execute();
                this.sendStringNotification("addEntry", name, value, guid);
            } catch (e) {
                this.log("addEntry (create) failed: " + e);
                throw e;
            } finally {
                if (stmt) {
                    stmt.reset();
                }
            }
        }
    },


    removeEntry : function removeEntry(name, value) {
        this.log("removeEntry for " + name + "=" + value);

        let [id, guid] = this.getExistingEntryID(name, value);
        this.sendStringNotification("before-removeEntry", name, value, guid);

        let stmt;
        let query = "DELETE FROM moz_formhistory WHERE id = :id";
        let params = { id };
        let existingTransactionInProgress;

        try {
            // Don't start a transaction if one is already in progress since we can't nest them.
            existingTransactionInProgress = this.dbConnection.transactionInProgress;
            if (!existingTransactionInProgress)
                this.dbConnection.beginTransaction();
            this.moveToDeletedTable("VALUES (:guid, :timeDeleted)", {
              guid,
              timeDeleted: Date.now()
            });

            // remove from the formhistory database
            stmt = this.dbCreateStatement(query, params);
            stmt.execute();
            this.sendStringNotification("removeEntry", name, value, guid);
        } catch (e) {
            if (!existingTransactionInProgress)
                this.dbConnection.rollbackTransaction();
            this.log("removeEntry failed: " + e);
            throw e;
        } finally {
            if (stmt) {
                stmt.reset();
            }
        }
        if (!existingTransactionInProgress)
            this.dbConnection.commitTransaction();
    },


    removeEntriesForName : function removeEntriesForName(name) {
        this.log("removeEntriesForName with name=" + name);

        this.sendStringNotification("before-removeEntriesForName", name);

        let stmt;
        let query = "DELETE FROM moz_formhistory WHERE fieldname = :fieldname";
        let params = { fieldname : name };
        let existingTransactionInProgress;

        try {
            // Don't start a transaction if one is already in progress since we can't nest them.
            existingTransactionInProgress = this.dbConnection.transactionInProgress;
            if (!existingTransactionInProgress)
                this.dbConnection.beginTransaction();
            this.moveToDeletedTable(
              "SELECT guid, :timeDeleted FROM moz_formhistory " +
              "WHERE fieldname = :fieldname", {
                fieldname: name,
                timeDeleted: Date.now()
            });

            stmt = this.dbCreateStatement(query, params);
            stmt.execute();
            this.sendStringNotification("removeEntriesForName", name);
        } catch (e) {
            if (!existingTransactionInProgress)
                this.dbConnection.rollbackTransaction();
            this.log("removeEntriesForName failed: " + e);
            throw e;
        } finally {
            if (stmt) {
                stmt.reset();
            }
        }
        if (!existingTransactionInProgress)
            this.dbConnection.commitTransaction();
    },


    removeAllEntries : function removeAllEntries() {
        this.log("removeAllEntries");

        this.sendNotification("before-removeAllEntries", null);

        let stmt;
        let query = "DELETE FROM moz_formhistory";
        let existingTransactionInProgress;

        try {
            // Don't start a transaction if one is already in progress since we can't nest them.
            existingTransactionInProgress = this.dbConnection.transactionInProgress;
            if (!existingTransactionInProgress)
                this.dbConnection.beginTransaction();
            // TODO: Add these items to the deleted items table once we've sorted
            //       out the issues from bug 756701
            stmt = this.dbCreateStatement(query);
            stmt.execute();
            this.sendNotification("removeAllEntries", null);
        } catch (e) {
            if (!existingTransactionInProgress)
                this.dbConnection.rollbackTransaction();
            this.log("removeAllEntries failed: " + e);
            throw e;
        } finally {
            if (stmt) {
                stmt.reset();
            }
        }
        if (!existingTransactionInProgress)
            this.dbConnection.commitTransaction();
    },


    nameExists : function nameExists(name) {
        this.log("nameExists for name=" + name);
        let stmt;
        let query = "SELECT COUNT(1) AS numEntries FROM moz_formhistory WHERE fieldname = :fieldname";
        let params = { fieldname : name };
        try {
            stmt = this.dbCreateStatement(query, params);
            stmt.executeStep();
            return (stmt.row.numEntries > 0);
        } catch (e) {
            this.log("nameExists failed: " + e);
            throw e;
        } finally {
            if (stmt) {
                stmt.reset();
            }
        }
    },

    entryExists : function entryExists(name, value) {
        this.log("entryExists for " + name + "=" + value);
        let [id] = this.getExistingEntryID(name, value);
        this.log("entryExists: id=" + id);
        return (id != -1);
    },

    removeEntriesByTimeframe : function removeEntriesByTimeframe(beginTime, endTime) {
        this.log("removeEntriesByTimeframe for " + beginTime + " to " + endTime);

        this.sendIntNotification("before-removeEntriesByTimeframe", beginTime, endTime);

        let stmt;
        let query = "DELETE FROM moz_formhistory WHERE firstUsed >= :beginTime AND firstUsed <= :endTime";
        let params = {
                        beginTime,
                        endTime
                     };
        let existingTransactionInProgress;

        try {
            // Don't start a transaction if one is already in progress since we can't nest them.
            existingTransactionInProgress = this.dbConnection.transactionInProgress;
            if (!existingTransactionInProgress)
                this.dbConnection.beginTransaction();
            this.moveToDeletedTable(
                  "SELECT guid, :timeDeleted FROM moz_formhistory " +
                  "WHERE firstUsed >= :beginTime AND firstUsed <= :endTime", {
              beginTime,
              endTime
            });

            stmt = this.dbCreateStatement(query, params);
            stmt.executeStep();
            this.sendIntNotification("removeEntriesByTimeframe", beginTime, endTime);
        } catch (e) {
            if (!existingTransactionInProgress)
                this.dbConnection.rollbackTransaction();
            this.log("removeEntriesByTimeframe failed: " + e);
            throw e;
        } finally {
            if (stmt) {
                stmt.reset();
            }
        }
        if (!existingTransactionInProgress)
            this.dbConnection.commitTransaction();
    },

    moveToDeletedTable : function moveToDeletedTable(values, params) {
        if (AppConstants.platform == "android") {
            this.log("Moving entries to deleted table.");

            let stmt;

            try {
                // Move the entries to the deleted items table.
                let query = "INSERT INTO moz_deleted_formhistory (guid, timeDeleted) ";
                if (values) query += values;
                stmt = this.dbCreateStatement(query, params);
                stmt.execute();
            } catch (e) {
                this.log("Moving deleted entries failed: " + e);
                throw e;
            } finally {
                if (stmt) {
                    stmt.reset();
                }
            }
        }
    },

    get dbConnection() {
        // Make sure dbConnection can't be called from now to prevent infinite loops.
        delete FormHistory.prototype.dbConnection;

        try {
            this.dbFile = Services.dirsvc.get("ProfD", Ci.nsIFile).clone();
            this.dbFile.append("formhistory.sqlite");
            this.log("Opening database at " + this.dbFile.path);

            FormHistory.prototype.dbConnection = this.dbOpen();
            this.dbInit();
        } catch (e) {
            this.log("Initialization failed: " + e);
            // If dbInit fails...
            if (e.result == Cr.NS_ERROR_FILE_CORRUPTED) {
                this.dbCleanup();
                FormHistory.prototype.dbConnection = this.dbOpen();
                this.dbInit();
            } else {
                throw "Initialization failed";
            }
        }

        return FormHistory.prototype.dbConnection;
    },

    get DBConnection() {
        return this.dbConnection;
    },


    /* ---- nsIObserver interface ---- */


    observe : function observe(subject, topic, data) {
        switch (topic) {
        case "nsPref:changed":
            this.updatePrefs();
            break;
        case "profile-before-change":
            this._dbClose(false);
            break;
        default:
            this.log("Oops! Unexpected notification: " + topic);
            break;
        }
    },


    /* ---- helpers ---- */


    generateGUID() {
        // string like: "{f60d9eac-9421-4abc-8491-8e8322b063d4}"
        let uuid = this.uuidService.generateUUID().toString();
        let raw = ""; // A string with the low bytes set to random values
        let bytes = 0;
        for (let i = 1; bytes < 12 ; i += 2) {
            // Skip dashes
            if (uuid[i] == "-")
                i++;
            let hexVal = parseInt(uuid[i] + uuid[i + 1], 16);
            raw += String.fromCharCode(hexVal);
            bytes++;
        }
        return btoa(raw);
    },


    sendStringNotification(changeType, str1, str2, str3) {
        function wrapit(str) {
            let wrapper = Cc["@mozilla.org/supports-string;1"].
                          createInstance(Ci.nsISupportsString);
            wrapper.data = str;
            return wrapper;
        }

        let strData;
        if (arguments.length == 2) {
            // Just 1 string, no need to put it in an array
            strData = wrapit(str1);
        } else {
            // 3 strings, put them in an array.
            strData = Cc["@mozilla.org/array;1"].
                      createInstance(Ci.nsIMutableArray);
            strData.appendElement(wrapit(str1), false);
            strData.appendElement(wrapit(str2), false);
            strData.appendElement(wrapit(str3), false);
        }
        this.sendNotification(changeType, strData);
    },


    sendIntNotification(changeType, int1, int2) {
        function wrapit(int) {
            let wrapper = Cc["@mozilla.org/supports-PRInt64;1"].
                          createInstance(Ci.nsISupportsPRInt64);
            wrapper.data = int;
            return wrapper;
        }

        let intData;
        if (arguments.length == 2) {
            // Just 1 int, no need for an array
            intData = wrapit(int1);
        } else {
            // 2 ints, put them in an array.
            intData = Cc["@mozilla.org/array;1"].
                      createInstance(Ci.nsIMutableArray);
            intData.appendElement(wrapit(int1), false);
            intData.appendElement(wrapit(int2), false);
        }
        this.sendNotification(changeType, intData);
    },


    sendNotification(changeType, data) {
        Services.obs.notifyObservers(data, "satchel-storage-changed", changeType);
    },


    getExistingEntryID(name, value) {
        let id = -1, guid = null;
        let stmt;
        let query = "SELECT id, guid FROM moz_formhistory WHERE fieldname = :fieldname AND value = :value";
        let params = {
                        fieldname : name,
                        value
                     };
        try {
            stmt = this.dbCreateStatement(query, params);
            if (stmt.executeStep()) {
                id   = stmt.row.id;
                guid = stmt.row.guid;
            }
        } catch (e) {
            this.log("getExistingEntryID failed: " + e);
            throw e;
        } finally {
            if (stmt) {
                stmt.reset();
            }
        }

        return [id, guid];
    },


    countAllEntries() {
        let query = "SELECT COUNT(1) AS numEntries FROM moz_formhistory";

        let stmt, numEntries;
        try {
            stmt = this.dbCreateStatement(query, null);
            stmt.executeStep();
            numEntries = stmt.row.numEntries;
        } catch (e) {
            this.log("countAllEntries failed: " + e);
            throw e;
        } finally {
            if (stmt) {
                stmt.reset();
            }
        }

        this.log("countAllEntries: counted entries: " + numEntries);
        return numEntries;
    },


    updatePrefs() {
        this.debug          = Services.prefs.getBoolPref("browser.formfill.debug");
        this.enabled        = Services.prefs.getBoolPref("browser.formfill.enable");
    },

    // Database Creation & Access

    /*
     * dbCreateStatement
     *
     * Creates a statement, wraps it, and then does parameter replacement
     * Will use memoization so that statements can be reused.
     */
    dbCreateStatement(query, params) {
        let stmt = this.dbStmts[query];
        // Memoize the statements
        if (!stmt) {
            this.log("Creating new statement for query: " + query);
            stmt = this.dbConnection.createStatement(query);
            this.dbStmts[query] = stmt;
        }
        // Replace parameters, must be done 1 at a time
        if (params)
            for (let i in params)
                stmt.params[i] = params[i];
        return stmt;
    },

    /*
     * dbOpen
     *
     * Open a connection with the database and returns it.
     *
     * @returns a db connection object.
     */
    dbOpen() {
        this.log("Open Database");

        let storage = Cc["@mozilla.org/storage/service;1"].
                      getService(Ci.mozIStorageService);
        return storage.openDatabase(this.dbFile);
    },

    /*
     * dbInit
     *
     * Attempts to initialize the database. This creates the file if it doesn't
     * exist, performs any migrations, etc.
     */
    dbInit() {
        this.log("Initializing Database");

        let version = this.dbConnection.schemaVersion;

        // Note: Firefox 3 didn't set a schema value, so it started from 0.
        // So we can't depend on a simple version == 0 check
        if (version == 0 && !this.dbConnection.tableExists("moz_formhistory"))
            this.dbCreate();
        else if (version != DB_VERSION)
            this.dbMigrate(version);
    },


    dbCreate() {
        this.log("Creating DB -- tables");
        for (let name in this.dbSchema.tables) {
            let table = this.dbSchema.tables[name];
            this.dbCreateTable(name, table);
        }

        this.log("Creating DB -- indices");
        for (let name in this.dbSchema.indices) {
            let index = this.dbSchema.indices[name];
            let statement = "CREATE INDEX IF NOT EXISTS " + name + " ON " + index.table +
                            "(" + index.columns.join(", ") + ")";
            this.dbConnection.executeSimpleSQL(statement);
        }

        this.dbConnection.schemaVersion = DB_VERSION;
    },

    dbCreateTable(name, table) {
        let tSQL = Object.keys(table).map(col => [col, table[col]].join(" ")).join(", ");
        this.log("Creating table " + name + " with " + tSQL);
        this.dbConnection.createTable(name, tSQL);
    },

    dbMigrate(oldVersion) {
        this.log("Attempting to migrate from version " + oldVersion);

        if (oldVersion > DB_VERSION) {
            this.log("Downgrading to version " + DB_VERSION);
            // User's DB is newer. Sanity check that our expected columns are
            // present, and if so mark the lower version and merrily continue
            // on. If the columns are borked, something is wrong so blow away
            // the DB and start from scratch. [Future incompatible upgrades
            // should swtich to a different table or file.]

            if (!this.dbAreExpectedColumnsPresent())
                throw Components.Exception("DB is missing expected columns",
                                           Cr.NS_ERROR_FILE_CORRUPTED);

            // Change the stored version to the current version. If the user
            // runs the newer code again, it will see the lower version number
            // and re-upgrade (to fixup any entries the old code added).
            this.dbConnection.schemaVersion = DB_VERSION;
            return;
        }

        // Upgrade to newer version...

        this.dbConnection.beginTransaction();

        try {
            for (let v = oldVersion + 1; v <= DB_VERSION; v++) {
                this.log("Upgrading to version " + v + "...");
                let migrateFunction = "dbMigrateToVersion" + v;
                this[migrateFunction]();
            }
        } catch (e) {
            this.log("Migration failed: "  + e);
            this.dbConnection.rollbackTransaction();
            throw e;
        }

        this.dbConnection.schemaVersion = DB_VERSION;
        this.dbConnection.commitTransaction();
        this.log("DB migration completed.");
    },


    /*
     * dbMigrateToVersion1
     *
     * Updates the DB schema to v1 (bug 463154).
     * Adds firstUsed, lastUsed, timesUsed columns.
     */
    dbMigrateToVersion1() {
        // Check to see if the new columns already exist (could be a v1 DB that
        // was downgraded to v0). If they exist, we don't need to add them.
        let query;
        ["timesUsed", "firstUsed", "lastUsed"].forEach(function(column) {
            if (!this.dbColumnExists(column)) {
                query = "ALTER TABLE moz_formhistory ADD COLUMN " + column + " INTEGER";
                this.dbConnection.executeSimpleSQL(query);
            }
        }, this);

        // Set the default values for the new columns.
        //
        // Note that we set the timestamps to 24 hours in the past. We want a
        // timestamp that's recent (so that "keep form history for 90 days"
        // doesn't expire things surprisingly soon), but not so recent that
        // "forget the last hour of stuff" deletes all freshly migrated data.
        let stmt;
        query = "UPDATE moz_formhistory " +
                "SET timesUsed = 1, firstUsed = :time, lastUsed = :time " +
                "WHERE timesUsed isnull OR firstUsed isnull or lastUsed isnull";
        let params = { time: (Date.now() - DAY_IN_MS) * 1000 }
        try {
            stmt = this.dbCreateStatement(query, params);
            stmt.execute();
        } catch (e) {
            this.log("Failed setting timestamps: " + e);
            throw e;
        } finally {
            if (stmt) {
                stmt.reset();
            }
        }
    },


    /*
     * dbMigrateToVersion2
     *
     * Updates the DB schema to v2 (bug 243136).
     * Adds lastUsed index, removes moz_dummy_table
     */
    dbMigrateToVersion2() {
        let query = "DROP TABLE IF EXISTS moz_dummy_table";
        this.dbConnection.executeSimpleSQL(query);

        query = "CREATE INDEX IF NOT EXISTS moz_formhistory_lastused_index ON moz_formhistory (lastUsed)";
        this.dbConnection.executeSimpleSQL(query);
    },


    /*
     * dbMigrateToVersion3
     *
     * Updates the DB schema to v3 (bug 506402).
     * Adds guid column and index.
     */
    dbMigrateToVersion3() {
        // Check to see if GUID column already exists, add if needed
        let query;
        if (!this.dbColumnExists("guid")) {
            query = "ALTER TABLE moz_formhistory ADD COLUMN guid TEXT";
            this.dbConnection.executeSimpleSQL(query);

            query = "CREATE INDEX IF NOT EXISTS moz_formhistory_guid_index ON moz_formhistory (guid)";
            this.dbConnection.executeSimpleSQL(query);
        }

        // Get a list of IDs for existing logins
        let ids = [];
        query = "SELECT id FROM moz_formhistory WHERE guid isnull";
        let stmt;
        try {
            stmt = this.dbCreateStatement(query);
            while (stmt.executeStep())
                ids.push(stmt.row.id);
        } catch (e) {
            this.log("Failed getting IDs: " + e);
            throw e;
        } finally {
            if (stmt) {
                stmt.reset();
            }
        }

        // Generate a GUID for each login and update the DB.
        query = "UPDATE moz_formhistory SET guid = :guid WHERE id = :id";
        for (let id of ids) {
            let params = {
                id,
                guid : this.generateGUID()
            };

            try {
                stmt = this.dbCreateStatement(query, params);
                stmt.execute();
            } catch (e) {
                this.log("Failed setting GUID: " + e);
                throw e;
            } finally {
                if (stmt) {
                    stmt.reset();
                }
            }
        }
    },

    dbMigrateToVersion4() {
        if (!this.dbConnection.tableExists("moz_deleted_formhistory")) {
            this.dbCreateTable("moz_deleted_formhistory", this.dbSchema.tables.moz_deleted_formhistory);
        }
    },

    /*
     * dbAreExpectedColumnsPresent
     *
     * Sanity check to ensure that the columns this version of the code expects
     * are present in the DB we're using.
     */
    dbAreExpectedColumnsPresent() {
        for (let name in this.dbSchema.tables) {
            let table = this.dbSchema.tables[name];
            let query = "SELECT " +
                        Object.keys(table).join(", ") +
                        " FROM " + name;
            try {
                let stmt = this.dbConnection.createStatement(query);
                // (no need to execute statement, if it compiled we're good)
                stmt.finalize();
            } catch (e) {
                return false;
            }
        }

        this.log("verified that expected columns are present in DB.");
        return true;
    },


    /*
     * dbColumnExists
     *
     * Checks to see if the named column already exists.
     */
    dbColumnExists(columnName) {
        let query = "SELECT " + columnName + " FROM moz_formhistory";
        try {
            let stmt = this.dbConnection.createStatement(query);
            // (no need to execute statement, if it compiled we're good)
            stmt.finalize();
            return true;
        } catch (e) {
            return false;
        }
    },

    /**
     * _dbClose
     *
     * Finalize all statements and close the connection.
     *
      * @param aBlocking - Should we spin the loop waiting for the db to be
      *                    closed.
     */
    _dbClose : function FH__dbClose(aBlocking) {
        for (let query in this.dbStmts) {
            let stmt = this.dbStmts[query];
            stmt.finalize();
        }
        this.dbStmts = {};

        let connectionDescriptor = Object.getOwnPropertyDescriptor(FormHistory.prototype, "dbConnection");
        // Return if the database hasn't been opened.
        if (!connectionDescriptor || connectionDescriptor.value === undefined)
            return;

        let completed = false;
        try {
            this.dbConnection.asyncClose(function() { completed = true; });
        } catch (e) {
            completed = true;
            Components.utils.reportError(e);
        }

        let thread = Services.tm.currentThread;
        while (aBlocking && !completed) {
            thread.processNextEvent(true);
        }
    },

    /*
     * dbCleanup
     *
     * Called when database creation fails. Finalizes database statements,
     * closes the database connection, deletes the database file.
     */
    dbCleanup() {
        this.log("Cleaning up DB file - close & remove & backup")

        // Create backup file
        let storage = Cc["@mozilla.org/storage/service;1"].
                      getService(Ci.mozIStorageService);
        let backupFile = this.dbFile.leafName + ".corrupt";
        storage.backupDatabaseFile(this.dbFile, backupFile);

        this._dbClose(true);
        this.dbFile.remove(false);
    }
};

var component = [FormHistory];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(component);
