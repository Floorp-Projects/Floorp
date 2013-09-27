/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


this.EXPORTED_SYMBOLS = ["fxAccounts", "FxAccounts"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/osfile.jsm")
Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/Services.jsm");

const defaultBaseDir = OS.Path.join(OS.Constants.Path.profileDir);
const defaultStorageOptions = {
  filename: 'signedInUser.json',
  baseDir: defaultBaseDir,
};

/**
 * FxAccounts constructor
 *
 * @param signedInUserStorage is a storage instance for getting/setting
 *                            the signedInUser. Uses JSONStorage by default.
 * @return instance
 */
function FxAccounts(signedInUserStorage = new JSONStorage(defaultStorageOptions)) {
  this._signedInUserStorage = signedInUserStorage;
}

FxAccounts.prototype = Object.freeze({
  // data format version
  version: 1,

  /**
   * Set the current user signed in to Firefox Accounts (FxA)
   *
   * @param credentials
   *        The credentials object obtained by logging in or creating
   *        an account on the FxA server:
   *
   *        {
   *          email: The users email address
   *          uid: The user's unique id
   *          sessionToken: Session for the FxA server
   *          assertion: A Persona assertion used to enable Sync
   *          kA: An encryption key from the FxA server
   *          kB: An encryption key derived from the user's FxA password
   *        }
   *
   * @return Promise
   *         The promise resolves to null on success or is rejected on error
   */
  setSignedInUser: function setSignedInUser(credentials) {
    let record = { version: this.version, accountData: credentials };
    // cache a clone of the credentials object
    this._signedInUser = JSON.parse(JSON.stringify(record));

    return this._signedInUserStorage.set(record);
  },

  /**
   * Get the user currently signed in to Firefox Accounts (FxA)
   *
   * @return Promise
   *        The promise resolves to the credentials object of the signed-in user:
   *
   *        {
   *          email: The user's email address
   *          uid: The user's unique id
   *          sessionToken: Session for the FxA server
   *          assertion: A Persona assertion used to enable Sync
   *          kA: An encryption key from the FxA server
   *          kB: An encryption key derived from the user's FxA password
   *        }
   *
   *        or null if no user is signed in or the user data is an
   *        unrecognized version.
   *
   */
  getSignedInUser: function getSignedInUser() {
    // skip disk if user is cached
    if (typeof this._signedInUser !== 'undefined') {
      let deferred = Promise.defer();
      let result = this._signedInUser ? this._signedInUser.accountData : undefined;
      deferred.resolve(result);
      return deferred.promise;
    }

    return this._signedInUserStorage.get()
      .then((user) => {
          if (user.version === this.version) {
            this._signedInUser = user;
            return user.accountData;
          }
        },
        (err) => undefined);
  },

  /**
   * Sign the current user out
   *
   * @return Promise
   *         The promise is rejected if a storage error occurs
   */
  signOut: function signOut() {
    this._signedInUser = {};
    return this._signedInUserStorage.set(null);
  },

  getAccountsURI: function () {
    let url = Services.urlFormatter.formatURLPref("firefox.accounts.remoteUrl");
    if (!/^https:/.test(url)) {
      throw new Error("Firefox Accounts server must use HTTPS");
    }
    return url;
  },

});



/**
 * JSONStorage constructor that creates instances that may set/get
 * to a specified file, in a directory that will be created if it
 * doesn't exist.
 *
 * @param options {
 *                  filename: of the file to write to
 *                  baseDir: directory where the file resides
 *                }
 * @return instance
 */
function JSONStorage(options) {
  this.baseDir = options.baseDir;
  this.path = OS.Path.join(options.baseDir, options.filename);
}

JSONStorage.prototype = Object.freeze({
  set: function (contents) {
    return OS.File.makeDir(this.baseDir, {ignoreExisting: true})
      .then(CommonUtils.writeJSON.bind(null, contents, this.path));
  },

  get: function () {
    return CommonUtils.readJSON(this.path);
  },
});


// create an instance to export
this.fxAccounts = new FxAccounts();

