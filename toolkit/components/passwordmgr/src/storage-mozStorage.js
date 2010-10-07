/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Paul O'Shannessy <poshannessy@mozilla.com> (primary author)
 *  Mrinal Kant <mrinal.kant@gmail.com> (original sqlite related changes)
 *  Justin Dolske <dolske@mozilla.com> (encryption/decryption functions are
 *                                     a lift from Justin's storage-Legacy.js)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

const DB_VERSION = 4; // The database schema version

const ENCTYPE_BASE64 = 0;
const ENCTYPE_SDR = 1;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

function LoginManagerStorage_mozStorage() { };

LoginManagerStorage_mozStorage.prototype = {

    classID : Components.ID("{8c2023b9-175c-477e-9761-44ae7b549756}"),
    QueryInterface : XPCOMUtils.generateQI([Ci.nsILoginManagerStorage]),

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
    _importFile   : null,  // nsIFile for import from legacy
    _debug        : false, // mirrors signon.debug
    _base64checked : false,


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
     * initWithFile
     *
     * Initialize the component, but override the default filename locations.
     * This is primarily used to the unit tests and profile migration.
     * aImportFile is legacy storage file, aDBFile is a sqlite/mozStorage file.
     */
    initWithFile : function(aImportFile, aDBFile) {
        if (aImportFile)
            this._importFile = aImportFile;
        if (aDBFile)
            this._signonsFile = aDBFile;

        this.init();
    },


    /*
     * init
     *
     * Initialize this storage component; import from legacy files, if
     * necessary. Most of the work is done in _deferredInit.
     */
    init : function () {
        this._dbStmts = [];

        // Connect to the correct preferences branch.
        this._prefBranch = Services.prefs.getBranch("signon.");
        this._debug = this._prefBranch.getBoolPref("debug");

        let isFirstRun;
        try {
            // If initWithFile is calling us, _signonsFile may already be set.
            if (!this._signonsFile) {
                // Initialize signons.sqlite
                this._signonsFile = this._profileDir.clone();
                this._signonsFile.append("signons.sqlite");
            }
            this.log("Opening database at " + this._signonsFile.path);

            // Initialize the database (create, migrate as necessary)
            isFirstRun = this._dbInit();

            // On first run we want to import the default legacy storage files.
            // Otherwise if passed a file, import from that.
            if (isFirstRun && !this._importFile)
                this._importLegacySignons();
            else if (this._importFile)
                this._importLegacySignons(this._importFile);

            this._initialized = true;
        } catch (e) {
            this.log("Initialization failed: " + e);
            // If the import fails on first run, we want to delete the db
            if (isFirstRun && e == "Import failed")
                this._dbCleanup(false);
            throw "Initialization failed";
        }
    },


    /*
     * addLogin
     *
     */
    addLogin : function (login) {
        this._addLogin(login, false);
    },


    /*
     * _addLogin
     *
     * Private function wrapping core addLogin functionality.
     */
    _addLogin : function (login, isEncrypted) {
        let encUsername, encPassword;

        // Throws if there are bogus values.
        this._checkLoginValues(login);

        if (isEncrypted)
            [encUsername, encPassword] = [login.username, login.password];
        else
            [encUsername, encPassword] = this._encryptLogin(login);

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

        // Determine encryption type
        let encType = ENCTYPE_SDR;
        if (isEncrypted &&
            (encUsername.charAt(0) == '~' || encPassword.charAt(0) == '~'))
            encType = ENCTYPE_BASE64;

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
            this.log("_addLogin failed: " + e.name + " : " + e.message);
            throw "Couldn't write to database, login not added.";
        } finally {
            stmt.reset();
        }

        // Send a notification that a login was added.
        if (!isEncrypted)
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
        try {
            stmt = this._dbCreateStatement(query, params);
            stmt.execute();
        } catch (e) {
            this.log("_removeLogin failed: " + e.name + " : " + e.message);
            throw "Couldn't write to database, login not removed.";
        } finally {
            stmt.reset();
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
        oldStoredLogin.QueryInterface(Ci.nsILoginMetaInfo);

        let newLogin;
        if (newLoginData instanceof Ci.nsILoginInfo) {
            // Clone the existing login to get its nsILoginMetaInfo, then init it
            // with the replacement nsILoginInfo data from the new login.
            newLogin = oldStoredLogin.clone();
            newLogin.init(newLoginData.hostname,
                          newLoginData.formSubmitURL, newLoginData.httpRealm,
                          newLoginData.username, newLoginData.password,
                          newLoginData.usernameField, newLoginData.passwordField);
            newLogin.QueryInterface(Ci.nsILoginMetaInfo);

            // Automatically update metainfo when password is changed.
            if (newLogin.password != oldLogin.password)
                newLogin.timePasswordChanged = Date.now();
        } else if (newLoginData instanceof Ci.nsIPropertyBag) {
            function _bagHasProperty(aPropName) {
                try {
                    newLoginData.getProperty(aPropName);
                    return true;
                } catch (e) {
                    return false;
                }
            }

            // Clone the existing login, along with all its properties.
            newLogin = oldStoredLogin.clone();
            newLogin.QueryInterface(Ci.nsILoginMetaInfo);

            // Automatically update metainfo when password is changed.
            // (Done before the main property updates, lest the caller be
            // explicitly updating both .password and .timePasswordChanged)
            if (_bagHasProperty("password")) {
                let newPassword = newLoginData.getProperty("password");
                if (newPassword != oldLogin.password)
                    newLogin.timePasswordChanged = Date.now();
            }

            let propEnum = newLoginData.enumerator;
            while (propEnum.hasMoreElements()) {
                let prop = propEnum.getNext().QueryInterface(Ci.nsIProperty);
                switch (prop.name) {
                    // nsILoginInfo properties...
                    case "hostname":
                    case "httpRealm":
                    case "formSubmitURL":
                    case "username":
                    case "password":
                    case "usernameField":
                    case "passwordField":
                    // nsILoginMetaInfo properties...
                    case "guid":
                    case "timeCreated":
                    case "timeLastUsed":
                    case "timePasswordChanged":
                    case "timesUsed":
                        newLogin[prop.name] = prop.value;
                        if (prop.name == "guid" && !this._isGuidUnique(newLogin.guid))
                            throw "specified GUID already exists";
                        break;

                    // Fake property, allows easy incrementing.
                    case "timesUsedIncrement":
                        newLogin.timesUsed += prop.value;
                        break;

                    // Fail if caller requests setting an unknown property.
                    default:
                        throw "Unexpected propertybag item: " + prop.name;
                }
            }
        } else {
            throw "newLoginData needs an expected interface!";
        }

        // Throws if there are bogus values.
        this._checkLoginValues(newLogin);

        // Get the encrypted value of the username and password.
        let [encUsername, encPassword] = this._encryptLogin(newLogin);

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
            encType:             ENCTYPE_SDR,
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
            stmt.reset();
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
     * getAllEncryptedLogins
     *
     * Not implemented. This interface was added to extract logins from the
     * legacy storage module without decrypting them. Now that logins are in
     * mozStorage, if the encrypted data is really needed it can be easily
     * obtained with SQL and the mozStorage APIs.
     */
    getAllEncryptedLogins : function (count) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
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

        for (field in matchData) {
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
            stmt.reset();
        }

        this.log("_searchLogins: returning " + logins.length + " logins");
        return [logins, ids];
    },


    /*
     * removeAllLogins
     *
     * Removes all logins from storage.
     */
    removeAllLogins : function () {
        this.log("Removing all logins");
        // Delete any old, unused files.
        this._removeOldSignonsFiles();

        // Disabled hosts kept, as one presumably doesn't want to erase those.
        let query = "DELETE FROM moz_logins";
        let stmt;
        try {
            stmt = this._dbCreateStatement(query);
            stmt.execute();
        } catch (e) {
            this.log("_removeAllLogins failed: " + e.name + " : " + e.message);
            throw "Couldn't write to database";
        } finally {
            stmt.reset();
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
        this._checkHostnameValue(hostname);

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
            stmt.reset();
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
        for each (field in ["hostname", "formSubmitURL", "httpRealm"])
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
            stmt.reset();
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
        for each (field in ["hostname", "formSubmitURL", "httpRealm"])
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
            stmt.reset();
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
     * _checkLoginValues
     *
     * Due to the way the signons2.txt file is formatted, we need to make
     * sure certain field values or characters do not cause the file to
     * be parse incorrectly. Reject logins that we can't store correctly.
     */
    _checkLoginValues : function (aLogin) {
        function badCharacterPresent(l, c) {
            return ((l.formSubmitURL && l.formSubmitURL.indexOf(c) != -1) ||
                    (l.httpRealm     && l.httpRealm.indexOf(c)     != -1) ||
                                        l.hostname.indexOf(c)      != -1  ||
                                        l.usernameField.indexOf(c) != -1  ||
                                        l.passwordField.indexOf(c) != -1);
        }

        // Nulls are invalid, as they don't round-trip well.
        // Mostly not a formatting problem, although ".\0" can be quirky.
        if (badCharacterPresent(aLogin, "\0"))
            throw "login values can't contain nulls";

        // In theory these nulls should just be rolled up into the encrypted
        // values, but nsISecretDecoderRing doesn't use nsStrings, so the
        // nulls cause truncation. Check for them here just to avoid
        // unexpected round-trip surprises.
        if (aLogin.username.indexOf("\0") != -1 ||
            aLogin.password.indexOf("\0") != -1)
            throw "login values can't contain nulls";

        // Newlines are invalid for any field stored as plaintext.
        if (badCharacterPresent(aLogin, "\r") ||
            badCharacterPresent(aLogin, "\n"))
            throw "login values can't contain newlines";

        // A line with just a "." can have special meaning.
        if (aLogin.usernameField == "." ||
            aLogin.formSubmitURL == ".")
            throw "login values can't be periods";

        // A hostname with "\ \(" won't roundtrip.
        // eg host="foo (", realm="bar" --> "foo ( (bar)"
        // vs host="foo", realm=" (bar" --> "foo ( (bar)"
        if (aLogin.hostname.indexOf(" (") != -1)
            throw "bad parens in hostname";
    },


    /*
     * _checkHostnameValue
     *
     * Legacy storage prohibited newlines and nulls in hostnames, so we'll keep
     * that standard here. Throws on illegal format.
     */
    _checkHostnameValue : function (hostname) {
        // File format prohibits certain values. Also, nulls
        // won't round-trip with getAllDisabledHosts().
        if (hostname == "." ||
            hostname.indexOf("\r") != -1 ||
            hostname.indexOf("\n") != -1 ||
            hostname.indexOf("\0") != -1)
            throw "Invalid hostname";
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
            stmt.reset();
        }

        return (numLogins == 0);
    },


    /*
     * _importLegacySignons
     *
     * Imports a file that uses Legacy storage. Will use importFile if provided
     * else it will attempt to initialize the Legacy storage normally.
     *
     */
    _importLegacySignons : function (importFile) {
        this.log("Importing " + (importFile ? importFile.path : "legacy storage"));

        let legacy = Cc["@mozilla.org/login-manager/storage/legacy;1"].
                     createInstance(Ci.nsILoginManagerStorage);

        // Import all logins and disabled hosts
        try {
            if (importFile)
                legacy.initWithFile(importFile, null);
            else
                legacy.init();

            // Import logins and disabledHosts
            let logins = legacy.getAllEncryptedLogins();

            // Wrap in a transaction for better performance.
            this._dbConnection.beginTransaction();
            for each (let login in logins) {
                try {
                    this._addLogin(login, true);
                } catch (e) {
                    this.log("_importLegacySignons failed to add login: " + e);
                }
            }
            let disabledHosts = legacy.getAllDisabledHosts();
            for each (let hostname in disabledHosts)
                this.setLoginSavingEnabled(hostname, false);
            this._dbConnection.commitTransaction();
        } catch (e) {
            this.log("_importLegacySignons failed: " + e.name + " : " + e.message);
            throw "Import failed";
        }
    },


    /*
     * _removeOldSignonsFiles
     *
     * Deletes any storage files that we're not using any more.
     */
    _removeOldSignonsFiles : function () {
        // We've used a number of prefs over time due to compatibility issues.
        // We want to delete all files referenced in prefs, which are only for
        // importing and clearing logins from storage-Legacy.js.
        let filenamePrefs = ["SignonFileName3", "SignonFileName2", "SignonFileName"];
        for each (let prefname in filenamePrefs) {
            let filename = this._prefBranch.getCharPref(prefname);
            let file = this._profileDir.clone();
            file.append(filename);

            if (file.exists()) {
                this.log("Deleting old " + filename + " (" + prefname + ")");
                try {
                    file.remove(false);
                } catch (e) {
                    this.log("NOTICE: Couldn't delete " + filename + ": " + e);
                }
            }
        }
    },


    /*
     * _encryptLogin
     *
     * Returns the encrypted username and password for the specified login,
     * and a boolean indicating if the user canceled the master password entry
     * (in which case no encrypted values are returned).
     */
    _encryptLogin : function (login) {
        let encUsername = this._crypto.encrypt(login.username);
        let encPassword = this._crypto.encrypt(login.password);

        if (!this._base64checked)
            this._reencryptBase64Logins();

        return [encUsername, encPassword];
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

        if (!this._base64checked)
            this._reencryptBase64Logins();

        return result;
    },


    /*
     * _reencryptBase64Logins
     *
     * Checks the signons DB for any logins using the old wallet-style base64
     * obscuring of the username/password, instead of proper encryption. We're
     * called once per session, after the user has successfully encrypted or
     * decrypted some login (this helps ensure the user doesn't get mysterious
     * prompts for a master password, when set).
     */
    _reencryptBase64Logins : function () {
        this._base64checked = true;
        // Ignore failures, will try again next session...

        this.log("Reencrypting Base64 logins");
        this._dbConnection.beginTransaction();
        try {
            let [logins, ids] = this._searchLogins({ encType: ENCTYPE_BASE64 });

            if (!logins.length)
                return;

            try {
                logins = this._decryptLogins(logins);
            } catch (e) {
                // User might have canceled master password entry, just ignore.
                return;
            }

            let encUsername, encPassword, stmt;
            for each (let login in logins) {
                [encUsername, encPassword] = this._encryptLogin(login);

                let query =
                    "UPDATE moz_logins " +
                    "SET encryptedUsername = :encryptedUsername, " +
                        "encryptedPassword = :encryptedPassword, " +
                        "encType = :encType " +
                    "WHERE guid = :guid";
                let params = {
                    encryptedUsername: encUsername,
                    encryptedPassword: encPassword,
                    encType:           ENCTYPE_SDR,
                    guid:              login.guid
                };
                try {
                    stmt = this._dbCreateStatement(query, params);
                    stmt.execute();
                } catch (e) {
                    // Ignore singular errors, continue trying to update others.
                    this.log("_reencryptBase64Logins caught error: " + e);
                } finally {
                    stmt.reset();
                }
            }
        } catch (e) {
            this.log("_reencryptBase64Logins failed: " + e);
        } finally {
            this._dbConnection.commitTransaction();
        }
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
     * exist, performs any migrations, etc. When database is first created, we
     * attempt to import legacy signons. Return if this is the first run.
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
        return isFirstRun;
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

        this._dbConnection.beginTransaction();

        try {
            for (let v = oldVersion + 1; v <= DB_VERSION; v++) {
                this.log("Upgrading to version " + v + "...");
                let migrateFunction = "_dbMigrateToVersion" + v;
                this[migrateFunction]();
            }
        } catch (e) {
            this.log("Migration failed: "  + e);
            this._dbConnection.rollbackTransaction();
            throw e;
        }

        this._dbConnection.schemaVersion = DB_VERSION;
        this._dbConnection.commitTransaction();
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
            stmt.reset();
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
                stmt.reset();
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
                if (stmt.row.encryptedUsername.charAt(0) == '~' ||
                    stmt.row.encryptedPassword.charAt(0) == '~')
                    params.encType = ENCTYPE_BASE64;
                else
                    params.encType = ENCTYPE_SDR;
                logins.push(params);
            }
        } catch (e) {
            this.log("Failed getting logins: " + e);
            throw e;
        } finally {
            stmt.reset();
        }

        // Determine encryption type for each login and update the DB.
        query = "UPDATE moz_logins SET encType = :encType WHERE id = :id";
        for each (params in logins) {
            try {
                stmt = this._dbCreateStatement(query, params);
                stmt.execute();
            } catch (e) {
                this.log("Failed setting encType: " + e);
                throw e;
            } finally {
                stmt.reset();
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
            stmt.reset();
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
                stmt.reset();
            }
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

        // Finalize all statements to free memory, avoid errors later
        for each (let stmt in this._dbStmts)
            stmt.finalize();
        this._dbStmts = [];

        // Close the connection, ignore 'already closed' error
        try { this._dbConnection.close() } catch(e) {}
        this._signonsFile.remove(false);
    }

}; // end of nsLoginManagerStorage_mozStorage implementation

let component = [LoginManagerStorage_mozStorage];
var NSGetFactory = XPCOMUtils.generateNSGetFactory(component);
