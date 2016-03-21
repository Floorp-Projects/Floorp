/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Shared functions generally available for testing login components.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "LoginTestUtils",
];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("resource://testing-common/TestUtils.jsm");

const LoginInfo =
      Components.Constructor("@mozilla.org/login-manager/loginInfo;1",
                             "nsILoginInfo", "init");

// For now, we need consumers to provide a reference to Assert.jsm.
var Assert = null;

this.LoginTestUtils = {
  set Assert(assert) {
    Assert = assert; // eslint-disable-line no-native-reassign
  },

  /**
   * Forces the storage module to save all data, and the Login Manager service
   * to replace the storage module with a newly initialized instance.
   */
  * reloadData() {
    Services.obs.notifyObservers(null, "passwordmgr-storage-replace", null);
    yield TestUtils.topicObserved("passwordmgr-storage-replace-complete");
  },

  /**
   * Erases all the data stored by the Login Manager service.
   */
  clearData() {
    Services.logins.removeAllLogins();
    for (let hostname of Services.logins.getAllDisabledHosts()) {
      Services.logins.setLoginSavingEnabled(hostname, true);
    }
  },

  /**
   * Checks that the currently stored list of nsILoginInfo matches the provided
   * array.  The comparison uses the "equals" method of nsILoginInfo, that does
   * not include nsILoginMetaInfo properties in the test.
   */
  checkLogins(expectedLogins) {
    this.assertLoginListsEqual(Services.logins.getAllLogins(), expectedLogins);
  },

  /**
   * Checks that the two provided arrays of nsILoginInfo have the same length,
   * and every login in "expected" is also found in "actual".  The comparison
   * uses the "equals" method of nsILoginInfo, that does not include
   * nsILoginMetaInfo properties in the test.
   */
  assertLoginListsEqual(actual, expected) {
    Assert.equal(expected.length, actual.length);
    Assert.ok(expected.every(e => actual.some(a => a.equals(e))));
  },

  /**
   * Checks that every login in "expected" matches one in "actual".
   * The comparison uses the "matches" method of nsILoginInfo.
   */
  assertLoginListsMatches(actual, expected, ignorePassword) {
    Assert.equal(expected.length, actual.length);
    Assert.ok(expected.every(e => actual.some(a => a.matches(e, ignorePassword))));
  },

  /**
   * Checks that the two provided arrays of strings contain the same values,
   * maybe in a different order, case-sensitively.
   */
  assertDisabledHostsEqual(actual, expected) {
    Assert.deepEqual(actual.sort(), expected.sort());
  },

  /**
   * Checks whether the given time, expressed as the number of milliseconds
   * since January 1, 1970, 00:00:00 UTC, falls within 30 seconds of now.
   */
  assertTimeIsAboutNow(timeMs) {
    Assert.ok(Math.abs(timeMs - Date.now()) < 30000);
  },
};

/**
 * This object contains functions that return new instances of nsILoginInfo for
 * every call.  The returned instances can be compared using their "equals" or
 * "matches" methods, or modified for the needs of the specific test being run.
 *
 * Any modification to the test data requires updating the tests accordingly, in
 * particular the search tests.
 */
this.LoginTestUtils.testData = {
  /**
   * Returns a new nsILoginInfo for use with form submits.
   *
   * @param modifications
   *        Each property of this object replaces the property of the same name
   *        in the returned nsILoginInfo or nsILoginMetaInfo.
   */
  formLogin(modifications) {
    let loginInfo = new LoginInfo("http://www3.example.com",
                                  "http://www.example.com", null,
                                  "the username", "the password",
                                  "form_field_username", "form_field_password");
    loginInfo.QueryInterface(Ci.nsILoginMetaInfo);
    if (modifications) {
      for (let [name, value] of Iterator(modifications)) {
        loginInfo[name] = value;
      }
    }
    return loginInfo;
  },

  /**
   * Returns a new nsILoginInfo for use with HTTP authentication.
   *
   * @param modifications
   *        Each property of this object replaces the property of the same name
   *        in the returned nsILoginInfo or nsILoginMetaInfo.
   */
  authLogin(modifications) {
    let loginInfo = new LoginInfo("http://www.example.org", null,
                                  "The HTTP Realm", "the username",
                                  "the password", "", "");
    loginInfo.QueryInterface(Ci.nsILoginMetaInfo);
    if (modifications) {
      for (let [name, value] of Iterator(modifications)) {
        loginInfo[name] = value;
      }
    }
    return loginInfo;
  },

  /**
   * Returns an array of typical nsILoginInfo that could be stored in the
   * database.
   */
  loginList() {
    return [
      // --- Examples of form logins (subdomains of example.com) ---

      // Simple form login with named fields for username and password.
      new LoginInfo("http://www.example.com", "http://www.example.com", null,
                    "the username", "the password for www.example.com",
                    "form_field_username", "form_field_password"),

      // Different schemes are treated as completely different sites.
      new LoginInfo("https://www.example.com", "https://www.example.com", null,
                    "the username", "the password for https",
                    "form_field_username", "form_field_password"),

      // Subdomains are treated as completely different sites.
      new LoginInfo("https://example.com", "https://example.com", null,
                    "the username", "the password for example.com",
                    "form_field_username", "form_field_password"),

      // Forms found on the same host, but with different hostnames in the
      // "action" attribute, are handled independently.
      new LoginInfo("http://www3.example.com", "http://www.example.com", null,
                    "the username", "the password",
                    "form_field_username", "form_field_password"),
      new LoginInfo("http://www3.example.com", "http://example.com", null,
                    "the username", "the password",
                    "form_field_username", "form_field_password"),

      // It is not possible to store multiple passwords for the same username,
      // however multiple passwords can be stored when the usernames differ.
      // An empty username is a valid case and different from the others.
      new LoginInfo("http://www4.example.com", "http://www4.example.com", null,
                    "username one", "password one",
                    "form_field_username", "form_field_password"),
      new LoginInfo("http://www4.example.com", "http://www4.example.com", null,
                    "username two", "password two",
                    "form_field_username", "form_field_password"),
      new LoginInfo("http://www4.example.com", "http://www4.example.com", null,
                    "", "password three",
                    "form_field_username", "form_field_password"),

      // Username and passwords fields in forms may have no "name" attribute.
      new LoginInfo("http://www5.example.com", "http://www5.example.com", null,
                    "multi username", "multi password", "", ""),

      // Forms with PIN-type authentication will typically have no username.
      new LoginInfo("http://www6.example.com", "http://www6.example.com", null,
                    "", "12345", "", "form_field_password"),

      // --- Examples of authentication logins (subdomains of example.org) ---

      // Simple HTTP authentication login.
      new LoginInfo("http://www.example.org", null, "The HTTP Realm",
                    "the username", "the password", "", ""),

      // Simple FTP authentication login.
      new LoginInfo("ftp://ftp.example.org", null, "ftp://ftp.example.org",
                    "the username", "the password", "", ""),

      // Multiple HTTP authentication logins can be stored for different realms.
      new LoginInfo("http://www2.example.org", null, "The HTTP Realm",
                    "the username", "the password", "", ""),
      new LoginInfo("http://www2.example.org", null, "The HTTP Realm Other",
                    "the username other", "the password other", "", ""),

      // --- Both form and authentication logins (example.net) ---

      new LoginInfo("http://example.net", "http://example.net", null,
                    "the username", "the password",
                    "form_field_username", "form_field_password"),
      new LoginInfo("http://example.net", "http://www.example.net", null,
                    "the username", "the password",
                    "form_field_username", "form_field_password"),
      new LoginInfo("http://example.net", "http://www.example.net", null,
                    "username two", "the password",
                    "form_field_username", "form_field_password"),
      new LoginInfo("http://example.net", null, "The HTTP Realm",
                    "the username", "the password", "", ""),
      new LoginInfo("http://example.net", null, "The HTTP Realm Other",
                    "username two", "the password", "", ""),
      new LoginInfo("ftp://example.net", null, "ftp://example.net",
                    "the username", "the password", "", ""),

      // --- Examples of logins added by extensions (chrome scheme) ---

      new LoginInfo("chrome://example_extension", null, "Example Login One",
                    "the username", "the password one", "", ""),
      new LoginInfo("chrome://example_extension", null, "Example Login Two",
                    "the username", "the password two", "", ""),
    ];
  },
};

this.LoginTestUtils.recipes = {
  getRecipeParent() {
    let { LoginManagerParent } = Cu.import("resource://gre/modules/LoginManagerParent.jsm", {});
    if (!LoginManagerParent.recipeParentPromise) {
      return null;
    }
    return LoginManagerParent.recipeParentPromise.then((recipeParent) => {
      return recipeParent;
    });
  },
};
