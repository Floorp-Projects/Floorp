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
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Justin Dolske <dolske@mozilla.com> (original author)
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

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function LoginManagerStorage_legacy() { };

LoginManagerStorage_legacy.prototype = {

    classDescription  : "LoginManagerStorage_legacy",
    contractID : "@mozilla.org/login-manager/storage/legacy;1",
    classID : Components.ID("{e09e4ca6-276b-4bb4-8b71-0635a3a2a007}"),
    QueryInterface : XPCOMUtils.generateQI([Ci.nsILoginManagerStorage,
                                    Ci.nsILoginManagerIEMigrationHelper]),

    __logService : null, // Console logging service, used for debugging.
    get _logService() {
        if (!this.__logService)
            this.__logService = Cc["@mozilla.org/consoleservice;1"].
                                getService(Ci.nsIConsoleService);
        return this.__logService;
    },

    __ioService: null, // IO service for string -> nsIURI conversion
    get _ioService() {
        if (!this.__ioService)
            this.__ioService = Cc["@mozilla.org/network/io-service;1"].
                               getService(Ci.nsIIOService);
        return this.__ioService;
    },

    __decoderRing : null,  // nsSecretDecoderRing service
    get _decoderRing() {
        if (!this.__decoderRing)
            this.__decoderRing = Cc["@mozilla.org/security/sdr;1"].
                                 getService(Ci.nsISecretDecoderRing);
        return this.__decoderRing;
    },

    __utfConverter : null, // UCS2 <--> UTF8 string conversion
    get _utfConverter() {
        if (!this.__utfConverter) {
            this.__utfConverter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                                  createInstance(Ci.nsIScriptableUnicodeConverter);
            this.__utfConverter.charset = "UTF-8";
        }
        return this.__utfConverter;
    },

    _utfConverterReset : function() {
        this.__utfConverter = null;
    },

    __profileDir: null,  // nsIFile for the user's profile dir
    get _profileDir() {
        if (!this.__profileDir) {
            var dirService = Cc["@mozilla.org/file/directory_service;1"].
                             getService(Ci.nsIProperties);
            this.__profileDir = dirService.get("ProfD", Ci.nsIFile);
        }
        return this.__profileDir;
    },

    __nsLoginInfo: null,  // Constructor for nsILoginInfo implementation
    get _nsLoginInfo() {
        if (!this.__nsLoginInfo)
            this.__nsLoginInfo = new Components.Constructor(
                "@mozilla.org/login-manager/loginInfo;1", Ci.nsILoginInfo);
        return this.__nsLoginInfo;
    },

    _prefBranch : null,  // Preferences service

    _signonsFile : null,  // nsIFile for "signons3.txt" (or whatever pref is)
    _debug       : false, // mirrors signon.debug

    /*
     * A list of prefs that have been used to specify the filename for storing
     * logins. (We've used a number over time due to compatibility issues.)
     * This list is also used by _removeOldSignonsFile() to clean up old files.
     */
    _filenamePrefs : ["SignonFileName3", "SignonFileName2", "SignonFileName"],

    /*
     * Core datastructures
     *
     * EG: _logins["http://site.com"][0].password
     * EG: _disabledHosts["never.site.com"]
     */
    _logins        : null, 
    _disabledHosts : null,


    /*
     * log
     *
     * Internal function for logging debug messages to the Error Console.
     */
    log : function (message) {
        if (!this._debug)
            return;
        dump("PwMgr Storage: " + message + "\n");
        this._logService.logStringMessage("PwMgr Storage: " + message);
    },




    /* ==================== Public Methods ==================== */




    initWithFile : function(aInputFile, aOutputFile) {
        this._signonsFile = aInputFile;

        this.init();

        if (aOutputFile) {
            this._signonsFile = aOutputFile;
            this._writeFile();
        }
    },

    /*
     * init
     *
     * Initialize this storage component and load stored passwords from disk.
     */
    init : function () {
        this._logins  = {};
        this._disabledHosts = {};

        // Connect to the correct preferences branch.
        this._prefBranch = Cc["@mozilla.org/preferences-service;1"].
                           getService(Ci.nsIPrefService);
        this._prefBranch = this._prefBranch.getBranch("signon.");
        this._prefBranch.QueryInterface(Ci.nsIPrefBranch2);

        this._debug = this._prefBranch.getBoolPref("debug");

        // Check to see if the internal PKCS#11 token has been initialized.
        // If not, set a blank password.
        var tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"].
                      getService(Ci.nsIPK11TokenDB);

        var token = tokenDB.getInternalKeyToken();
        if (token.needsUserInit) {
            this.log("Initializing key3.db with default blank password.");
            token.initPassword("");
        }

        var importFile = null;
        // If initWithFile is calling us, _signonsFile is already set.
        if (!this._signonsFile)
            [this._signonsFile, importFile] = this._getSignonsFile();

        // If we have an import file, do a switcharoo before reading it.
        if (importFile) {
            this.log("Importing " + importFile.path);

            var tmp = this._signonsFile;
            this._signonsFile = importFile;
        }

        // Read in the stored login data.
        this._readFile();

        // If we were importing, write back to the normal file.
        if (importFile) {
            this._signonsFile = tmp;
            this._writeFile();
        }
    },


    /*
     * addLogin
     *
     */
    addLogin : function (login) {
        // Throws if there are bogus values.
        this._checkLoginValues(login);

        // Clone the input. This ensures changes made by the caller to the
        // login (after calling addLogin) do no change the login we have.
        // Also, we rely on using login.wrappedJSObject, but can't rely on the
        // thing provided by the caller to support that.
        var clone = new this._nsLoginInfo();
        clone.init(login.hostname, login.formSubmitURL, login.httpRealm,
                   login.username,      login.password,
                   login.usernameField, login.passwordField);
        login = clone;

        var key = login.hostname;

        // If first entry for key, create an Array to hold its logins.
        var rollback;
        if (!this._logins[key]) {
            this._logins[key] = [];
            rollback = null;
        } else {
            rollback = this._logins[key].concat(); // clone array
        }

        this._logins[key].push(login);

        var ok = this._writeFile();

        // If we failed, don't keep the added login in memory.
        if (!ok) {
            if (rollback)
                this._logins[key] = rollback;
            else
                delete this._logins[key];

            throw "Couldn't write to file, login not added.";
        }
    },


    /*
     * removeLogin
     *
     */
    removeLogin : function (login) {
        var key = login.hostname;
        var logins = this._logins[key];

        if (!logins)
            throw "No logins found for hostname (" + key + ")";

        var rollback = this._logins[key].concat(); // clone array

        // The specified login isn't encrypted, so we need to ensure
        // the logins we're comparing with are decrypted. We decrypt one entry
        // at a time, lest _decryptLogins return fewer entries and screw up
        // indices between the two.
        for (var i = 0; i < logins.length; i++) {

            var [[decryptedLogin], userCanceled] =
                        this._decryptLogins([logins[i]]);

            if (userCanceled)
                throw "User canceled master password entry, login not removed.";

            if (!decryptedLogin)
                continue;

            if (decryptedLogin.equals(login)) {
                logins.splice(i, 1); // delete that login from array.
                break;
                // Note that if there are duplicate entries, they'll
                // have to be deleted one-by-one.
            }
        }

        // Did we delete the last login for this host?
        if (logins.length == 0)
            delete this._logins[key];

        var ok = this._writeFile();

        // If we failed, don't actually remove the login.
        if (!ok) {
            this._logins[key] = rollback;
            throw "Couldn't write to file, login not removed.";
        }
    },


    /*
     * modifyLogin
     *
     */
    modifyLogin : function (oldLogin, newLogin) {
        if (newLogin instanceof Ci.nsIPropertyBag)
            throw "legacy modifyLogin with propertybag not implemented.";
        newLogin.QueryInterface(Ci.nsILoginInfo);
        // Throws if there are bogus values.
        this._checkLoginValues(newLogin);

        this.removeLogin(oldLogin);
        this.addLogin(newLogin);
    },


    /*
     * getAllLogins
     *
     * Returns an array of nsAccountInfo.
     */
    getAllLogins : function (count) {
        var result = [], userCanceled;

        // Each entry is an array -- append the array entries to |result|.
        for each (var hostLogins in this._logins) {
            result = result.concat(hostLogins);
        }

        // decrypt entries for caller.
        [result, userCanceled] = this._decryptLogins(result);

        if (userCanceled)
            throw "User canceled Master Password entry";

        count.value = result.length; // needed for XPCOM
        return result;
    },


    /*
     * getAllEncryptedLogins
     *
     * Returns an array of nsAccountInfo, each in the encrypted state.
     */
    getAllEncryptedLogins : function (count) {
        var result = [];

        // Each entry is an array -- append the array entries to |result|.
        for each (var hostLogins in this._logins) {
            // Return copies to the caller. Prevents callers from modifying
            // our internal storage
            for each (var login in hostLogins) {
                var clone = new this._nsLoginInfo();
                clone.init(login.hostname, login.formSubmitURL, login.httpRealm,
                           login.wrappedJSObject.encryptedUsername,
                           login.wrappedJSObject.encryptedPassword,
                           login.usernameField, login.passwordField);
                result.push(clone);
            }
        }

        count.value = result.length; // needed for XPCOM
        return result;
    },


    /*
     * searchLogins
     *
     * Not implemented. This interface was added to perform arbitrary searches.
     * Since the legacy storage module is no longer used, there is no need to
     * implement it here.
     */
    searchLogins : function (count, matchData) {
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    },


    /*
     * removeAllLogins
     *
     * Removes all logins from storage.
     */
    removeAllLogins : function () {
        // Delete any old, unused files.
        this._removeOldSignonsFiles();

        // Disabled hosts kept, as one presumably doesn't want to erase those.
        this._logins = {};
        this._writeFile();
    },


    /*
     * getAllDisabledHosts
     *
     */
    getAllDisabledHosts : function (count) {
        var result = [];

        for (var hostname in this._disabledHosts) {
            result.push(hostname);
        }

        count.value = result.length; // needed for XPCOM
        return result;
    },


    /*
     * getLoginSavingEnabled
     *
     */
    getLoginSavingEnabled : function (hostname) {
        return !this._disabledHosts[hostname];
    },


    /*
     * setLoginSavingEnabled
     *
     */
    setLoginSavingEnabled : function (hostname, enabled) {
        // File format prohibits certain values. Also, nulls
        // won't round-trip with getAllDisabledHosts().
        if (hostname == "." ||
            hostname.indexOf("\r") != -1 ||
            hostname.indexOf("\n") != -1 ||
            hostname.indexOf("\0") != -1)
            throw "Invalid hostname";

        if (enabled)
            delete this._disabledHosts[hostname];
        else
            this._disabledHosts[hostname] = true;

        this._writeFile();
    },


    /*
     * findLogins
     *
     */
    findLogins : function (count, hostname, formSubmitURL, httpRealm) {
        var userCanceled;

        var logins = this._searchLogins(hostname, formSubmitURL, httpRealm);

        // Decrypt entries found for the caller.
        [logins, userCanceled] = this._decryptLogins(logins);

        // We want to throw in this case, so that the Login Manager
        // knows to stop processing forms on the page so the user isn't
        // prompted multiple times.
        if (userCanceled)
            throw "User canceled Master Password entry";

        count.value = logins.length; // needed for XPCOM
        return logins;
    },

    
    /*
     * countLogins
     *
     */
    countLogins : function (aHostname, aFormSubmitURL, aHttpRealm) {
        var logins;

        // Normal case: return direct results for the specified host.
        if (aHostname) {
            logins = this._searchLogins(aHostname, aFormSubmitURL, aHttpRealm);
            return logins.length
        } 

        // For consistency with how aFormSubmitURL and aHttpRealm work
        if (aHostname == null)
            return 0;

        // aHostname == "", so loop through each known host to match with each.
        var count = 0;
        for (var hostname in this._logins) {
            logins = this._searchLogins(hostname, aFormSubmitURL, aHttpRealm);
            count += logins.length;
        }

        return count;
    },




    /* ==================== Internal Methods ==================== */




    /*
     * _searchLogins
     *
     */
    _searchLogins : function (hostname, formSubmitURL, httpRealm) {
        var hostLogins = this._logins[hostname];
        if (hostLogins == null)
            return [];

        var result = [], userCanceled;

        for each (var login in hostLogins) {

            // If search arg is null, skip login unless it doesn't specify a
            // httpRealm (ie, it's also null). If the search arg is an empty
            // string, always match.
            if (httpRealm == null) {
                if (login.httpRealm != null)
                    continue;
            } else if (httpRealm != "") {
                // Make sure the realms match. If search arg is null,
                // only match if login doesn't specify a realm (is null)
                if (httpRealm != login.httpRealm)
                    continue;
            }

            // If search arg is null, skip login unless it doesn't specify a
            // action URL (ie, it's also null). If the search arg is an empty
            // string, always match.
            if (formSubmitURL == null) {
                if (login.formSubmitURL != null)
                    continue;
            } else if (formSubmitURL != "") {
                // If the stored login is blank (not null), that means the
                // login was stored before we started keeping the action
                // URL, so always match. Unless the search g
                if (login.formSubmitURL != "" &&
                    formSubmitURL != login.formSubmitURL)
                    continue;
            }

            result.push(login);
        }

        return result;
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
     * _getSignonsFile
     *
     * Determines what file to use based on prefs. Returns it as a
     * nsILocalFile, along with a file to import from first (if needed)
     *
     */
    _getSignonsFile : function() {
        var destFile = null, importFile = null;

        // We've used a number of prefs over time due to compatibility issues.
        // Use the filename specified in the newest pref, but import from
        // older files if needed.
        for (var i = 0; i < this._filenamePrefs.length; i++) {
            var prefname = this._filenamePrefs[i];
            var filename = this._prefBranch.getCharPref(prefname);
            var file = this._profileDir.clone();
            file.append(filename);

            this.log("Checking file " + filename + " (" + prefname + ")");

            // First loop through, save the preferred filename.
            if (!destFile)
                destFile = file;
            else
                importFile = file;

            if (file.exists())
                return [destFile, importFile];
        }

        // If we can't find any existing file, use the preferred file.
        return [destFile, null];
    },


    /*
     * _removeOldSignonsFiles
     *
     * Deletes any storage files that we're not using any more.
     */
    _removeOldSignonsFiles : function() {
        // We've used a number of prefs over time due to compatibility issues.
        // Skip the first entry (the newest) and delete the others.
        for (var i = 1; i < this._filenamePrefs.length; i++) {
            var prefname = this._filenamePrefs[i];
            var filename = this._prefBranch.getCharPref(prefname);
            var file = this._profileDir.clone();
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
     * _upgrade_entry_to_2E
     *
     * Updates the format of an entry from 2D to 2E. Returns an array of
     * logins (1 or 2), as sometimes updating an entry requires creating an
     * extra login.
     */
    _upgrade_entry_to_2E : function (aLogin) {
        var upgradedLogins = [aLogin];

        /*
         * For logins stored from HTTP channels
         *    - scheme needs to be derived and prepended
         *    - blank or missing realm becomes same as hostname.
         *
         *  "site.com:80"  --> "http://site.com"
         *  "site.com:443" --> "https://site.com"
         *  "site.com:123" --> Who knows! (So add both)
         *
         * Note: For HTTP logins, the hostname never contained a username
         *       or password. EG "user@site.com:80" shouldn't ever happen.
         *
         * Note: Proxy logins are also stored in this format.
         */
        if (aLogin.hostname.indexOf("://") == -1) {
            var oldHost = aLogin.hostname;

            // Check for a trailing port number, EG "site.com:80". If there's
            // no port, it wasn't saved by the browser and is probably some
            // arbitrary string picked by an extension.
            if (!/:\d+$/.test(aLogin.hostname)) {
                this.log("2E upgrade: no port, skipping " + aLogin.hostname);
                return upgradedLogins;
            }

            // Parse out "host:port".
            try {
                // Small hack: Need a scheme for nsIURI, so just prepend http.
                // We'll check for a port == -1 in case nsIURI ever starts
                // noticing that "http://foo:80" is using the default port.
                var uri = this._ioService.newURI("http://" + aLogin.hostname,
                                                 null, null);
                var host = uri.host;
                var port = uri.port;
            } catch (e) {
                this.log("2E upgrade: Can't parse hostname " + aLogin.hostname);
                return upgradedLogins;
            }

            if (port == 80 || port == -1)
                aLogin.hostname = "http://" + host;
            else if (port == 443)
                aLogin.hostname = "https://" + host;
            else {
                // Not a standard port! Could be either http or https!
                // (Or maybe it's a proxy login!) To try and avoid
                // breaking logins, we'll add *both* http and https
                // versions.
                this.log("2E upgrade: Cloning login for " + aLogin.hostname);

                aLogin.hostname = "http://" + host + ":" + port;

                var extraLogin = new this._nsLoginInfo();
                extraLogin.init("https://" + host + ":" + port,
                                null, aLogin.httpRealm,
                                aLogin.username, aLogin.password, "", "");
                // We don't have decrypted values, unless we're importing from IE,
                // so clone the encrypted bits into the new entry.
                extraLogin.wrappedJSObject.encryptedPassword = 
                    aLogin.wrappedJSObject.encryptedPassword;
                extraLogin.wrappedJSObject.encryptedUsername = 
                    aLogin.wrappedJSObject.encryptedUsername;

                if (extraLogin.httpRealm == "")
                    extraLogin.httpRealm = extraLogin.hostname;
                
                upgradedLogins.push(extraLogin);
            }

            // If the server didn't send a realm (or it was blank), we
            // previously didn't store anything.
            if (aLogin.httpRealm == "")
                aLogin.httpRealm = aLogin.hostname;

            this.log("2E upgrade: " + oldHost + " ---> " + aLogin.hostname);

            return upgradedLogins;
        }


        /*
         * For form logins and non-HTTP channel logins (both were stored in
         * the same format):
         *
         * Standardize URLs (.hostname and .actionURL)
         *    - remove default port numbers, if specified
         *      "http://site.com:80"  --> "http://site.com"
         *    - remove usernames from URL (may move into aLogin.username)
         *      "ftp://user@site.com" --> "ftp://site.com"
         *
         * Note: Passwords in the URL ("foo://user:pass@site.com") were not
         *       stored in FF2, so no need to try to move the value into
         *       aLogin.password.
         */

        // closures in cleanupURL
        var ioService = this._ioService;
        var log = this.log;

        function cleanupURL(aURL, allowJS) {
            var newURL, username = null, pathname = "";

            try {
                var uri = ioService.newURI(aURL, null, null);
                var scheme = uri.scheme;

                if (allowJS && scheme == "javascript")
                    return ["javascript:", null, ""];

                newURL = scheme + "://" + uri.host;

                // If the URL explicitly specified a port, only include it when
                // it's not the default. (We never want "http://foo.com:80")
                port = uri.port;
                if (port != -1) {
                    var handler = ioService.getProtocolHandler(scheme);
                    if (port != handler.defaultPort)
                        newURL += ":" + port;
                }

                // Could be a channel login with a username. 
                if (scheme != "http" && scheme != "https" && uri.username)
                    username = uri.username;

                if (uri.path != "/")
                    pathname = uri.path;

            } catch (e) {
                log("Can't cleanup URL: " + aURL + " e: " + e);
                newURL = aURL;
            }

            if (newURL != aURL)
                log("2E upgrade: " + aURL + " ---> " + newURL);

            return [newURL, username, pathname];
        }

        const isMailNews = /^(ldaps?|smtp|imap|news|mailbox):\/\//;

        // Old mailnews logins were protocol logins with a username/password
        // field name set.
        var isFormLogin = (aLogin.formSubmitURL ||
                           aLogin.usernameField ||
                           aLogin.passwordField) &&
                          !isMailNews.test(aLogin.hostname);

        var [hostname, username, pathname] = cleanupURL(aLogin.hostname);
        aLogin.hostname = hostname;

        // If a non-HTTP URL contained a username, it wasn't stored in the
        // encrypted username field (which contains an encrypted empty value)
        // (Don't do this if it's a form login, though.)
        if (username && !isFormLogin) {
            if (isMailNews.test(aLogin.hostname))
                username = decodeURIComponent(username);

            var [encUsername, userCanceled] = this._encrypt(username);
            if (!userCanceled)
                aLogin.wrappedJSObject.encryptedUsername = encUsername;
        }


        if (aLogin.formSubmitURL) {
            [hostname, username, pathname] = cleanupURL(aLogin.formSubmitURL,
                                                        true);
            aLogin.formSubmitURL = hostname;
            // username, if any, ignored.
        }


        /*
         * For logins stored from non-HTTP channels
         *    - Set httpRealm so they don't look like form logins
         *     "ftp://site.com" --> "ftp://site.com (ftp://site.com)"
         *
         * Tricky: Form logins and non-HTTP channel logins are stored in the
         * same format, and we don't want to add a realm to a form login.
         * Form logins have field names, so only update the realm if there are
         * no field names set. [Any login with a http[s]:// hostname is always
         * a form login, so explicitly ignore those just to be safe.]
         */
        const isHTTP = /^https?:\/\//;
        const isLDAP = /^ldaps?:\/\//;
        const isNews = /^news?:\/\//;
        if (!isHTTP.test(aLogin.hostname) && !isFormLogin) {
            // LDAP and News logins need to keep the path.
            if (isLDAP.test(aLogin.hostname) ||
                isNews.test(aLogin.hostname))
                aLogin.httpRealm = aLogin.hostname + pathname;
            else
                aLogin.httpRealm = aLogin.hostname;

            aLogin.formSubmitURL = null;

            // Null out the form items because mailnews will no longer treat
            // or expect these as form logins
            if (isMailNews.test(aLogin.hostname)) {
                aLogin.usernameField = "";
                aLogin.passwordField = "";
            }

            this.log("2E upgrade: set empty realm to " + aLogin.httpRealm);
        }

        return upgradedLogins;
    },


    /*
     * _readFile
     *
     */
    _readFile : function () {
        var formatVersion;

        this.log("Reading passwords from " + this._signonsFile.path);

        // If it doesn't exist, just bail out.
        if (!this._signonsFile.exists()) {
            this.log("No existing signons file found.");
            return;
        }

        var inputStream = Cc["@mozilla.org/network/file-input-stream;1"].
                          createInstance(Ci.nsIFileInputStream);
        // init the stream as RD_ONLY, -1 == default permissions.
        inputStream.init(this._signonsFile, 0x01, -1, null);
        var lineStream = inputStream.QueryInterface(Ci.nsILineInputStream);
        var line = { value: "" };

        const STATE = { HEADER : 0, REJECT : 1, REALM : 2,
                        USERFIELD : 3, USERVALUE : 4,
                        PASSFIELD : 5, PASSVALUE : 6, ACTIONURL : 7,
                        FILLER : 8 };
        var parseState = STATE.HEADER;

        var processEntry = false;
        var discardEntry = false;

        do {
            var hasMore = lineStream.readLine(line);
            try {
              line.value = this._utfConverter.ConvertToUnicode(line.value);
            } catch (e) {
              this.log("Bad UTF8 conversion: " + line.value);
              this._utfConverterReset();
            }

            switch (parseState) {
                // Check file header
                case STATE.HEADER:
                    if (line.value == "#2c") {
                        formatVersion = 0x2c;
                    } else if (line.value == "#2d") {
                        formatVersion = 0x2d;
                    } else if (line.value == "#2e") {
                        formatVersion = 0x2e;
                    } else {
                        this.log("invalid file header (" + line.value + ")");
                        throw "invalid file header in signons file";
                        // We could disable later writing to file, so we
                        // don't clobber whatever it is. ...however, that
                        // would mean corrupt files are not self-healing.
                        return;
                    }
                    parseState++;
                    break;

                // Line is a hostname for which passwords should never be saved.
                case STATE.REJECT:
                    if (line.value == ".") {
                        parseState++;
                        break;
                    }

                    this._disabledHosts[line.value] = true;

                    break;

                // Line is a hostname, saved login(s) will follow
                case STATE.REALM:
                    var hostrealm = line.value;

                    // Format is "http://site.com", with "(some realm)"
                    // appended if it's a HTTP-Auth login.
                    const realmFormat = /^(.+?)( \(.*\))?$/;
                    var matches = realmFormat.exec(hostrealm);
                    var hostname, httpRealm;
                    if (matches && matches.length == 3) {
                        hostname  = matches[1];
                        httpRealm = matches[2] ?
                                        matches[2].slice(2, -1) : null;
                    } else {
                        if (hostrealm != "") {
                            // Uhoh. This shouldn't happen, but try to deal.
                            this.log("Error parsing host/realm: " + hostrealm);
                        }
                        hostname = hostrealm;
                        httpRealm = null;
                    }

                    parseState++;
                    break;

                // Line is the HTML 'name' attribute for the username field
                // (or "." to indicate end of hostrealm)
                case STATE.USERFIELD:
                    if (line.value == ".") {
                        discardEntry = false;
                        parseState = STATE.REALM;
                        break;
                    }

                    // If we're discarding the entry, keep looping in this
                    // state until we hit the "." marking the end of the entry.
                    if (discardEntry)
                        break;

                    var entry = new this._nsLoginInfo();
                    entry.hostname  = hostname;
                    entry.httpRealm = httpRealm;

                    entry.usernameField = line.value;
                    parseState++;
                    break;

                // Line is a username
                case STATE.USERVALUE:
                    entry.wrappedJSObject.encryptedUsername = line.value;
                    parseState++;
                    break;

                // Line is the HTML 'name' attribute for the password field,
                // with a leading '*' character
                case STATE.PASSFIELD:
                    if (line.value.charAt(0) != '*') {
                        discardEntry = true;
                        entry = null;
                        parseState = STATE.USERFIELD;
                        break;
                    }
                    entry.passwordField = line.value.substr(1);
                    parseState++;
                    break;

                // Line is a password
                case STATE.PASSVALUE:
                    entry.wrappedJSObject.encryptedPassword = line.value;

                    // Version 2C doesn't have an ACTIONURL  line, so
                    // process entry now.
                    if (formatVersion < 0x2d)
                        processEntry = true;

                    parseState++;
                    break;

                // Line is the action URL
                case STATE.ACTIONURL:
                    var formSubmitURL = line.value;
                    if (!formSubmitURL && entry.httpRealm != null)
                        entry.formSubmitURL = null;
                    else
                        entry.formSubmitURL = formSubmitURL;

                    // Version 2D doesn't have a FILLER line, so
                    // process entry now.
                    if (formatVersion < 0x2e)
                        processEntry = true;

                    parseState++;
                    break;

                // Line is unused filler for future use
                case STATE.FILLER:
                    // Save the line's value (so we can dump it back out when
                    // we save the file next time) for forwards compatability.
                    entry.wrappedJSObject.filler = line.value;
                    processEntry = true;

                    parseState++;
                    break;
            }

            // If we've read all the lines for the current entry,
            // process it and reset the parse state for the next entry.
            if (processEntry) {
                if (formatVersion < 0x2d) {
                    // A blank, non-null value is handled as a wildcard.
                    if (entry.httpRealm != null)
                        entry.formSubmitURL = null;
                    else
                        entry.formSubmitURL = "";
                }

                // Upgrading an entry to 2E can sometimes result in the need
                // to create an extra login.
                var entries = [entry];
                if (formatVersion < 0x2e)
                    entries = this._upgrade_entry_to_2E(entry);


                for each (var e in entries) {
                    if (!this._logins[e.hostname])
                        this._logins[e.hostname] = [];
                    this._logins[e.hostname].push(e);
                }

                entry = null;
                processEntry = false;
                parseState = STATE.USERFIELD;
            }
        } while (hasMore);

        lineStream.close();

        return;
    },


    /*
     * _writeFile
     *
     * Returns true if the operation was successfully completed, or false
     * if there was an error (probably the user refusing to enter a
     * master password if prompted).
     */
    _writeFile : function () {
        var converter = this._utfConverter;
        function writeLine(data) {
            data = converter.ConvertFromUnicode(data);
            data += converter.Finish();
            data += "\r\n";
            outputStream.write(data, data.length);
        }

        this.log("Writing passwords to " + this._signonsFile.path);

        var safeStream = Cc["@mozilla.org/network/safe-file-output-stream;1"].
                         createInstance(Ci.nsIFileOutputStream);
        // WR_ONLY|CREAT|TRUNC
        safeStream.init(this._signonsFile, 0x02 | 0x08 | 0x20, 0600, null);

        var outputStream = Cc["@mozilla.org/network/buffered-output-stream;1"].
                           createInstance(Ci.nsIBufferedOutputStream);
        outputStream.init(safeStream, 8192);
        outputStream.QueryInterface(Ci.nsISafeOutputStream); // for .finish()


        // write file version header
        writeLine("#2e");

        // write disabled logins list
        for (var hostname in this._disabledHosts) {
            writeLine(hostname);
        }

        // write end-of-reject-list marker
        writeLine(".");

        for (var hostname in this._logins) {
            function sortByRealm(a,b) {
                a = a.httpRealm;
                b = b.httpRealm;

                if (!a && !b)
                    return  0;

                if (!a || a < b)
                    return -1;

                if (!b || b > a)
                    return  1;

                return 0; // a==b, neither is null
            }

            // Sort logins by httpRealm. This allows us to group multiple
            // logins for the same realm together.
            this._logins[hostname].sort(sortByRealm);


            // write each login known for the host
            var lastRealm = null;
            var firstEntry = true;
            var userCanceled = false;
            for each (var login in this._logins[hostname]) {

                // If this login is for a new realm, start a new entry.
                if (login.httpRealm != lastRealm || firstEntry) {
                    // end previous entry, if needed.
                    if (!firstEntry)
                        writeLine(".");

                    var hostrealm = login.hostname;
                    if (login.httpRealm)
                        hostrealm += " (" + login.httpRealm + ")";

                    writeLine(hostrealm);
                }

                firstEntry = false;

                // Get the encrypted value of the username. Newly added
                // logins will need the plaintext value encrypted.
                var encUsername = login.wrappedJSObject.encryptedUsername;
                if (!encUsername) {
                    [encUsername, userCanceled] = this._encrypt(login.username);
                    login.wrappedJSObject.encryptedUsername = encUsername;
                }

                if (userCanceled)
                    break;

                // Get the encrypted value of the password. Newly added
                // logins will need the plaintext value encrypted.
                var encPassword = login.wrappedJSObject.encryptedPassword;
                if (!encPassword) {
                    [encPassword, userCanceled] = this._encrypt(login.password);
                    login.wrappedJSObject.encryptedPassword = encPassword;
                }

                if (userCanceled)
                    break;


                writeLine((login.usernameField ?  login.usernameField : ""));
                writeLine(encUsername);
                writeLine("*" +
                    (login.passwordField ?  login.passwordField : ""));
                writeLine(encPassword);
                writeLine((login.formSubmitURL ? login.formSubmitURL : ""));
                if (login.wrappedJSObject.filler)
                    writeLine(login.wrappedJSObject.filler);
                else
                    writeLine("---");

                lastRealm = login.httpRealm;
            }

            if (userCanceled) {
                this.log("User canceled Master Password, aborting write.");
                // .close will cause an abort w/o modifying original file
                outputStream.close();
                return false;
            }

            // write end-of-host marker
            writeLine(".");
        }

        // [if there were no hosts, no end-of-host marker (".") needed]

        outputStream.finish();
        return true;
    },


    /*
     * _decryptLogins
     *
     * Decrypts username and password fields in the provided array of
     * logins. This is deferred from the _readFile() code, so that
     * the user is not prompted for a master password (if set) until
     * the entries are actually used.
     *
     * The entries specified by the array will be decrypted, if possible.
     * An array of successfully decrypted logins will be returned. The return
     * value should be given to external callers (since still-encrypted
     * entries are useless), whereas internal callers generally don't want
     * to lose unencrypted entries (eg, because the user clicked Cancel
     * instead of entering their master password)
     */
    _decryptLogins : function (logins) {
        var result = [], userCanceled = false;

        for each (var login in logins) {
            var decryptedUsername, decryptedPassword;

            [decryptedUsername, userCanceled] =
                this._decrypt(login.wrappedJSObject.encryptedUsername);

            if (userCanceled)
                break;

            [decryptedPassword, userCanceled] =
                this._decrypt(login.wrappedJSObject.encryptedPassword);

            // Probably can't hit this case, but for completeness...
            if (userCanceled)
                break;

            // If decryption failed (corrupt entry?) skip it. Note that we
            // allow password-only logins, so decryptedUsername can be "".
            if (decryptedUsername == null || !decryptedPassword)
                continue;

            // Return copies to the caller. Prevents callers from modifying
            // our internal stoage, and helps avoid keeping decrypted data in
            // memory (although this is fuzzy, because of GC issues).
            var clone = new this._nsLoginInfo();
            clone.init(login.hostname, login.formSubmitURL, login.httpRealm,
                       decryptedUsername, decryptedPassword,
                       login.usernameField, login.passwordField);

            // Old mime64-obscured entries should be opportunistically
            // reencrypted in the new format.
            var recrypted;
            if (login.wrappedJSObject.encryptedUsername &&
                login.wrappedJSObject.encryptedUsername.charAt(0) == '~') {
                  [recrypted, userCanceled] = this._encrypt(decryptedUsername);

                  if (userCanceled)
                    break;

                  login.wrappedJSObject.encryptedUsername = recrypted;
            }

            if (login.wrappedJSObject.encryptedPassword &&
                login.wrappedJSObject.encryptedPassword.charAt(0) == '~') {
                  [recrypted, userCanceled] = this._encrypt(decryptedPassword);

                  if (userCanceled)
                    break;

                  login.wrappedJSObject.encryptedPassword = recrypted;
            }

            result.push(clone);
        }

        return [result, userCanceled];
    },


    /*
     * _encrypt
     *
     * Encrypts the specified string, using the SecretDecoderRing.
     *
     * Returns [cipherText, userCanceled] where:
     *  cipherText   -- the encrypted string, or null if it failed.
     *  userCanceled -- if the encryption failed, this is true if the
     *                  user selected Cancel when prompted to enter their
     *                  Master Password. The caller should bail out, and not
     *                  not request that more things be encrypted (which 
     *                  results in prompting the user for a Master Password
     *                  over and over.)
     */
    _encrypt : function (plainText) {
        var cipherText = null, userCanceled = false;

        try {
            var plainOctet = this._utfConverter.ConvertFromUnicode(plainText);
            plainOctet += this._utfConverter.Finish();
            cipherText = this._decoderRing.encryptString(plainOctet);
        } catch (e) {
            this.log("Failed to encrypt string. (" + e.name + ")");
            // If the user clicks Cancel, we get NS_ERROR_FAILURE.
            // (unlike decrypting, which gets NS_ERROR_NOT_AVAILABLE).
            if (e.result == Components.results.NS_ERROR_FAILURE)
                userCanceled = true;
        }

        return [cipherText, userCanceled];
    },


    /*
     * _decrypt
     *
     * Decrypts the specified string, using the SecretDecoderRing.
     *
     * Returns [plainText, userCanceled] where:
     *  plainText    -- the decrypted string, or null if it failed.
     *  userCanceled -- if the decryption failed, this is true if the
     *                  user selected Cancel when prompted to enter their
     *                  Master Password. The caller should bail out, and not
     *                  not request that more things be decrypted (which 
     *                  results in prompting the user for a Master Password
     *                  over and over.)
     */
    _decrypt : function (cipherText) {
        var plainText = null, userCanceled = false;

        try {
            var plainOctet;
            if (cipherText.charAt(0) == '~') {
                // The older file format obscured entries by
                // base64-encoding them. These entries are signaled by a
                // leading '~' character. 
                plainOctet = atob(cipherText.substring(1));
            } else {
                plainOctet = this._decoderRing.decryptString(cipherText);
            }
            plainText = this._utfConverter.ConvertToUnicode(plainOctet);
        } catch (e) {
            this.log("Failed to decrypt string: " + cipherText +
                " (" + e.name + ")");

            // In the unlikely event the converter threw, reset it.
            this._utfConverterReset();

            // If the user clicks Cancel, we get NS_ERROR_NOT_AVAILABLE.
            // If the cipherText is bad / wrong key, we get NS_ERROR_FAILURE
            // Wrong passwords are handled by the decoderRing reprompting;
            // we get no notification.
            if (e.result == Components.results.NS_ERROR_NOT_AVAILABLE)
                userCanceled = true;
        }

        return [plainText, userCanceled];
    },




    /* ================== nsILoginManagerIEMigratorHelper ================== */




    _migrationLoginManager : null,

    /*
     * migrateAndAddLogin
     *
     * Given a login with IE6-formatted fields, migrates it to the new format
     * and adds it to the login manager.
     *
     * Experimentally derived format of IE6 logins, see:
     *     https://bugzilla.mozilla.org/attachment.cgi?id=319346
     *
     * HTTP AUTH:
     * - hostname is always "example.com:123"
     *   * "example.com", "http://example.com", "http://example.com:80" all
     *     end up as just "example.com:80"
     *   * Entering "example.com:80" in the URL bar isn't recognized as a
     *     valid URL by IE6.
     *   * "https://example.com" is saved as "example.com:443"
     *   * "https://example.com:666" is saved as "example.com:666". Thus, for
     *     non-standard ports we don't know the right scheme, so create both.
     *
     * - an empty or missing "realm" in the WWW-Authenticate reply is stored
     *   as just an empty string by IE6.
     *
     * - IE6 will store logins where one or both (!) of the username/password
     *   is left blank. We don't support logins without a password, so these
     *   logins won't be added [addLogin() will throw].
     *
     * - IE6 won't recognize a URL with and embedded username/password (eg
     *   http://user@example.com, http://user:pass@example.com), so these
     *   shouldn't be encountered.
     *
     * - Our migration code doesn't extract non-HTTP logins (eg, FTP). So
     *   they shouldn't be encountered here. (Verified by saving FTP logins
     *   in IE and then importing in Firefox.)
     *
     *
     * FORM LOGINS:
     * - hostname is "http://site.com" or "https://site.com".
     *   * scheme always included
     *   * default port not included
     * - port numbers, even for non-standard posts, are never present!
     *   unfortunately, this means logins will only work on the default
     *   port, because we don't know what the original was (or even that
     *   it wasn't originally stored for the original port).
     * - Logins are stored without a field name by IE, but we look one up
     *   in the migrator for the username. The password field name will
     *   always be empty-string.
     */
    migrateAndAddLogin : function (aLogin) {
        // Initialize outself on the first call
        if (!this._migrationLoginManager) {
            // Connect to the correct preferences branch.
            this._prefBranch = Cc["@mozilla.org/preferences-service;1"].
                               getService(Ci.nsIPrefService);
            this._prefBranch = this._prefBranch.getBranch("signon.");
            this._prefBranch.QueryInterface(Ci.nsIPrefBranch2);

            this._debug = this._prefBranch.getBoolPref("debug");

            this._migrationLoginManager = Cc["@mozilla.org/login-manager;1"].
                                          getService(Ci.nsILoginManager);
        }

        this.log("Migrating login for " + aLogin.hostname);

        // The IE login is in the same format as the old password
        // manager entries, so just reuse that code.
        var logins = this._upgrade_entry_to_2E(aLogin);

        // Add logins via the login manager (and not this.addLogin),
        // lest an alternative storage module be in use.
        for each (var login in logins)
            this._migrationLoginManager.addLogin(login);
    }
}; // end of nsLoginManagerStorage_legacy implementation

var component = [LoginManagerStorage_legacy];
function NSGetModule(compMgr, fileSpec) {
    return XPCOMUtils.generateModule(component);
}
