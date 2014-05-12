/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Provides infrastructure for automated login components tests.
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Globals

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/commonjs/sdk/core/promise.js");

const LoginInfo =
      Components.Constructor("@mozilla.org/login-manager/loginInfo;1",
                             "nsILoginInfo", "init");

/**
 * All the tests are implemented with add_task, this starts them automatically.
 */
function run_test()
{
  do_get_profile();
  run_next_test();
}

////////////////////////////////////////////////////////////////////////////////
//// Global helpers

// Some of these functions are already implemented in other parts of the source
// tree, see bug 946708 about sharing more code.

/**
 * Allows waiting for an observer notification once.
 *
 * @param aTopic
 *        Notification topic to observe.
 *
 * @return {Promise}
 * @resolves The array [aSubject, aData] from the observed notification.
 * @rejects Never.
 */
function promiseTopicObserved(aTopic)
{
  let deferred = Promise.defer();

  Services.obs.addObserver(
    function PTO_observe(aSubject, aTopic, aData) {
      Services.obs.removeObserver(PTO_observe, aTopic);
      deferred.resolve([aSubject, aData]);
    }, aTopic, false);

  return deferred.promise;
}

/**
 * Returns a new XPCOM property bag with the provided properties.
 *
 * @param aProperties
 *        Each property of this object is copied to the property bag.  This
 *        parameter can be omitted to return an empty property bag.
 *
 * @return A new property bag, that is an instance of nsIWritablePropertyBag,
 *         nsIWritablePropertyBag2, nsIPropertyBag, and nsIPropertyBag2.
 */
function newPropertyBag(aProperties)
{
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
}

////////////////////////////////////////////////////////////////////////////////
//// Local helpers

const LoginTest = {
  /**
   * Forces the storage module to save all data, and the Login Manager service
   * to replace the storage module with a newly initialized instance.
   */
  reloadData: function ()
  {
    Services.obs.notifyObservers(null, "passwordmgr-storage-replace", null);
    yield promiseTopicObserved("passwordmgr-storage-replace-complete");
  },

  /**
   * Erases all the data stored by the Login Manager service.
   */
  clearData: function ()
  {
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
  checkLogins: function (aExpectedLogins)
  {
    this.assertLoginListsEqual(Services.logins.getAllLogins(), aExpectedLogins);
  },

  /**
   * Checks that the two provided arrays of nsILoginInfo have the same length,
   * and every login in aExpectedLogins is also found in aActualLogins.  The
   * comparison uses the "equals" method of nsILoginInfo, that does not include
   * nsILoginMetaInfo properties in the test.
   */
  assertLoginListsEqual: function (aActual, aExpected)
  {
    do_check_eq(aExpected.length, aActual.length);
    do_check_true(aExpected.every(e => aActual.some(a => a.equals(e))));
  },

  /**
   * Checks that the two provided arrays of strings contain the same values,
   * maybe in a different order, case-sensitively.
   */
  assertDisabledHostsEqual: function (aActual, aExpected)
  {
    Assert.deepEqual(aActual.sort(), aExpected.sort());
  },

  /**
   * Checks whether the given time, expressed as the number of milliseconds
   * since January 1, 1970, 00:00:00 UTC, falls within 30 seconds of now.
   */
  assertTimeIsAboutNow: function (aTimeMs)
  {
    do_check_true(Math.abs(aTimeMs - Date.now()) < 30000);
  }
};

////////////////////////////////////////////////////////////////////////////////
//// Predefined test data

/**
 * This object contains functions that return new instances of nsILoginInfo for
 * every call.  The returned instances can be compared using their "equals" or
 * "matches" methods, or modified for the needs of the specific test being run.
 *
 * Any modification to the test data requires updating the tests accordingly, in
 * particular the search tests.
 */
const TestData = {
  /**
   * Returns a new nsILoginInfo for use with form submits.
   *
   * @param aModifications
   *        Each property of this object replaces the property of the same name
   *        in the returned nsILoginInfo or nsILoginMetaInfo.
   */
  formLogin: function (aModifications)
  {
    let loginInfo = new LoginInfo("http://www3.example.com",
                                  "http://www.example.com", null,
                                  "the username", "the password",
                                  "form_field_username", "form_field_password");
    loginInfo.QueryInterface(Ci.nsILoginMetaInfo);
    if (aModifications) {
      for (let [name, value] of Iterator(aModifications)) {
        loginInfo[name] = value;
      }
    }
    return loginInfo;
  },

  /**
   * Returns a new nsILoginInfo for use with HTTP authentication.
   *
   * @param aModifications
   *        Each property of this object replaces the property of the same name
   *        in the returned nsILoginInfo or nsILoginMetaInfo.
   */
  authLogin: function (aModifications)
  {
    let loginInfo = new LoginInfo("http://www.example.org", null,
                                  "The HTTP Realm", "the username",
                                  "the password", "", "");
    loginInfo.QueryInterface(Ci.nsILoginMetaInfo);
    if (aModifications) {
      for (let [name, value] of Iterator(aModifications)) {
        loginInfo[name] = value;
      }
    }
    return loginInfo;
  },

  /**
   * Returns an array of typical nsILoginInfo that could be stored in the
   * database.
   */
  loginList: function ()
  {
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
      new LoginInfo("http://www3.example.com", "https://www.example.com", null,
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

////////////////////////////////////////////////////////////////////////////////
//// Initialization functions common to all tests

add_task(function test_common_initialize()
{
  // Before initializing the service for the first time, we should copy the key
  // file required to decrypt the logins contained in the SQLite databases used
  // by migration tests.  This file is not required for the other tests.
  yield OS.File.copy(do_get_file("data/key3.db").path,
                     OS.Path.join(OS.Constants.Path.profileDir, "key3.db"));

  // Ensure that the service and the storage module are initialized.
  yield Services.logins.initializationPromise;

  // Ensure that every test file starts with an empty database.
  LoginTest.clearData();

  // Clean up after every test.
  do_register_cleanup(() => LoginTest.clearData());
});
