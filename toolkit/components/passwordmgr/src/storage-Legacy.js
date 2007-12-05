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
    QueryInterface : XPCOMUtils.generateQI([Ci.nsILoginManagerStorage]),

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

    _prefBranch : null,  // Preferences service

    _signonsFile : null,  // nsIFile for "signons3.txt" (or whatever pref is)
    _debug       : false, // mirrors signon.debug


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
        this._prefBranch = Cc["@mozilla.org/preferences-service;1"]
                                .getService(Ci.nsIPrefService);
        this._prefBranch = this._prefBranch.getBranch("signon.");
        this._prefBranch.QueryInterface(Ci.nsIPrefBranch2);

        this._debug = this._prefBranch.getBoolPref("debug");

        // Check to see if the internal PKCS#11 token has been initialized.
        // If not, set a blank password.
        var tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"]
                            .getService(Ci.nsIPK11TokenDB);

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
        // We rely on using login.wrappedJSObject. addLogin is the
        // only entry point where we might get a nsLoginInfo object
        // that wasn't created by us (and so might not be a JS
        // implementation being wrapped)
        if (!login.wrappedJSObject) {
            var clone = Cc["@mozilla.org/login-manager/loginInfo;1"].
                        createInstance(Ci.nsILoginInfo);
            clone.init(login.hostname, login.formSubmitURL, login.httpRealm,
                       login.username,      login.password,
                       login.usernameField, login.passwordField);
            login = clone;
        }

        var key = login.hostname;

        // If first entry for key, create an Array to hold it's logins.
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

        count.value = result.length; // needed for XPCOM
        return result;
    },


    /*
     * removeAllLogins
     *
     * Removes all logins from storage.
     */
    removeAllLogins : function () {
        this._logins = {};
        // Disabled hosts kept, as one presumably doesn't want to erase those.

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
    countLogins : function (hostname, formSubmitURL, httpRealm) {
        var logins = this._searchLogins(hostname, formSubmitURL, httpRealm);

        return logins.length;
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
     * _getSignonsFile
     *
     * Determines what file to use based on prefs. Returns it as a
     * nsILocalFile, along with a file to import from first (if needed)
     *
     */
    _getSignonsFile : function() {
        var destFile = null, importFile = null;

        // Get the location of the user's profile.
        var DIR_SERVICE = new Components.Constructor(
                "@mozilla.org/file/directory_service;1", "nsIProperties");
        var pathname = (new DIR_SERVICE()).get("ProfD", Ci.nsIFile).path;

        // We've used a number of prefs over time due to compatibility issues.
        // Use the filename specified in the newest pref, but import from
        // older files if needed.
        var prefs = ["SignonFileName3", "SignonFileName2", "SignonFileName"];
        for (var i = 0; i < prefs.length; i++) {
            var prefName = prefs[i];

            var filename = this._prefBranch.getCharPref(prefName);

            this.log("Checking file " + filename + " (" + prefName + ")");

            var file = Cc["@mozilla.org/file/local;1"].
                       createInstance(Ci.nsILocalFile);
            file.initWithPath(pathname);
            file.append(filename);

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

                var extraLogin = Cc["@mozilla.org/login-manager/loginInfo;1"].
                                 createInstance(Ci.nsILoginInfo);
                extraLogin.init("https://" + host + ":" + port,
                                null, aLogin.httpRealm,
                                null, null, "", "");
                // We don't have decrypted values, so clone the encrypted
                // bits into the new entry.
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

        function cleanupURL(aURL) {
            var newURL, username = null;

            try {
                var uri = ioService.newURI(aURL, null, null);

                var scheme = uri.scheme;
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
                
            } catch (e) {
                log("Can't cleanup URL: " + aURL);
                newURL = aURL;
            }

            if (newURL != aURL)
                log("2E upgrade: " + aURL + " ---> " + newURL);

            return [newURL, username];
        }

        var isFormLogin = (aLogin.formSubmitURL ||
                           aLogin.usernameField ||
                           aLogin.passwordField);

        var [hostname, username] = cleanupURL(aLogin.hostname);
        aLogin.hostname = hostname;

        // If a non-HTTP URL contained a username, it wasn't stored in the
        // encrypted username field (which contains an encrypted empty value)
        // (Don't do this if it's a form login, though.)
        if (username && !isFormLogin) {
            var [encUsername, userCanceled] = this._encrypt(username);
            if (!userCanceled)
                aLogin.wrappedJSObject.encryptedUsername = encUsername;
        }


        if (aLogin.formSubmitURL) {
            [hostname, username] = cleanupURL(aLogin.formSubmitURL);
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
         *
         * Bug 403790: mail entries (imap://, ldaps://, mailbox:// smtp:// have
         * fieldnames set to "\=username=\" and "\=password=\" (non-escaping
         * backslash). More work is needed to upgrade these properly.
         */
        const isHTTP = /^https?:\/\//;
        if (!isHTTP.test(aLogin.hostname) && !isFormLogin) {
            aLogin.httpRealm = aLogin.hostname;
            aLogin.formSubmitURL = null;
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

        // If it doesn't exist, just create an empty file and bail out.
        if (!this._signonsFile.exists()) {
            this.log("Creating new signons file...");
            this._writeFile();
            return;
        }

        var inputStream = Cc["@mozilla.org/network/file-input-stream;1"]
                                .createInstance(Ci.nsIFileInputStream);
        // init the stream as RD_ONLY, -1 == default permissions.
        inputStream.init(this._signonsFile, 0x01, -1, null);
        var lineStream = inputStream.QueryInterface(Ci.nsILineInputStream);
        var line = { value: "" };

        const STATE = { HEADER : 0, REJECT : 1, REALM : 2,
                        USERFIELD : 3, USERVALUE : 4,
                        PASSFIELD : 5, PASSVALUE : 6, ACTIONURL : 7,
                        FILLER : 8 };
        var parseState = STATE.HEADER;

        var nsLoginInfo = new Components.Constructor(
                "@mozilla.org/login-manager/loginInfo;1", Ci.nsILoginInfo);
        var processEntry = false;

        do {
            var hasMore = lineStream.readLine(line);

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
                        parseState = STATE.REALM;
                        break;
                    }

                    var entry = new nsLoginInfo();
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
        function writeLine(data) {
            data += "\r\n";
            outputStream.write(data, data.length);
        }

        this.log("Writing passwords to " + this._signonsFile.path);

        var outputStream = Cc["@mozilla.org/network/safe-file-output-stream;1"]
                                .createInstance(Ci.nsIFileOutputStream);
        outputStream.QueryInterface(Ci.nsISafeOutputStream);

        // WR_ONLY|CREAT|TRUNC
        outputStream.init(this._signonsFile, 0x02 | 0x08 | 0x20, 0600, null);

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
            var username, password;

            [username, userCanceled] =
                this._decrypt(login.wrappedJSObject.encryptedUsername);

            if (userCanceled)
                break;

            [password, userCanceled] =
                this._decrypt(login.wrappedJSObject.encryptedPassword);

            // Probably can't hit this case, but for completeness...
            if (userCanceled)
                break;

            // If decryption failed (corrupt entry?) skip it.
            // Note that we allow password-only logins, so username con be "".
            if (username == null || !password)
                continue;

            // We could set the decrypted values on a copy of the object, to
            // try to prevent the decrypted values from sitting around in
            // memory if they're not needed. But thanks to GC that's happening
            // anyway, so meh.
            login.username = username;
            login.password = password;

            // Old mime64-obscured entries need to be reencrypted in the new
            // format.
            if (login.wrappedJSObject.encryptedUsername &&
                login.wrappedJSObject.encryptedUsername.charAt(0) == '~') {
                  [username, userCanceled] = this._encrypt(login.username);

                  if (userCanceled)
                    break;

                  login.wrappedJSObject.encryptedUsername = username;
            }

            if (login.wrappedJSObject.encryptedPassword &&
                login.wrappedJSObject.encryptedPassword.charAt(0) == '~') {

                  [password, userCanceled] = this._encrypt(login.password);

                  if (userCanceled)
                    break;

                  login.wrappedJSObject.encryptedPassword = password;
            }

            result.push(login);
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
            var converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                            createInstance(Ci.nsIScriptableUnicodeConverter);
            converter.charset = "UTF-8";
            var plainOctet = converter.ConvertFromUnicode(plainText);
            plainOctet += converter.Finish();
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
            var converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                              .createInstance(Ci.nsIScriptableUnicodeConverter);
            converter.charset = "UTF-8";
            plainText = converter.ConvertToUnicode(plainOctet);
        } catch (e) {
            this.log("Failed to decrypt string: " + cipherText +
                " (" + e.name + ")");

            // If the user clicks Cancel, we get NS_ERROR_NOT_AVAILABLE.
            // If the cipherText is bad / wrong key, we get NS_ERROR_FAILURE
            // Wrong passwords are handled by the decoderRing reprompting;
            // we get no notification.
            if (e.result == Components.results.NS_ERROR_NOT_AVAILABLE)
                userCanceled = true;
        }

        return [plainText, userCanceled];
    },

}; // end of nsLoginManagerStorage_legacy implementation

var component = [LoginManagerStorage_legacy];
function NSGetModule(compMgr, fileSpec) {
    return XPCOMUtils.generateModule(component);
}
