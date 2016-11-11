/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provides an object that has a method to import login-related data from the
 * previous SQLite storage format.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "LoginImport",
];

// Globals

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Sqlite",
                                  "resource://gre/modules/Sqlite.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

// LoginImport

/**
 * Provides an object that has a method to import login-related data from the
 * previous SQLite storage format.
 *
 * @param aStore
 *        LoginStore object where imported data will be added.
 * @param aPath
 *        String containing the file path of the SQLite login database.
 */
this.LoginImport = function(aStore, aPath) {
  this.store = aStore;
  this.path = aPath;
};

this.LoginImport.prototype = {
  /**
   * LoginStore object where imported data will be added.
   */
  store: null,

  /**
   * String containing the file path of the SQLite login database.
   */
  path: null,

  /**
   * Imports login-related data from the previous SQLite storage format.
   */
  import: Task.async(function* () {
    // We currently migrate data directly from the database to the JSON store at
    // first run, then we set a preference to prevent repeating the import.
    // Thus, merging with existing data is not a use case we support.  This
    // restriction might be removed to support re-importing passwords set by an
    // old version by flipping the import preference and restarting.
    if (this.store.data.logins.length > 0 ||
        this.store.data.disabledHosts.length > 0) {
      throw new Error("Unable to import saved passwords because some data " +
                      "has already been imported or saved.");
    }

    // When a timestamp is not specified, we will use the same reference time.
    let referenceTimeMs = Date.now();

    let connection = yield Sqlite.openConnection({ path: this.path });
    try {
      let schemaVersion = yield connection.getSchemaVersion();

      // We support importing database schema versions from 3 onwards.
      // Version 3 was implemented in bug 316084 (Firefox 3.6, March 2009).
      // Version 4 was implemented in bug 465636 (Firefox 4, March 2010).
      // Version 5 was implemented in bug 718817 (Firefox 13, February 2012).
      if (schemaVersion < 3) {
        throw new Error("Unable to import saved passwords because " +
                        "the existing profile is too old.");
      }

      let rows = yield connection.execute("SELECT * FROM moz_logins");
      for (let row of rows) {
        try {
          let hostname = row.getResultByName("hostname");
          let httpRealm = row.getResultByName("httpRealm");
          let formSubmitURL = row.getResultByName("formSubmitURL");
          let usernameField = row.getResultByName("usernameField");
          let passwordField = row.getResultByName("passwordField");
          let encryptedUsername = row.getResultByName("encryptedUsername");
          let encryptedPassword = row.getResultByName("encryptedPassword");

          // The "guid" field was introduced in schema version 2, and the
          // "enctype" field was introduced in schema version 3.  We don't
          // support upgrading from older versions of the database.
          let guid = row.getResultByName("guid");
          let encType = row.getResultByName("encType");

          // The time and count fields were introduced in schema version 4.
          let timeCreated = null;
          let timeLastUsed = null;
          let timePasswordChanged = null;
          let timesUsed = null;
          try {
            timeCreated = row.getResultByName("timeCreated");
            timeLastUsed = row.getResultByName("timeLastUsed");
            timePasswordChanged = row.getResultByName("timePasswordChanged");
            timesUsed = row.getResultByName("timesUsed");
          } catch (ex) { }

          // These columns may be null either because they were not present in
          // the database or because the record was created on a new schema
          // version by an old application version.
          if (!timeCreated) {
            timeCreated = referenceTimeMs;
          }
          if (!timeLastUsed) {
            timeLastUsed = referenceTimeMs;
          }
          if (!timePasswordChanged) {
            timePasswordChanged = referenceTimeMs;
          }
          if (!timesUsed) {
            timesUsed = 1;
          }

          this.store.data.logins.push({
            id: this.store.data.nextId++,
            hostname: hostname,
            httpRealm: httpRealm,
            formSubmitURL: formSubmitURL,
            usernameField: usernameField,
            passwordField: passwordField,
            encryptedUsername: encryptedUsername,
            encryptedPassword: encryptedPassword,
            guid: guid,
            encType: encType,
            timeCreated: timeCreated,
            timeLastUsed: timeLastUsed,
            timePasswordChanged: timePasswordChanged,
            timesUsed: timesUsed,
          });
        } catch (ex) {
          Cu.reportError("Error importing login: " + ex);
        }
      }

      rows = yield connection.execute("SELECT * FROM moz_disabledHosts");
      for (let row of rows) {
        try {
          let hostname = row.getResultByName("hostname");

          this.store.data.disabledHosts.push(hostname);
        } catch (ex) {
          Cu.reportError("Error importing disabled host: " + ex);
        }
      }
    } finally {
      yield connection.close();
    }
  }),
};
