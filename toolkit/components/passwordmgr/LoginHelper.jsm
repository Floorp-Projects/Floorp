/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
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

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

////////////////////////////////////////////////////////////////////////////////
//// LoginHelper

/**
 * Contains functions shared by different Login Manager components.
 */
this.LoginHelper = {
  /**
   * Due to the way the signons2.txt file is formatted, we need to make
   * sure certain field values or characters do not cause the file to
   * be parsed incorrectly.  Reject hostnames that we can't store correctly.
   *
   * @throws String with English message in case validation failed.
   */
  checkHostnameValue: function (aHostname)
  {
    // Nulls are invalid, as they don't round-trip well.  Newlines are also
    // invalid for any field stored as plaintext, and a hostname made of a
    // single dot cannot be stored in the legacy format.
    if (aHostname == "." ||
        aHostname.indexOf("\r") != -1 ||
        aHostname.indexOf("\n") != -1 ||
        aHostname.indexOf("\0") != -1) {
      throw "Invalid hostname";
    }
  },

  /**
   * Due to the way the signons2.txt file is formatted, we need to make
   * sure certain field values or characters do not cause the file to
   * be parsed incorrectly.  Reject logins that we can't store correctly.
   *
   * @throws String with English message in case validation failed.
   */
  checkLoginValues: function (aLogin)
  {
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
      throw "login values can't contain nulls";
    }

    // In theory these nulls should just be rolled up into the encrypted
    // values, but nsISecretDecoderRing doesn't use nsStrings, so the
    // nulls cause truncation. Check for them here just to avoid
    // unexpected round-trip surprises.
    if (aLogin.username.indexOf("\0") != -1 ||
        aLogin.password.indexOf("\0") != -1) {
      throw "login values can't contain nulls";
    }

    // Newlines are invalid for any field stored as plaintext.
    if (badCharacterPresent(aLogin, "\r") ||
        badCharacterPresent(aLogin, "\n")) {
      throw "login values can't contain newlines";
    }

    // A line with just a "." can have special meaning.
    if (aLogin.usernameField == "." ||
        aLogin.formSubmitURL == ".") {
      throw "login values can't be periods";
    }

    // A hostname with "\ \(" won't roundtrip.
    // eg host="foo (", realm="bar" --> "foo ( (bar)"
    // vs host="foo", realm=" (bar" --> "foo ( (bar)"
    if (aLogin.hostname.indexOf(" (") != -1) {
      throw "bad parens in hostname";
    }
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
  buildModifiedLogin: function (aOldStoredLogin, aNewLoginData)
  {
    function bagHasProperty(aPropName)
    {
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
            throw "Unexpected propertybag item: " + prop.name;
        }
      }
    } else {
      throw "newLoginData needs an expected interface!";
    }

    // Sanity check the login
    if (newLogin.hostname == null || newLogin.hostname.length == 0) {
      throw "Can't add a login with a null or empty hostname.";
    }

    // For logins w/o a username, set to "", not null.
    if (newLogin.username == null) {
      throw "Can't add a login with a null username.";
    }

    if (newLogin.password == null || newLogin.password.length == 0) {
      throw "Can't add a login with a null or empty password.";
    }

    if (newLogin.formSubmitURL || newLogin.formSubmitURL == "") {
      // We have a form submit URL. Can't have a HTTP realm.
      if (newLogin.httpRealm != null) {
        throw "Can't add a login with both a httpRealm and formSubmitURL.";
      }
    } else if (newLogin.httpRealm) {
      // We have a HTTP realm. Can't have a form submit URL.
      if (newLogin.formSubmitURL != null) {
        throw "Can't add a login with both a httpRealm and formSubmitURL.";
      }
    } else {
      // Need one or the other!
      throw "Can't add a login without a httpRealm or formSubmitURL.";
    }

    // Throws if there are bogus values.
    this.checkLoginValues(newLogin);

    return newLogin;
  },
};
