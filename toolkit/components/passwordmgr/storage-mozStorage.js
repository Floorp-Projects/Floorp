/* -*- tab-width: 4; indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* vim: set sw=4 ts=4 et lcs=trail\:.,tab\:>~ : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

const DB_VERSION = 5; // The database schema version

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/Promise.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LoginHelper",
                                  "resource://gre/modules/LoginHelper.jsm");

/**
 * Object that manages a database transaction properly so consumers don't have
 * to worry about it throwing.
 *
 * @param aDatabase
 *        The mozIStorageConnection to start a transaction on.
 */
function Transaction(aDatabase) {
    this._db = aDatabase;

    this._hasTransaction = false;
    try {
        this._db.beginTransaction();
        this._hasTransaction = true;
    }
    catch(e) { /* om nom nom exceptions */ }
}

Transaction.prototype = {
    commit : function() {
        if (this._hasTransaction)
            this._db.commitTransaction();
    },

    rollback : function() {
        if (this._hasTransaction)
            this._db.rollbackTransaction();
    },
};


function LoginManagerStorage_mozStorage() { };

LoginManagerStorage_mozStorage.prototype = {

    classID : Components.ID("{8c2023b9-175c-477e-9761-44ae7b549756}"),
    QueryInterface : XPCOMUtils.generateQI([Ci.nsILoginManagerStorage,
                                            Ci.nsIInterfaceRequestor]),
    getInterface : function(aIID) {
        if (aIID.equals(Ci.nsIVariant)) {
            // Allows unwrapping the JavaScript object for regression tests.
            return this;
        }

        if (aIID.equals(Ci.mozIStorageConnection)) {
            return this._dbConnection;
        }

        throw Cr.NS_ERROR_NO_INTERFACE;
    },

    __crypto : null,  // nsILoginManagerCrypto service
    get _crypto() {
        if (!this.__crypto)
            this.__crypto = Cc["@mozilla.org/login-manager/crypto/SDR;1"].
                            getService(Ci.nsILoginManagerCrypto);
        return this.__crypto;
    },

    __profileDir: null,  // nsIFile for the user's profile dir
    get _profileDir() {
        if (!this.__profileDir)
            this.__profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
        return this.__profileDir;
    },

    __storageService: null, // Storage service for using mozStorage
    get _storageService() {
        if (!this.__storageService)
            this.__storageService = Cc["@mozilla.org/storage/service;1"].
                                    getService(Ci.mozIStorageService);
        return this.__storageService;
    },

    __uuidService: null,
    get _uuidService() {
        if (!this.__uuidService)
            this.__uuidService = Cc["@mozilla.org/uuid-generator;1"].
                                 getService(Ci.nsIUUIDGenerator);
        return this.__uuidService;
    },


    // The current database schema.
    _dbSchema: {
        tables: {
            moz_logins:         "id                  INTEGER PRIMARY KEY," +
                                "hostname            TEXT NOT NULL,"       +
                                "httpRealm           TEXT,"                +
                                "formSubmitURL       TEXT,"                +
                                "usernameField       TEXT NOT NULL,"       +
                                "passwordField       TEXT NOT NULL,"       +
                                "encryptedUsername   TEXT NOT NULL,"       +
                                "encryptedPassword   TEXT NOT NULL,"       +
                                "guid                TEXT,"                +
                                "encType             INTEGER,"             +
                                "timeCreated         INTEGER,"             +
                                "timeLastUsed        INTEGER,"             +
                                "timePasswordChanged INTEGER,"             +
                                "timesUsed           INTEGER",
            // Changes must be reflected in this._dbAreExpectedColumnsPresent(),
            // this._searchLogins(), and this.modifyLogin().

            moz_disabledHosts:  "id                 INTEGER PRIMARY KEY," +
                                "hostname           TEXT UNIQUE ON CONFLICT REPLACE",

            moz_deleted_logins: "id                  INTEGER PRIMARY KEY," +
                                "guid                TEXT,"                +
                                "timeDeleted         INTEGER",
        },
        indices: {
          moz_logins_hostname_index: {
            table: "moz_logins",
            columns: ["hostname"]
          },
          moz_logins_hostname_formSubmitURL_index: {
            table: "moz_logins",
            columns: ["hostname", "formSubmitURL"]
          },
          moz_logins_hostname_httpRealm_index: {
              table: "moz_logins",
              columns: ["hostname", "httpRealm"]
          },
          moz_logins_guid_index: {
              table: "moz_logins",
              columns: ["guid"]
          },
          moz_logins_encType_index: {
              table: "moz_logins",
              columns: ["encType"]
          }
        }
    },
    _dbConnection : null,  // The database connection
    _dbStmts      : null,  // Database statements for memoization

    _prefBranch   : null,  // Preferences service
    _signonsFile  : null,  // nsIFile for "signons.sqlite"
    _debug        : false, // mirrors signon.debug


    /*
     * log
     *
     * Internal function for logging debug messages to the Error Console.
     */
    log : function (message) {
        if (!this._debug)
            return;
        dump("PwMgr mozStorage: " + message + "\n");
        Services.console.logStringMessage("PwMgr mozStorage: " + message);
    },


    /*
     * Internal method used by regression tests only.  It overrides the default
     * database location.
     */
    initWithFile : function(aDBFile) {
        if (aDBFile)
            this._signonsFile = aDBFile;

        this.initialize();
    },


    /*
     * initialize
     *
     */
    initialize : function () {
        this._dbStmts = {};

        // Connect to the correct preferences branch.
        this._prefBranch = Services.prefs.getBranch("signon.");
        this._debug = this._prefBranch.getBoolPref("debug");

        let isFirstRun;
        try {
            // Force initialization of the crypto module.
            // See bug 717490 comment 17.
            this._crypto;

            // If initWithFile is calling us, _signonsFile may already be set.
            if (!this._signonsFile) {
                // Initialize signons.sqlite
                this._signonsFile = this._profileDir.clone();
                this._signonsFile.append("signons.sqlite");
            }
            this.log("Opening database at " + this._signonsFile.path);

            // Initialize the database (create, migrate as necessary)
            isFirstRun = this._dbInit();

            this._initialized = true;

            return Promise.resolve();
        } catch (e) {
            this.log("Initialization failed: " + e);
            // If the import fails on first run, we want to delete the db
            if (isFirstRun && e == "Import failed")
                this._dbCleanup(false);
            throw "Initialization failed";
        }
    },


    /*
     * terminate
     *
     * Internal method used by regression tests only.  It is called before
     * replacing this storage module with a new instance.
     */
    terminate : function () {
        return Promise.resolve();
    },


    /*
     * addLogin
     *
     */
    addLogin : function (login) {
        // Throws if there are bogus values.
        LoginHelper.checkLoginValues(login);

        let [encUsername, encPassword, encType] = this._encryptLogin(login);

        // Clone the login, so we don't modify the caller's object.
        let loginClone = login.clone();

        // Initialize the nsILoginMetaInfo fields, unless the caller gave us values
        loginClone.QueryInterface(Ci.nsILoginMetaInfo);
        if (loginClone.guid) {
            if (!this._isGuidUnique(loginClone.guid))
                throw "specified GUID already exists";
        } else {
            loginClone.guid = this._uuidService.generateUUID().toString();
        }

        // Set timestamps
        let currentTime = Date.now();
        if (!loginClone.timeCreated)
            loginClone.timeCreated = currentTime;
        if (!loginClone.timeLastUsed)
            loginClone.timeLastUsed = currentTime;
        if (!loginClone.timePasswordChanged)
            loginClone.timePasswordChanged = currentTime;
        if (!loginClone.timesUsed)
            loginClone.timesUsed = 1;

        let query =
            "INSERT INTO moz_logins " +
            "(hostname, httpRealm, formSubmitURL, usernameField, " +
             "passwordField, encryptedUsername, encryptedPassword, " +
             "guid, encType, timeCreated, timeLastUsed, timePasswordChanged, " +
             "timesUsed) " +
            "VALUES (:hostname, :httpRealm, :formSubmitURL, :usernameField, " +
                    ":passwordField, :encryptedUsername, :encryptedPassword, " +
                    ":guid, :encType, :timeCreated, :timeLastUsed, " +
                    ":timePasswordChanged, :timesUsed)";

        let params = {
            hostname:            loginClone.hostname,
            httpRealm:           loginClone.httpRealm,
            formSubmitURL:       loginClone.formSubmitURL,
            usernameField:       loginClone.usernameField,
            passwordField:       loginClone.passwordField,
            encryptedUsername:   encUsername,
            encryptedPassword:   encPassword,
            guid:                loginClone.guid,
            encType:             encType,
            timeCreated:         loginClone.timeCreated,
            timeLastUsed:        loginClone.timeLastUsed,
            timePasswordChanged: loginClone.timePasswordChanged,
            timesUsed:           loginClone.timesUsed
        };

        let stmt;
        try {
            stmt = this._dbCreateStatement(query, params);
            stmt.execute();
        } catch (e) {
            this.log("addLogin failed: " + e.name + " : " + e.message);
            throw "Couldn't write to database, login not added.";
        } finally {
            if (stmt) {
                stmt.reset();
            }
        }

        // Send a notification that a login was added.
        this._sendNotification("addLogin", loginClone);
    },


    /*
     * removeLogin
     *
     */
    removeLogin : function (login) {
        let [idToDelete, storedLogin] = this._getIdForLogin(login);
        if (!idToDelete)
            throw "No matching logins";

        // Execute the statement & remove from DB
        let query  = "DELETE FROM moz_logins WHERE id = :id";
        let params = { id: idToDelete };
        let stmt;
        let transaction = new Transaction(this._dbConnection);
        try {
            stmt = this._dbCreateStatement(query, params);
            stmt.execute();
            this.storeDeletedLogin(storedLogin);
            transaction.commit();
        } catch (e) {
            this.log("_removeLogin failed: " + e.name + " : " + e.message);
            throw "Couldn't write to database, login not removed.";
            transaction.rollback();
        } finally {
            if (stmt) {
                stmt.reset();
            }
        }
        this._sendNotification("removeLogin", storedLogin);
    },


    /*
     * modifyLogin
     *
     */
    modifyLogin : function (oldLogin, newLoginData) {
        let [idToModify, oldStoredLogin] = this._getIdForLogin(oldLogin);
        if (!idToModify)
            throw "No matching logins";

        let newLogin = LoginHelper.buildModifiedLogin(oldStoredLogin, newLoginData);

        // Check if the new GUID is duplicate.
        if (newLogin.guid != oldStoredLogin.guid &&
            !this._isGuidUnique(newLogin.guid)) 
        {
            throw "specified GUID already exists";
        }

        // Look for an existing entry in case key properties changed.
        if (!newLogin.matches(oldLogin, true)) {
            let logins = this.findLogins({}, newLogin.hostname,
                                         newLogin.formSubmitURL,
                                         newLogin.httpRealm);

            if (logins.some(login => newLogin.matches(login, true)))
                throw "This login already exists.";
        }

        // Get the encrypted value of the username and password.
        let [encUsername, encPassword, encType] = this._encryptLogin(newLogin);

        let query =
            "UPDATE moz_logins " +
            "SET hostname = :hostname, " +
                "httpRealm = :httpRealm, " +
                "formSubmitURL = :formSubmitURL, " +
                "usernameField = :usernameField, " +
                "passwordField = :passwordField, " +
                "encryptedUsername = :encryptedUsername, " +
                "encryptedPassword = :encryptedPassword, " +
                "guid = :guid, " +
                "encType = :encType, " +
                "timeCreated = :timeCreated, " +
                "timeLastUsed = :timeLastUsed, " +
                "timePasswordChanged = :timePasswordChanged, " +
                "timesUsed = :timesUsed " +
            "WHERE id = :id";

        let params = {
            id:                  idToModify,
            hostname:            newLogin.hostname,
            httpRealm:           newLogin.httpRealm,
            formSubmitURL:       newLogin.formSubmitURL,
            usernameField:       newLogin.usernameField,
            passwordField:       newLogin.passwordField,
            encryptedUsername:   encUsername,
            encryptedPassword:   encPassword,
            guid:                newLogin.guid,
            encType:             encType,
            timeCreated:         newLogin.timeCreated,
            timeLastUsed:        newLogin.timeLastUsed,
            timePasswordChanged: newLogin.timePasswordChanged,
            timesUsed:           newLogin.timesUsed
        };

        let stmt;
        try {
            stmt = this._dbCreateStatement(query, params);
            stmt.execute();
        } catch (e) {
            this.log("modifyLogin failed: " + e.name + " : " + e.message);
            throw "Couldn't write to database, login not modified.";
        } finally {
            if (stmt) {
                stmt.reset();
            }
        }

        this._sendNotification("modifyLogin", [oldStoredLogin, newLogin]);
    },


    /*
     * getAllLogins
     *
     * Returns an array of nsILoginInfo.
     */
    getAllLogins : function (count) {
        let [logins, ids] = this._searchLogins({});

        // decrypt entries for caller.
        logins = this._decryptLogins(logins);

        this.log("_getAllLogins: returning " + logins.length + " logins.");
        if (count)
            count.value = logins.length; // needed for XPCOM
        return logins;
    },


    /*
     * searchLogins
     *
     * Public wrapper around _searchLogins to convert the nsIPropertyBag to a
     * JavaScript object and decrypt the results.
     *
     * Returns an array of decrypted nsILoginInfo.
     */
    searchLogins : function(count, matchData) {
        let realMatchData = {};
        // Convert nsIPropertyBag to normal JS object
        let propEnum = matchData.enumerator;
        while (propEnum.hasMoreElements()) {
            let prop = propEnum.getNext().QueryInterface(Ci.nsIProperty);
            realMatchData[prop.name] = prop.value;
        }

        let [logins, ids] = this._searchLogins(realMatchData);

        // Decrypt entries found for the caller.
        logins = this._decryptLogins(logins);

        count.value = logins.length; // needed for XPCOM
        return logins;
    },


    /*
     * _searchLogins
     *
     * Private method to perform arbitrary searches on any field. Decryption is
     * left to the caller.
     *
     * Returns [logins, ids] for logins that match the arguments, where logins
     * is an array of encrypted nsLoginInfo and ids is an array of associated
     * ids in the database.
     */
    _searchLogins : function (matchData) {
        let conditions = [], params = {};

        for (let field in matchData) {
            let value = matchData[field];
            switch (field) {
                // Historical compatibility requires this special case
                case "formSubmitURL":
                    if (value != null) {
                        conditions.push("formSubmitURL = :formSubmitURL OR formSubmitURL = ''");
                        params["formSubmitURL"] = value;
                        break;
                    }
                // Normal cases.
                case "hostname":
                case "httpRealm":
                case "id":
                case "usernameField":
                case "passwordField":
                case "encryptedUsername":
                case "encryptedPassword":
                case "guid":
                case "encType":
                case "timeCreated":
                case "timeLastUsed":
                case "timePasswordChanged":
                case "timesUsed":
                    if (value == null) {
                        conditions.push(field + " isnull");
                    } else {
                        conditions.push(field + " = :" + field);
                        params[field] = value;
                    }
                    break;
                // Fail if caller requests an unknown property.
                default:
                    throw "Unexpected field: " + field;
            }
        }

        // Build query
        let query = "SELECT * FROM moz_logins";
        if (conditions.length) {
            conditions = conditions.map(function(c) "(" + c + ")");
            query += " WHERE " + conditions.join(" AND ");
        }

        let stmt;
        let logins = [], ids = [];
        try {
            stmt = this._dbCreateStatement(query, params);
            // We can't execute as usual here, since we're iterating over rows
            while (stmt.executeStep()) {
                // Create the new nsLoginInfo object, push to array
                let login = Cc["@mozilla.org/login-manager/loginInfo;1"].
                            createInstance(Ci.nsILoginInfo);
                login.init(stmt.row.hostname, stmt.row.formSubmitURL,
                           stmt.row.httpRealm, stmt.row.encryptedUsername,
                           stmt.row.encryptedPassword, stmt.row.usernameField,
                           stmt.row.passwordField);
                // set nsILoginMetaInfo values
                login.QueryInterface(Ci.nsILoginMetaInfo);
                login.guid = stmt.row.guid;
                login.timeCreated = stmt.row.timeCreated;
                login.timeLastUsed = stmt.row.timeLastUsed;
                login.timePasswordChanged = stmt.row.timePasswordChanged;
                login.timesUsed = stmt.row.timesUsed;
                logins.push(login);
                ids.push(stmt.row.id);
            }
        } catch (e) {
            this.log("_searchLogins failed: " + e.name + " : " + e.message);
        } finally {
            if (stmt) {
                stmt.reset();
            }
        }

        this.log("_searchLogins: returning " + logins.length + " logins");
        return [logins, ids];
    },

    /* storeDeletedLogin
     *
     * Moves a login to the deleted logins table
     *
     */
     storeDeletedLogin : function(aLogin) {
          let stmt = null; 
          try {
              this.log("Storing " + aLogin.guid + " in deleted passwords\n");
              let query = "INSERT INTO moz_deleted_logins (guid, timeDeleted) VALUES (:guid, :timeDeleted)";
              let params = { guid: aLogin.guid,
                             timeDeleted: Date.now() };
              let stmt = this._dbCreateStatement(query, params);
              stmt.execute();
          } catch(ex) {
              throw ex;
          } finally {
              if (stmt)
                  stmt.reset();
          }		
     },


    /*
     * removeAllLogins
     *
     * Removes all logins from storage.
     */
    removeAllLogins : function () {
        this.log("Removing all logins");
        let query;
        let stmt;
        let transaction = new Transaction(this._dbConnection);
 
        // Disabled hosts kept, as one presumably doesn't want to erase those.
        // TODO: Add these items to the deleted items table once we've sorted
        //       out the issues from bug 756701
        query = "DELETE FROM moz_logins";
        try {
            stmt = this._dbCreateStatement(query);
            stmt.execute();
            transaction.commit();
        } catch (e) {
            this.log("_removeAllLogins failed: " + e.name + " : " + e.message);
            transaction.rollback();
            throw "Couldn't write to database";
        } finally {
            if (stmt) {
                stmt.reset();
            }
        }

        this._sendNotification("removeAllLogins", null);
    },


    /*
     * getAllDisabledHosts
     *
     */
    getAllDisabledHosts : function (count) {
        let disabledHosts = this._queryDisabledHosts(null);

        this.log("_getAllDisabledHosts: returning " + disabledHosts.length + " disabled hosts.");
        if (count)
            count.value = disabledHosts.length; // needed for XPCOM
        return disabledHosts;
    },


    /*
     * getLoginSavingEnabled
     *
     */
    getLoginSavingEnabled : function (hostname) {
        this.log("Getting login saving is enabled for " + hostname);
        return this._queryDisabledHosts(hostname).length == 0
    },


    /*
     * setLoginSavingEnabled
     *
     */
    setLoginSavingEnabled : function (hostname, enabled) {
        // Throws if there are bogus values.
        LoginHelper.checkHostnameValue(hostname);

        this.log("Setting login saving enabled for " + hostname + " to " + enabled);
        let query;
        if (enabled)
            query = "DELETE FROM moz_disabledHosts " +
                    "WHERE hostname = :hostname";
        else
            query = "INSERT INTO moz_disabledHosts " +
                    "(hostname) VALUES (:hostname)";
        let params = { hostname: hostname };

        let stmt
        try {
            stmt = this._dbCreateStatement(query, params);
            stmt.execute();
        } catch (e) {
            this.log("setLoginSavingEnabled failed: " + e.name + " : " + e.message);
            throw "Couldn't write to database"
        } finally {
            if (stmt) {
                stmt.reset();
            }
        }

        this._sendNotification(enabled ? "hostSavingEnabled" : "hostSavingDisabled", hostname);
    },


    /*
     * findLogins
     *
     */
    findLogins : function (count, hostname, formSubmitURL, httpRealm) {
        let loginData = {
            hostname: hostname,
            formSubmitURL: formSubmitURL,
            httpRealm: httpRealm
        };
        let matchData = { };
        for each (let field in ["hostname", "formSubmitURL", "httpRealm"])
            if (loginData[field] != '')
                matchData[field] = loginData[field];
        let [logins, ids] = this._searchLogins(matchData);

        // Decrypt entries found for the caller.
        logins = this._decryptLogins(logins);

        this.log("_findLogins: returning " + logins.length + " logins");
        count.value = logins.length; // needed for XPCOM
        return logins;
    },


    /*
     * countLogins
     *
     */
    countLogins : function (hostname, formSubmitURL, httpRealm) {
        // Do checks for null and empty strings, adjust conditions and params
        let [conditions, params] =
            this._buildConditionsAndParams(hostname, formSubmitURL, httpRealm);

        let query = "SELECT COUNT(1) AS numLogins FROM moz_logins";
        if (conditions.length) {
            conditions = conditions.map(function(c) "(" + c + ")");
            query += " WHERE " + conditions.join(" AND ");
        }

        let stmt, numLogins;
        try {
            stmt = this._dbCreateStatement(query, params);
            stmt.executeStep();
            numLogins = stmt.row.numLogins;
        } catch (e) {
            this.log("_countLogins failed: " + e.name + " : " + e.message);
        } finally {
            if (stmt) {
                stmt.reset();
            }
        }

        this.log("_countLogins: counted logins: " + numLogins);
        return numLogins;
    },


    /*
     * uiBusy
     */
    get uiBusy() {
        return this._crypto.uiBusy;
    },


    /*
     * isLoggedIn
     */
    get isLoggedIn() {
        return this._crypto.isLoggedIn;
    },


    /*
     * _sendNotification
     *
     * Send a notification when stored data is changed.
     */
    _sendNotification : function (changeType, data) {
        let dataObject = data;
        // Can't pass a raw JS string or array though notifyObservers(). :-(
        if (data instanceof Array) {
            dataObject = Cc["@mozilla.org/array;1"].
                         createInstance(Ci.nsIMutableArray);
            for (let i = 0; i < data.length; i++)
                dataObject.appendElement(data[i], false);
        } else if (typeof(data) == "string") {
            dataObject = Cc["@mozilla.org/supports-string;1"].
                         createInstance(Ci.nsISupportsString);
            dataObject.data = data;
        }
        Services.obs.notifyObservers(dataObject, "passwordmgr-storage-changed", changeType);
    },


    /*
     * _getIdForLogin
     *
     * Returns an array with two items: [id, login]. If the login was not
     * found, both items will be null. The returned login contains the actual
     * stored login (useful for looking at the actual nsILoginMetaInfo values).
     */
    _getIdForLogin : function (login) {
        let matchData = { };
        for each (let field in ["hostname", "formSubmitURL", "httpRealm"])
            if (login[field] != '')
                matchData[field] = login[field];
        let [logins, ids] = this._searchLogins(matchData);

        let id = null;
        let foundLogin = null;

        // The specified login isn't encrypted, so we need to ensure
        // the logins we're comparing with are decrypted. We decrypt one entry
        // at a time, lest _decryptLogins return fewer entries and screw up
        // indices between the two.
        for (let i = 0; i < logins.length; i++) {
            let [decryptedLogin] = this._decryptLogins([logins[i]]);

            if (!decryptedLogin || !decryptedLogin.equals(login))
                continue;

            // We've found a match, set id and break
            foundLogin = decryptedLogin;
            id = ids[i];
            break;
        }

        return [id, foundLogin];
    },


    /*
     * _queryDisabledHosts
     *
     * Returns an array of hostnames from the database according to the
     * criteria given in the argument. If the argument hostname is null, the
     * result array contains all hostnames
     */
    _queryDisabledHosts : function (hostname) {
        let disabledHosts = [];

        let query = "SELECT hostname FROM moz_disabledHosts";
        let params = {};
        if (hostname) {
            query += " WHERE hostname = :hostname";
            params = { hostname: hostname };
        }

        let stmt;
        try {
            stmt = this._dbCreateStatement(query, params);
            while (stmt.executeStep())
                disabledHosts.push(stmt.row.hostname);
        } catch (e) {
            this.log("_queryDisabledHosts failed: " + e.name + " : " + e.message);
        } finally {
            if (stmt) {
                stmt.reset();
            }
        }

        return disabledHosts;
    },


    /*
     * _buildConditionsAndParams
     *
     * Adjusts the WHERE conditions and parameters for statements prior to the
     * statement being created. This fixes the cases where nulls are involved
     * and the empty string is supposed to be a wildcard match
     */
    _buildConditionsAndParams : function (hostname, formSubmitURL, httpRealm) {
        let conditions = [], params = {};

        if (hostname == null) {
            conditions.push("hostname isnull");
        } else if (hostname != '') {
            conditions.push("hostname = :hostname");
            params["hostname"] = hostname;
        }

        if (formSubmitURL == null) {
            conditions.push("formSubmitURL isnull");
        } else if (formSubmitURL != '') {
            conditions.push("formSubmitURL = :formSubmitURL OR formSubmitURL = ''");
            params["formSubmitURL"] = formSubmitURL;
        }

        if (httpRealm == null) {
            conditions.push("httpRealm isnull");
        } else if (httpRealm != '') {
            conditions.push("httpRealm = :httpRealm");
            params["httpRealm"] = httpRealm;
        }

        return [conditions, params];
    },


    /*
     * _isGuidUnique
     *
     * Checks to see if the specified GUID already exists.
     */
    _isGuidUnique : function (guid) {
        let query = "SELECT COUNT(1) AS numLogins FROM moz_logins WHERE guid = :guid";
        let params = { guid: guid };

        let stmt, numLogins;
        try {
            stmt = this._dbCreateStatement(query, params);
            stmt.executeStep();
            numLogins = stmt.row.numLogins;
        } catch (e) {
            this.log("_isGuidUnique failed: " + e.name + " : " + e.message);
        } finally {
            if (stmt) {
                stmt.reset();
            }
        }

        return (numLogins == 0);
    },


    /*
     * _encryptLogin
     *
     * Returns the encrypted username, password, and encrypton type for the specified
     * login. Can throw if the user cancels a master password entry.
     */
    _encryptLogin : function (login) {
        let encUsername = this._crypto.encrypt(login.username);
        let encPassword = this._crypto.encrypt(login.password);
        let encType     = this._crypto.defaultEncType;

        return [encUsername, encPassword, encType];
    },


    /*
     * _decryptLogins
     *
     * Decrypts username and password fields in the provided array of
     * logins.
     *
     * The entries specified by the array will be decrypted, if possible.
     * An array of successfully decrypted logins will be returned. The return
     * value should be given to external callers (since still-encrypted
     * entries are useless), whereas internal callers generally don't want
     * to lose unencrypted entries (eg, because the user clicked Cancel
     * instead of entering their master password)
     */
    _decryptLogins : function (logins) {
        let result = [];

        for each (let login in logins) {
            try {
                login.username = this._crypto.decrypt(login.username);
                login.password = this._crypto.decrypt(login.password);
            } catch (e) {
                // If decryption failed (corrupt entry?), just skip it.
                // Rethrow other errors (like canceling entry of a master pw)
                if (e.result == Cr.NS_ERROR_FAILURE)
                    continue;
                throw e;
            }
            result.push(login);
        }

        return result;
    },


    //**************************************************************************//
    // Database Creation & Access

    /*
     * _dbCreateStatement
     *
     * Creates a statement, wraps it, and then does parameter replacement
     * Returns the wrapped statement for execution.  Will use memoization
     * so that statements can be reused.
     */
    _dbCreateStatement : function (query, params) {
        let wrappedStmt = this._dbStmts[query];
        // Memoize the statements
        if (!wrappedStmt) {
            this.log("Creating new statement for query: " + query);
            wrappedStmt = this._dbConnection.createStatement(query);
            this._dbStmts[query] = wrappedStmt;
        }
        // Replace parameters, must be done 1 at a time
        if (params)
            for (let i in params)
                wrappedStmt.params[i] = params[i];
        return wrappedStmt;
    },


    /*
     * _dbInit
     *
     * Attempts to initialize the database. This creates the file if it doesn't
     * exist, performs any migrations, etc. Return if this is the first run.
     */
    _dbInit : function () {
        this.log("Initializing Database");
        let isFirstRun = false;
        try {
            this._dbConnection = this._storageService.openDatabase(this._signonsFile);
            // Get the version of the schema in the file. It will be 0 if the
            // database has not been created yet.
            let version = this._dbConnection.schemaVersion;
            if (version == 0) {
                this._dbCreate();
                isFirstRun = true;
            } else if (version != DB_VERSION) {
                this._dbMigrate(version);
            }
        } catch (e if e.result == Cr.NS_ERROR_FILE_CORRUPTED) {
            // Database is corrupted, so we backup the database, then throw
            // causing initialization to fail and a new db to be created next use
            this._dbCleanup(true);
            throw e;
        }

        Services.obs.addObserver(this, "profile-before-change", false);
        return isFirstRun;
    },

    observe: function (subject, topic, data) {
        switch (topic) {
            case "profile-before-change":
                Services.obs.removeObserver(this, "profile-before-change");
                this._dbClose();
            break;
        }
    },

    _dbCreate: function () {
        this.log("Creating Database");
        this._dbCreateSchema();
        this._dbConnection.schemaVersion = DB_VERSION;
    },


    _dbCreateSchema : function () {
        this._dbCreateTables();
        this._dbCreateIndices();
    },


    _dbCreateTables : function () {
        this.log("Creating Tables");
        for (let name in this._dbSchema.tables)
            this._dbConnection.createTable(name, this._dbSchema.tables[name]);
    },


    _dbCreateIndices : function () {
        this.log("Creating Indices");
        for (let name in this._dbSchema.indices) {
            let index = this._dbSchema.indices[name];
            let statement = "CREATE INDEX IF NOT EXISTS " + name + " ON " + index.table +
                            "(" + index.columns.join(", ") + ")";
            this._dbConnection.executeSimpleSQL(statement);
        }
    },


    _dbMigrate : function (oldVersion) {
        this.log("Attempting to migrate from version " + oldVersion);

        if (oldVersion > DB_VERSION) {
            this.log("Downgrading to version " + DB_VERSION);
            // User's DB is newer. Sanity check that our expected columns are
            // present, and if so mark the lower version and merrily continue
            // on. If the columns are borked, something is wrong so blow away
            // the DB and start from scratch. [Future incompatible upgrades
            // should swtich to a different table or file.]

            if (!this._dbAreExpectedColumnsPresent())
                throw Components.Exception("DB is missing expected columns",
                                           Cr.NS_ERROR_FILE_CORRUPTED);

            // Change the stored version to the current version. If the user
            // runs the newer code again, it will see the lower version number
            // and re-upgrade (to fixup any entries the old code added).
            this._dbConnection.schemaVersion = DB_VERSION;
            return;
        }

        // Upgrade to newer version...

        let transaction = new Transaction(this._dbConnection);

        try {
            for (let v = oldVersion + 1; v <= DB_VERSION; v++) {
                this.log("Upgrading to version " + v + "...");
                let migrateFunction = "_dbMigrateToVersion" + v;
                this[migrateFunction]();
            }
        } catch (e) {
            this.log("Migration failed: "  + e);
            transaction.rollback();
            throw e;
        }

        this._dbConnection.schemaVersion = DB_VERSION;
        transaction.commit();
        this.log("DB migration completed.");
    },


    /*
     * _dbMigrateToVersion2
     *
     * Version 2 adds a GUID column. Existing logins are assigned a random GUID.
     */
    _dbMigrateToVersion2 : function () {
        // Check to see if GUID column already exists, add if needed
        let query;
        if (!this._dbColumnExists("guid")) {
            query = "ALTER TABLE moz_logins ADD COLUMN guid TEXT";
            this._dbConnection.executeSimpleSQL(query);

            query = "CREATE INDEX IF NOT EXISTS moz_logins_guid_index ON moz_logins (guid)";
            this._dbConnection.executeSimpleSQL(query);
        }

        // Get a list of IDs for existing logins
        let ids = [];
        let query = "SELECT id FROM moz_logins WHERE guid isnull";
        let stmt;
        try {
            stmt = this._dbCreateStatement(query);
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
        query = "UPDATE moz_logins SET guid = :guid WHERE id = :id";
        for each (let id in ids) {
            let params = {
                id:   id,
                guid: this._uuidService.generateUUID().toString()
            };

            try {
                stmt = this._dbCreateStatement(query, params);
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


    /*
     * _dbMigrateToVersion3
     *
     * Version 3 adds a encType column.
     */
    _dbMigrateToVersion3 : function () {
        // Check to see if encType column already exists, add if needed
        let query;
        if (!this._dbColumnExists("encType")) {
            query = "ALTER TABLE moz_logins ADD COLUMN encType INTEGER";
            this._dbConnection.executeSimpleSQL(query);

            query = "CREATE INDEX IF NOT EXISTS " +
                        "moz_logins_encType_index ON moz_logins (encType)";
            this._dbConnection.executeSimpleSQL(query);
        }

        // Get a list of existing logins
        let logins = [];
        let stmt;
        query = "SELECT id, encryptedUsername, encryptedPassword " +
                    "FROM moz_logins WHERE encType isnull";
        try {
            stmt = this._dbCreateStatement(query);
            while (stmt.executeStep()) {
                let params = { id: stmt.row.id };
                // We will tag base64 logins correctly, but no longer support their use.
                if (stmt.row.encryptedUsername.charAt(0) == '~' ||
                    stmt.row.encryptedPassword.charAt(0) == '~')
                    params.encType = Ci.nsILoginManagerCrypto.ENCTYPE_BASE64;
                else
                    params.encType = Ci.nsILoginManagerCrypto.ENCTYPE_SDR;
                logins.push(params);
            }
        } catch (e) {
            this.log("Failed getting logins: " + e);
            throw e;
        } finally {
            if (stmt) {
                stmt.reset();
            }
        }

        // Determine encryption type for each login and update the DB.
        query = "UPDATE moz_logins SET encType = :encType WHERE id = :id";
        for each (let params in logins) {
            try {
                stmt = this._dbCreateStatement(query, params);
                stmt.execute();
            } catch (e) {
                this.log("Failed setting encType: " + e);
                throw e;
            } finally {
                if (stmt) {
                    stmt.reset();
                }
            }
        }
    },


    /*
     * _dbMigrateToVersion4
     *
     * Version 4 adds timeCreated, timeLastUsed, timePasswordChanged,
     * and timesUsed columns
     */
    _dbMigrateToVersion4 : function () {
        let query;
        // Add the new columns, if needed.
        for each (let column in ["timeCreated", "timeLastUsed", "timePasswordChanged", "timesUsed"]) {
            if (!this._dbColumnExists(column)) {
                query = "ALTER TABLE moz_logins ADD COLUMN " + column + " INTEGER";
                this._dbConnection.executeSimpleSQL(query);
            }
        }

        // Get a list of IDs for existing logins.
        let ids = [];
        let stmt;
        query = "SELECT id FROM moz_logins WHERE timeCreated isnull OR " +
                "timeLastUsed isnull OR timePasswordChanged isnull OR timesUsed isnull";
        try {
            stmt = this._dbCreateStatement(query);
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

        // Initialize logins with current time.
        query = "UPDATE moz_logins SET timeCreated = :initTime, timeLastUsed = :initTime, " +
                "timePasswordChanged = :initTime, timesUsed = 1 WHERE id = :id";
        let params = {
            id:       null,
            initTime: Date.now()
        };
        for each (let id in ids) {
            params.id = id;
            try {
                stmt = this._dbCreateStatement(query, params);
                stmt.execute();
            } catch (e) {
                this.log("Failed setting timestamps: " + e);
                throw e;
            } finally {
                if (stmt) {
                    stmt.reset();
                }
            }
        }
    },


    /*
     * _dbMigrateToVersion5
     *
     * Version 5 adds the moz_deleted_logins table
     */
    _dbMigrateToVersion5 : function () {
        if (!this._dbConnection.tableExists("moz_deleted_logins")) {
          this._dbConnection.createTable("moz_deleted_logins", this._dbSchema.tables.moz_deleted_logins);
        }
    },

    /*
     * _dbAreExpectedColumnsPresent
     *
     * Sanity check to ensure that the columns this version of the code expects
     * are present in the DB we're using.
     */
    _dbAreExpectedColumnsPresent : function () {
        let query = "SELECT " +
                       "id, " +
                       "hostname, " +
                       "httpRealm, " +
                       "formSubmitURL, " +
                       "usernameField, " +
                       "passwordField, " +
                       "encryptedUsername, " +
                       "encryptedPassword, " +
                       "guid, " +
                       "encType, " +
                       "timeCreated, " +
                       "timeLastUsed, " +
                       "timePasswordChanged, " +
                       "timesUsed " +
                    "FROM moz_logins";
        try {
            let stmt = this._dbConnection.createStatement(query);
            // (no need to execute statement, if it compiled we're good)
            stmt.finalize();
        } catch (e) {
            return false;
        }

        query = "SELECT " +
                   "id, " +
                   "hostname " +
                "FROM moz_disabledHosts";
        try {
            let stmt = this._dbConnection.createStatement(query);
            // (no need to execute statement, if it compiled we're good)
            stmt.finalize();
        } catch (e) {
            return false;
        }

        this.log("verified that expected columns are present in DB.");
        return true;
    },


    /*
     * _dbColumnExists
     *
     * Checks to see if the named column already exists.
     */
    _dbColumnExists : function (columnName) {
        let query = "SELECT " + columnName + " FROM moz_logins";
        try {
            let stmt = this._dbConnection.createStatement(query);
            // (no need to execute statement, if it compiled we're good)
            stmt.finalize();
            return true;
        } catch (e) {
            return false;
        }
    },

    _dbClose : function () {
        this.log("Closing the DB connection.");
        // Finalize all statements to free memory, avoid errors later
        for each (let stmt in this._dbStmts) {
            stmt.finalize();
        }
        this._dbStmts = {};

        if (this._dbConnection !== null) {
            try {
                this._dbConnection.close();
            } catch (e) {
                Components.utils.reportError(e);
            }
        }
        this._dbConnection = null;
    },

    /*
     * _dbCleanup
     *
     * Called when database creation fails. Finalizes database statements,
     * closes the database connection, deletes the database file.
     */
    _dbCleanup : function (backup) {
        this.log("Cleaning up DB file - close & remove & backup=" + backup)

        // Create backup file
        if (backup) {
            let backupFile = this._signonsFile.leafName + ".corrupt";
            this._storageService.backupDatabaseFile(this._signonsFile, backupFile);
        }

        this._dbClose();
        this._signonsFile.remove(false);
    }

}; // end of nsLoginManagerStorage_mozStorage implementation

let component = [LoginManagerStorage_mozStorage];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(component);
