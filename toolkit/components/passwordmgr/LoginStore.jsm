/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Handles serialization of the data and persistence into a file.
 *
 * The file is stored in JSON format, without indentation, using UTF-8 encoding.
 * With indentation applied, the file would look like this:
 *
 * {
 *   "logins": [
 *     {
 *       "id": 2,
 *       "hostname": "http://www.example.com",
 *       "httpRealm": null,
 *       "formSubmitURL": "http://www.example.com",
 *       "usernameField": "username_field",
 *       "passwordField": "password_field",
 *       "encryptedUsername": "...",
 *       "encryptedPassword": "...",
 *       "guid": "...",
 *       "encType": 1,
 *       "timeCreated": 1262304000000,
 *       "timeLastUsed": 1262304000000,
 *       "timePasswordChanged": 1262476800000,
 *       "timesUsed": 1
 *     },
 *     {
 *       "id": 4,
 *       (...)
 *     }
 *   ],
 *   "nextId": 10,
 *   "version": 1
 * }
 */

"use strict";

const EXPORTED_SYMBOLS = ["LoginStore"];

// Globals

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { JSONFile } = ChromeUtils.import("resource://gre/modules/JSONFile.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

ChromeUtils.defineModuleGetter(lazy, "OS", "resource://gre/modules/osfile.jsm");

XPCOMUtils.defineLazyModuleGetters(lazy, {
  FXA_PWDMGR_HOST: "resource://gre/modules/FxAccountsCommon.js",
  FXA_PWDMGR_REALM: "resource://gre/modules/FxAccountsCommon.js",
});

/**
 * Current data version assigned by the code that last touched the data.
 *
 * This number should be updated only when it is important to understand whether
 * an old version of the code has touched the data, for example to execute an
 * update logic.  In most cases, this number should not be changed, in
 * particular when no special one-time update logic is needed.
 *
 * For example, this number should NOT be changed when a new optional field is
 * added to a login entry.
 */
const kDataVersion = 3;

// The permission type we store in the permission manager.
const PERMISSION_SAVE_LOGINS = "login-saving";

const MAX_DATE_MS = 8640000000000000;

// LoginStore

/**
 * Inherits from JSONFile and handles serialization of login-related data and
 * persistence into a file.
 *
 * @param aPath
 *        String containing the file path where data should be saved.
 */
function LoginStore(aPath, aBackupPath = "") {
  JSONFile.call(this, {
    path: aPath,
    dataPostProcessor: this._dataPostProcessor.bind(this),
    backupTo: aBackupPath,
  });
}

LoginStore.prototype = Object.create(JSONFile.prototype);
LoginStore.prototype.constructor = LoginStore;

LoginStore.prototype._save = async function() {
  await JSONFile.prototype._save.call(this);
  // Notify tests that writes to the login store is complete.
  Services.obs.notifyObservers(null, "password-storage-updated");

  if (this._options.backupTo) {
    await this._backupHandler();
  }
};

/**
 * Delete logins backup file if the last saved login was removed using
 * removeLogin() or if all logins were removed at once using removeAllUserFacingLogins().
 * Note that if the user has a fxa key stored as a login, we just update the
 * backup to only store the key when the last saved user facing login is removed.
 */
LoginStore.prototype._backupHandler = async function() {
  const logins = this._data.logins;
  // Return early if more than one login is stored because the backup won't need
  // updating in this case.
  if (logins.length > 1) {
    return;
  }

  // If one login is stored and it's a fxa sync key, we update the backup to store the
  // key only.
  if (
    logins.length &&
    logins[0].hostname == lazy.FXA_PWDMGR_HOST &&
    logins[0].httpRealm == lazy.FXA_PWDMGR_REALM
  ) {
    try {
      await lazy.OS.File.copy(this.path, this._options.backupTo);

      // This notification is specifically sent out for a test.
      Services.obs.notifyObservers(null, "logins-backup-updated");
    } catch (ex) {
      Cu.reportError(ex);
    }
  } else if (!logins.length) {
    // If no logins are stored anymore, delete backup.
    await lazy.OS.File.remove(this._options.backupTo, {
      ignoreAbsent: true,
    });
  }
};

/**
 * Synchronously work on the data just loaded into memory.
 */
LoginStore.prototype._dataPostProcessor = function(data) {
  if (data.nextId === undefined) {
    data.nextId = 1;
  }

  // Create any arrays that are not present in the saved file.
  if (!data.logins) {
    data.logins = [];
  }

  if (!data.potentiallyVulnerablePasswords) {
    data.potentiallyVulnerablePasswords = [];
  }

  if (!data.dismissedBreachAlertsByLoginGUID) {
    data.dismissedBreachAlertsByLoginGUID = {};
  }

  // sanitize dates in logins
  if (!("version" in data) || data.version < 3) {
    let dateProperties = ["timeCreated", "timeLastUsed", "timePasswordChanged"];
    let now = Date.now();
    function getEarliestDate(login, defaultDate) {
      let earliestDate = dateProperties.reduce((earliest, pname) => {
        let ts = login[pname];
        return !ts ? earliest : Math.min(ts, earliest);
      }, defaultDate);
      return earliestDate;
    }
    for (let login of data.logins) {
      for (let pname of dateProperties) {
        let earliestDate;
        if (!login[pname] || login[pname] > MAX_DATE_MS) {
          login[pname] =
            earliestDate || (earliestDate = getEarliestDate(login, now));
        }
      }
    }
  }

  // Indicate that the current version of the code has touched the file.
  data.version = kDataVersion;

  return data;
};
