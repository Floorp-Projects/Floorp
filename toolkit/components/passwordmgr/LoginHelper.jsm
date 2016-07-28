/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Contains functions shared by different Login Manager components.
 *
 * This JavaScript module exists in order to share code between the different
 * XPCOM components that constitute the Login Manager, including implementations
 * of nsILoginManager and nsILoginManagerStorage.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "LoginHelper",
];

////////////////////////////////////////////////////////////////////////////////
//// Globals

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

////////////////////////////////////////////////////////////////////////////////
//// LoginHelper

/**
 * Contains functions shared by different Login Manager components.
 */
this.LoginHelper = {
  /**
   * Warning: these only update if a logger was created.
   */
  debug: Services.prefs.getBoolPref("signon.debug"),
  formlessCaptureEnabled: Services.prefs.getBoolPref("signon.formlessCapture.enabled"),
  schemeUpgrades: Services.prefs.getBoolPref("signon.schemeUpgrades"),

  createLogger(aLogPrefix) {
    let getMaxLogLevel = () => {
      return this.debug ? "debug" : "warn";
    };

    // Create a new instance of the ConsoleAPI so we can control the maxLogLevel with a pref.
    let ConsoleAPI = Cu.import("resource://gre/modules/Console.jsm", {}).ConsoleAPI;
    let consoleOptions = {
      maxLogLevel: getMaxLogLevel(),
      prefix: aLogPrefix,
    };
    let logger = new ConsoleAPI(consoleOptions);

    // Watch for pref changes and update this.debug and the maxLogLevel for created loggers
    Services.prefs.addObserver("signon.", () => {
      this.debug = Services.prefs.getBoolPref("signon.debug");
      this.formlessCaptureEnabled = Services.prefs.getBoolPref("signon.formlessCapture.enabled");
      this.schemeUpgrades = Services.prefs.getBoolPref("signon.schemeUpgrades");
      logger.maxLogLevel = getMaxLogLevel();
    }, false);

    return logger;
  },

  /**
   * Due to the way the signons2.txt file is formatted, we need to make
   * sure certain field values or characters do not cause the file to
   * be parsed incorrectly.  Reject hostnames that we can't store correctly.
   *
   * @throws String with English message in case validation failed.
   */
  checkHostnameValue(aHostname) {
    // Nulls are invalid, as they don't round-trip well.  Newlines are also
    // invalid for any field stored as plaintext, and a hostname made of a
    // single dot cannot be stored in the legacy format.
    if (aHostname == "." ||
        aHostname.indexOf("\r") != -1 ||
        aHostname.indexOf("\n") != -1 ||
        aHostname.indexOf("\0") != -1) {
      throw new Error("Invalid hostname");
    }
  },

  /**
   * Due to the way the signons2.txt file is formatted, we need to make
   * sure certain field values or characters do not cause the file to
   * be parsed incorrectly.  Reject logins that we can't store correctly.
   *
   * @throws String with English message in case validation failed.
   */
  checkLoginValues(aLogin) {
    function badCharacterPresent(l, c)
    {
      return ((l.formSubmitURL && l.formSubmitURL.indexOf(c) != -1) ||
              (l.httpRealm     && l.httpRealm.indexOf(c)     != -1) ||
                                  l.hostname.indexOf(c)      != -1  ||
                                  l.usernameField.indexOf(c) != -1  ||
                                  l.passwordField.indexOf(c) != -1);
    }

    // Nulls are invalid, as they don't round-trip well.
    // Mostly not a formatting problem, although ".\0" can be quirky.
    if (badCharacterPresent(aLogin, "\0")) {
      throw new Error("login values can't contain nulls");
    }

    // In theory these nulls should just be rolled up into the encrypted
    // values, but nsISecretDecoderRing doesn't use nsStrings, so the
    // nulls cause truncation. Check for them here just to avoid
    // unexpected round-trip surprises.
    if (aLogin.username.indexOf("\0") != -1 ||
        aLogin.password.indexOf("\0") != -1) {
      throw new Error("login values can't contain nulls");
    }

    // Newlines are invalid for any field stored as plaintext.
    if (badCharacterPresent(aLogin, "\r") ||
        badCharacterPresent(aLogin, "\n")) {
      throw new Error("login values can't contain newlines");
    }

    // A line with just a "." can have special meaning.
    if (aLogin.usernameField == "." ||
        aLogin.formSubmitURL == ".") {
      throw new Error("login values can't be periods");
    }

    // A hostname with "\ \(" won't roundtrip.
    // eg host="foo (", realm="bar" --> "foo ( (bar)"
    // vs host="foo", realm=" (bar" --> "foo ( (bar)"
    if (aLogin.hostname.indexOf(" (") != -1) {
      throw new Error("bad parens in hostname");
    }
  },

  /**
   * Returns a new XPCOM property bag with the provided properties.
   *
   * @param {Object} aProperties
   *        Each property of this object is copied to the property bag.  This
   *        parameter can be omitted to return an empty property bag.
   *
   * @return A new property bag, that is an instance of nsIWritablePropertyBag,
   *         nsIWritablePropertyBag2, nsIPropertyBag, and nsIPropertyBag2.
   */
  newPropertyBag(aProperties) {
    let propertyBag = Cc["@mozilla.org/hash-property-bag;1"]
                      .createInstance(Ci.nsIWritablePropertyBag);
    if (aProperties) {
      for (let [name, value] of Iterator(aProperties)) {
        propertyBag.setProperty(name, value);
      }
    }
    return propertyBag.QueryInterface(Ci.nsIPropertyBag)
                      .QueryInterface(Ci.nsIPropertyBag2)
                      .QueryInterface(Ci.nsIWritablePropertyBag2);
  },

  /**
   * Helper to avoid the `count` argument and property bags when calling
   * Services.logins.searchLogins from JS.
   *
   * @param {Object} aSearchOptions - A regular JS object to copy to a property bag before searching
   * @return {nsILoginInfo[]} - The result of calling searchLogins.
   */
  searchLoginsWithObject(aSearchOptions) {
    return Services.logins.searchLogins({}, this.newPropertyBag(aSearchOptions));
  },

  /**
   * @param {String} aLoginOrigin - An origin value from a stored login's
   *                                hostname or formSubmitURL properties.
   * @param {String} aSearchOrigin - The origin that was are looking to match
   *                                 with aLoginOrigin. This would normally come
   *                                 from a form or page that we are considering.
   * @param {nsILoginFindOptions} aOptions - Options to affect whether the origin
   *                                         from the login (aLoginOrigin) is a
   *                                         match for the origin we're looking
   *                                         for (aSearchOrigin).
   */
  isOriginMatching(aLoginOrigin, aSearchOrigin, aOptions = {
    schemeUpgrades: false,
  }) {
    if (aLoginOrigin == aSearchOrigin) {
      return true;
    }

    if (!aOptions) {
      return false;
    }

    if (aOptions.schemeUpgrades) {
      try {
        let loginURI = Services.io.newURI(aLoginOrigin, null, null);
        let searchURI = Services.io.newURI(aSearchOrigin, null, null);
        if (loginURI.scheme == "http" && searchURI.scheme == "https" &&
            loginURI.hostPort == searchURI.hostPort) {
          return true;
        }
      } catch (ex) {
        // newURI will throw for some values e.g. chrome://FirefoxAccounts
        return false;
      }
    }

    return false;
  },

  doLoginsMatch(aLogin1, aLogin2, {
    ignorePassword = false,
    ignoreSchemes = false,
  }) {
    if (aLogin1.httpRealm != aLogin2.httpRealm ||
        aLogin1.username != aLogin2.username)
      return false;

    if (!ignorePassword && aLogin1.password != aLogin2.password)
      return false;

    if (ignoreSchemes) {
      let hostname1URI = Services.io.newURI(aLogin1.hostname, null, null);
      let hostname2URI = Services.io.newURI(aLogin2.hostname, null, null);
      if (hostname1URI.hostPort != hostname2URI.hostPort)
        return false;

      if (aLogin1.formSubmitURL != "" && aLogin2.formSubmitURL != "" &&
          Services.io.newURI(aLogin1.formSubmitURL, null, null).hostPort !=
          Services.io.newURI(aLogin2.formSubmitURL, null, null).hostPort)
        return false;
    } else {
      if (aLogin1.hostname != aLogin2.hostname)
        return false;

      // If either formSubmitURL is blank (but not null), then match.
      if (aLogin1.formSubmitURL != "" && aLogin2.formSubmitURL != "" &&
          aLogin1.formSubmitURL != aLogin2.formSubmitURL)
        return false;
    }

    // The .usernameField and .passwordField values are ignored.

    return true;
  },

  /**
   * Creates a new login object that results by modifying the given object with
   * the provided data.
   *
   * @param aOldStoredLogin
   *        Existing nsILoginInfo object to modify.
   * @param aNewLoginData
   *        The new login values, either as nsILoginInfo or nsIProperyBag.
   *
   * @return The newly created nsILoginInfo object.
   *
   * @throws String with English message in case validation failed.
   */
  buildModifiedLogin(aOldStoredLogin, aNewLoginData) {
    function bagHasProperty(aPropName) {
      try {
        aNewLoginData.getProperty(aPropName);
        return true;
      } catch (ex) { }
      return false;
    }

    aOldStoredLogin.QueryInterface(Ci.nsILoginMetaInfo);

    let newLogin;
    if (aNewLoginData instanceof Ci.nsILoginInfo) {
      // Clone the existing login to get its nsILoginMetaInfo, then init it
      // with the replacement nsILoginInfo data from the new login.
      newLogin = aOldStoredLogin.clone();
      newLogin.init(aNewLoginData.hostname,
                    aNewLoginData.formSubmitURL, aNewLoginData.httpRealm,
                    aNewLoginData.username, aNewLoginData.password,
                    aNewLoginData.usernameField, aNewLoginData.passwordField);
      newLogin.QueryInterface(Ci.nsILoginMetaInfo);

      // Automatically update metainfo when password is changed.
      if (newLogin.password != aOldStoredLogin.password) {
        newLogin.timePasswordChanged = Date.now();
      }
    } else if (aNewLoginData instanceof Ci.nsIPropertyBag) {
      // Clone the existing login, along with all its properties.
      newLogin = aOldStoredLogin.clone();
      newLogin.QueryInterface(Ci.nsILoginMetaInfo);

      // Automatically update metainfo when password is changed.
      // (Done before the main property updates, lest the caller be
      // explicitly updating both .password and .timePasswordChanged)
      if (bagHasProperty("password")) {
        let newPassword = aNewLoginData.getProperty("password");
        if (newPassword != aOldStoredLogin.password) {
          newLogin.timePasswordChanged = Date.now();
        }
      }

      let propEnum = aNewLoginData.enumerator;
      while (propEnum.hasMoreElements()) {
        let prop = propEnum.getNext().QueryInterface(Ci.nsIProperty);
        switch (prop.name) {
          // nsILoginInfo
          case "hostname":
          case "httpRealm":
          case "formSubmitURL":
          case "username":
          case "password":
          case "usernameField":
          case "passwordField":
          // nsILoginMetaInfo
          case "guid":
          case "timeCreated":
          case "timeLastUsed":
          case "timePasswordChanged":
          case "timesUsed":
            newLogin[prop.name] = prop.value;
            break;

          // Fake property, allows easy incrementing.
          case "timesUsedIncrement":
            newLogin.timesUsed += prop.value;
            break;

          // Fail if caller requests setting an unknown property.
          default:
            throw new Error("Unexpected propertybag item: " + prop.name);
        }
      }
    } else {
      throw new Error("newLoginData needs an expected interface!");
    }

    // Sanity check the login
    if (newLogin.hostname == null || newLogin.hostname.length == 0) {
      throw new Error("Can't add a login with a null or empty hostname.");
    }

    // For logins w/o a username, set to "", not null.
    if (newLogin.username == null) {
      throw new Error("Can't add a login with a null username.");
    }

    if (newLogin.password == null || newLogin.password.length == 0) {
      throw new Error("Can't add a login with a null or empty password.");
    }

    if (newLogin.formSubmitURL || newLogin.formSubmitURL == "") {
      // We have a form submit URL. Can't have a HTTP realm.
      if (newLogin.httpRealm != null) {
        throw new Error("Can't add a login with both a httpRealm and formSubmitURL.");
      }
    } else if (newLogin.httpRealm) {
      // We have a HTTP realm. Can't have a form submit URL.
      if (newLogin.formSubmitURL != null) {
        throw new Error("Can't add a login with both a httpRealm and formSubmitURL.");
      }
    } else {
      // Need one or the other!
      throw new Error("Can't add a login without a httpRealm or formSubmitURL.");
    }

    // Throws if there are bogus values.
    this.checkLoginValues(newLogin);

    return newLogin;
  },

  /**
   * Removes duplicates from a list of logins while preserving the sort order.
   *
   * @param {nsILoginInfo[]} logins
   *        A list of logins we want to deduplicate.
   * @param {string[]} [uniqueKeys = ["username", "password"]]
   *        A list of login attributes to use as unique keys for the deduplication.
   * @param {string[]} [resolveBy = ["timeLastUsed"]]
   *        Ordered array of keyword strings used to decide which of the
   *        duplicates should be used. "scheme" would prefer the login that has
   *        a scheme matching `preferredOrigin`'s if there are two logins with
   *        the same `uniqueKeys`. The default preference to distinguish two
   *        logins is `timeLastUsed`. If there is no preference between two
   *        logins, the first one found wins.
   * @param {string} [preferredOrigin = undefined]
   *        String representing the origin to use for preferring one login over
   *        another when they are dupes. This is used with "scheme" for
   *        `resolveBy` so the scheme from this origin will be preferred.
   *
   * @returns {nsILoginInfo[]} list of unique logins.
   */
  dedupeLogins(logins, uniqueKeys = ["username", "password"],
               resolveBy = ["timeLastUsed"],
               preferredOrigin = undefined) {
    const KEY_DELIMITER = ":";

    if (!preferredOrigin && resolveBy.includes("scheme")) {
      throw new Error("dedupeLogins: `preferredOrigin` is required in order to "+
                      "prefer schemes which match it.");
    }

    let preferredOriginScheme;
    if (preferredOrigin) {
      try {
        preferredOriginScheme = Services.io.newURI(preferredOrigin, null, null).scheme;
      } catch (ex) {
        // Handle strings that aren't valid URIs e.g. chrome://FirefoxAccounts
      }
    }

    if (!preferredOriginScheme && resolveBy.includes("scheme")) {
      log.warn("dedupeLogins: Deduping with a scheme preference but couldn't " +
               "get the preferred origin scheme.");
    }

    // We use a Map to easily lookup logins by their unique keys.
    let loginsByKeys = new Map();

    // Generate a unique key string from a login.
    function getKey(login, uniqueKeys) {
      return uniqueKeys.reduce((prev, key) => prev + KEY_DELIMITER + login[key], "");
    }

    /**
     * @return {bool} whether `login` is preferred over its duplicate (considering `uniqueKeys`)
     *                `existingLogin`.
     *
     * `resolveBy` is a sorted array so we can return true the first time `login` is preferred
     * over the existingLogin.
     */
    function isLoginPreferred(existingLogin, login) {
      if (!resolveBy || resolveBy.length == 0) {
        // If there is no preference, prefer the existing login.
        return false;
      }

      for (let preference of resolveBy) {
        switch (preference) {
          case "scheme": {
            if (!preferredOriginScheme) {
              break;
            }

            try {
              // Only `hostname` is currently considered
              let existingLoginURI = Services.io.newURI(existingLogin.hostname, null, null);
              let loginURI = Services.io.newURI(login.hostname, null, null);
              // If the schemes of the two logins are the same or neither match the
              // preferredOriginScheme then we have no preference and look at the next resolveBy.
              if (loginURI.scheme == existingLoginURI.scheme ||
                  (loginURI.scheme != preferredOriginScheme &&
                   existingLoginURI.scheme != preferredOriginScheme)) {
                break;
              }

              return loginURI.scheme == preferredOriginScheme;
            } catch (ex) {
              // Some URLs aren't valid nsIURI (e.g. chrome://FirefoxAccounts)
              log.debug("dedupeLogins/shouldReplaceExisting: Error comparing schemes:",
                        existingLogin.hostname, login.hostname,
                        "preferredOrigin:", preferredOrigin, ex);
            }
            break;
          }
          case "timeLastUsed":
          case "timePasswordChanged": {
            // If we find a more recent login for the same key, replace the existing one.
            let loginDate = login.QueryInterface(Ci.nsILoginMetaInfo)[preference];
            let storedLoginDate = existingLogin.QueryInterface(Ci.nsILoginMetaInfo)[preference];
            if (loginDate == storedLoginDate) {
              break;
            }

            return loginDate > storedLoginDate;
          }
          default: {
            throw new Error("dedupeLogins: Invalid resolveBy preference: " + preference);
          }
        }
      }

      return false;
    }

    for (let login of logins) {
      let key = getKey(login, uniqueKeys);

      if (loginsByKeys.has(key)) {
        if (!isLoginPreferred(loginsByKeys.get(key), login)) {
          // If there is no preference for the new login, use the existing one.
          continue;
        }
      }
      loginsByKeys.set(key, login);
    }

    // Return the map values in the form of an array.
    return [...loginsByKeys.values()];
  },

  /**
   * Open the password manager window.
   *
   * @param {Window} window
   *                 the window from where we want to open the dialog
   *
   * @param {string} [filterString=""]
   *                 the filterString parameter to pass to the login manager dialog
   */
  openPasswordManager(window, filterString = "") {
    let win = Services.wm.getMostRecentWindow("Toolkit:PasswordManager");
    if (win) {
      win.setFilter(filterString);
      win.focus();
    } else {
      window.openDialog("chrome://passwordmgr/content/passwordManager.xul",
                        "Toolkit:PasswordManager", "",
                        {filterString : filterString});
    }
  },

  /**
   * Checks if a field type is username compatible.
   *
   * @param {Element} element
   *                  the field we want to check.
   *
   * @returns {Boolean} true if the field type is one
   *                    of the username types.
   */
  isUsernameFieldType(element) {
    if (!(element instanceof Ci.nsIDOMHTMLInputElement))
      return false;

    let fieldType = (element.hasAttribute("type") ?
                     element.getAttribute("type").toLowerCase() :
                     element.type);
    if (fieldType == "text"  ||
        fieldType == "email" ||
        fieldType == "url"   ||
        fieldType == "tel"   ||
        fieldType == "number") {
      return true;
    }
    return false;
  },

  /**
   * Add the login to the password manager if a similar one doesn't already exist. Merge it
   * otherwise with the similar existing ones.
   * @param {Object} loginData - the data about the login that needs to be added.
   */
  maybeImportLogin(loginData) {
    // create a new login
    let login = Cc["@mozilla.org/login-manager/loginInfo;1"].createInstance(Ci.nsILoginInfo);
    login.init(loginData.hostname,
               loginData.submitURL || (typeof(loginData.httpRealm) == "string" ? null : ""),
               typeof(loginData.httpRealm) == "string" ? loginData.httpRealm : null,
               loginData.username,
               loginData.password,
               loginData.usernameElement || "",
               loginData.passwordElement || "");

    login.QueryInterface(Ci.nsILoginMetaInfo);
    login.timeCreated = loginData.timeCreated;
    login.timeLastUsed = loginData.timeLastUsed || loginData.timeCreated;
    login.timePasswordChanged = loginData.timePasswordChanged  || loginData.timeCreated;
    login.timesUsed = loginData.timesUsed || 1;
    // While here we're passing formSubmitURL and httpRealm, they could be empty/null and get
    // ignored in that case, leading to multiple logins for the same username.
    let existingLogins = Services.logins.findLogins({}, login.hostname,
                                                    login.formSubmitURL,
                                                    login.httpRealm);
    // Add the login only if it doesn't already exist
    // if the login is not already available, it's going to be added or merged with other
    // logins
    if (existingLogins.some(l => login.matches(l, true))) {
      return;
    }
    // the login is just an update for an old one or the login is older than an existing one
    let foundMatchingLogin = false;
    for (let existingLogin of existingLogins) {
      if (login.username == existingLogin.username) {
        // Bug 1187190: Password changes should be propagated depending on timestamps.
        // this an old login or a just an update, so make sure not to add it
        foundMatchingLogin = true;
        if(login.password != existingLogin.password &
           login.timePasswordChanged > existingLogin.timePasswordChanged) {
          // if a login with the same username and different password already exists and it's older
          // than the current one, that login needs to be updated using the current one details

          // the existing login password and timestamps should be updated
          let propBag = Cc["@mozilla.org/hash-property-bag;1"].
                        createInstance(Ci.nsIWritablePropertyBag);
          propBag.setProperty("password", login.password);
          propBag.setProperty("timePasswordChanged", login.timePasswordChanged);
          Services.logins.modifyLogin(existingLogin, propBag);
        }
      }
    }
    // if the new login is an update or is older than an exiting login, don't add it.
    if (foundMatchingLogin) {
      return;
    }
    Services.logins.addLogin(login);
  },

  /**
   * Convert an array of nsILoginInfo to vanilla JS objects suitable for
   * sending over IPC.
   *
   * NB: All members of nsILoginInfo and nsILoginMetaInfo are strings.
   */
  loginsToVanillaObjects(logins) {
    return logins.map(this.loginToVanillaObject);
  },

  /**
   * Same as above, but for a single login.
   */
  loginToVanillaObject(login) {
    let obj = {};
    for (let i in login.QueryInterface(Ci.nsILoginMetaInfo)) {
      if (typeof login[i] !== 'function') {
        obj[i] = login[i];
      }
    }

    return obj;
  },

  /**
   * Convert an object received from IPC into an nsILoginInfo (with guid).
   */
  vanillaObjectToLogin(login) {
    let formLogin = Cc["@mozilla.org/login-manager/loginInfo;1"].
                    createInstance(Ci.nsILoginInfo);
    formLogin.init(login.hostname, login.formSubmitURL,
                   login.httpRealm, login.username,
                   login.password, login.usernameField,
                   login.passwordField);

    formLogin.QueryInterface(Ci.nsILoginMetaInfo);
    for (let prop of ["guid", "timeCreated", "timeLastUsed", "timePasswordChanged", "timesUsed"]) {
      formLogin[prop] = login[prop];
    }
    return formLogin;
  },

  /**
   * As above, but for an array of objects.
   */
  vanillaObjectsToLogins(logins) {
    return logins.map(this.vanillaObjectToLogin);
  },

  removeLegacySignonFiles() {
    const {Constants, Path, File} = Cu.import("resource://gre/modules/osfile.jsm").OS;

    const profileDir = Constants.Path.profileDir;
    const defaultSignonFilePrefs = new Map([
      ["signon.SignonFileName", "signons.txt"],
      ["signon.SignonFileName2", "signons2.txt"],
      ["signon.SignonFileName3", "signons3.txt"]
    ]);
    const toDeletes = new Set();

    for (let [pref, val] of defaultSignonFilePrefs.entries()) {
      toDeletes.add(Path.join(profileDir, val));

      try {
        let signonFile = Services.prefs.getCharPref(pref);

        toDeletes.add(Path.join(profileDir, signonFile));
        Services.prefs.clearUserPref(pref);
      } catch (e) {}
    }

    for (let file of toDeletes) {
      File.remove(file);
    }
  },

  /**
   * Returns true if the user has a master password set and false otherwise.
   */
  isMasterPasswordSet() {
    let secmodDB = Cc["@mozilla.org/security/pkcs11moduledb;1"].
                   getService(Ci.nsIPKCS11ModuleDB);
    let slot = secmodDB.findSlotByName("");
    if (!slot) {
      return false;
    }
    let hasMP = slot.status != Ci.nsIPKCS11Slot.SLOT_UNINITIALIZED &&
                slot.status != Ci.nsIPKCS11Slot.SLOT_READY;
    return hasMP;
  }
};

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let logger = LoginHelper.createLogger("LoginHelper");
  return logger;
});
