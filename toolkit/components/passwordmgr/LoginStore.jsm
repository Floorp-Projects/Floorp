/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Handles serialization of login-related data and persistence into a file.
 *
 * This modules handles the raw data stored in JavaScript serializable objects,
 * and contains no special validation or query logic, that is handled entirely
 * by "storage.js" instead.
 *
 * The data can be manipulated only after it has been loaded from disk.  The
 * load process can happen asynchronously, through the "load" method, or
 * synchronously, through "ensureDataReady".  After any modification, the
 * "saveSoon" method must be called to flush the data to disk asynchronously.
 *
 * The raw data should be manipulated synchronously, without waiting for the
 * event loop or for promise resolution, so that the saved file is always
 * consistent.  This synchronous approach also simplifies the query and update
 * logic.  For example, it is possible to find an object and modify it
 * immediately without caring whether other code modifies it in the meantime.
 *
 * An asynchronous shutdown observer makes sure that data is always saved before
 * the browser is closed.  The data cannot be modified during shutdown.
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
 *       "formSubmitURL": "http://www.example.com/submit-url",
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
 *   "disabledHosts": [
 *     "http://www.example.org",
 *     "http://www.example.net"
 *   ],
 *   "nextId": 10,
 *   "version": 1
 * }
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "LoginStore",
];

////////////////////////////////////////////////////////////////////////////////
//// Globals

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AsyncShutdown",
                                  "resource://gre/modules/AsyncShutdown.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DeferredTask",
                                  "resource://gre/modules/DeferredTask.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm")

XPCOMUtils.defineLazyGetter(this, "gTextDecoder", function () {
  return new TextDecoder();
});

XPCOMUtils.defineLazyGetter(this, "gTextEncoder", function () {
  return new TextEncoder();
});

const FileInputStream =
      Components.Constructor("@mozilla.org/network/file-input-stream;1",
                             "nsIFileInputStream", "init");

/**
 * Delay between a change to the login data and the related save operation.
 */
const kSaveDelayMs = 1500;

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
const kDataVersion = 1;

////////////////////////////////////////////////////////////////////////////////
//// LoginStore

/**
 * Handles serialization of login-related data and persistence into a file.
 *
 * @param aPath
 *        String containing the file path where data should be saved.
 */
function LoginStore(aPath)
{
  this.path = aPath;

  this._saver = new DeferredTask(() => this.save(), kSaveDelayMs);
  AsyncShutdown.profileBeforeChange.addBlocker("Login store: writing data",
                                               () => this._saver.finalize());
}

LoginStore.prototype = {
  /**
   * String containing the file path where data should be saved.
   */
  path: "",

  /**
   * Serializable object containing the login-related data.  This is populated
   * directly with the data loaded from the file, and is saved without
   * modifications.
   *
   * This contains one property for each list.
   */
  data: null,

  /**
   * True when data has been loaded.
   */
  dataReady: false,

  /**
   * Loads persistent data from the file to memory.
   *
   * @return {Promise}
   * @resolves When the operation finished successfully.
   * @rejects JavaScript exception.
   */
  load: function ()
  {
    return Task.spawn(function () {
      try {
        let bytes = yield OS.File.read(this.path);

        // If synchronous loading happened in the meantime, exit now.
        if (this.dataReady) {
          return;
        }

        this.data = JSON.parse(gTextDecoder.decode(bytes));
      } catch (ex) {
        // If an exception occurred because the file did not exist, we should
        // just start with new data.  Other errors may indicate that the file is
        // corrupt, thus we move it to a backup location before allowing it to
        // be overwritten by an empty file.
        if (!(ex instanceof OS.File.Error && ex.becauseNoSuchFile)) {
          Cu.reportError(ex);

          // Move the original file to a backup location, ignoring errors.
          try {
            let openInfo = yield OS.File.openUnique(this.path + ".corrupt",
                                                    { humanReadable: true });
            yield openInfo.file.close();
            yield OS.File.move(this.path, openInfo.path);
          } catch (e2) {
            Cu.reportError(e2);
          }
        }

        // In some rare cases it's possible for logins to have been added to
        // our database between the call to OS.File.read and when we've been
        // notified that there was a problem with it. In that case, leave the
        // synchronously-added data alone. See bug 1029128, comment 4.
        if (this.dataReady) {
          return;
        }

        // In any case, initialize a new object to host the data.
        this.data = {
          nextId: 1,
        };
      }

      this._processLoadedData();
    }.bind(this));
  },

  /**
   * Loads persistent data from the file to memory, synchronously.
   */
  ensureDataReady: function ()
  {
    if (this.dataReady) {
      return;
    }

    try {
      // This reads the file and automatically detects the UTF-8 encoding.
      let inputStream = new FileInputStream(new FileUtils.File(this.path),
                                            FileUtils.MODE_RDONLY,
                                            FileUtils.PERMS_FILE, 0)
      try {
        let json = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
        this.data = json.decodeFromStream(inputStream,
                                          inputStream.available());
      } finally {
        inputStream.close();
      }
    } catch (ex) {
      // If an exception occurred because the file did not exist, we should just
      // start with new data.  Other errors may indicate that the file is
      // corrupt, thus we move it to a backup location before allowing it to be
      // overwritten by an empty file.
      if (!(ex instanceof Components.Exception &&
            ex.result == Cr.NS_ERROR_FILE_NOT_FOUND)) {
        Cu.reportError(ex);
        // Move the original file to a backup location, ignoring errors.
        try {
          let originalFile = new FileUtils.File(this.path);
          let backupFile = originalFile.clone();
          backupFile.leafName += ".corrupt";
          backupFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE,
                                  FileUtils.PERMS_FILE);
          backupFile.remove(false);
          originalFile.moveTo(backupFile.parent, backupFile.leafName);
        } catch (e2) {
          Cu.reportError(e2);
        }
      }

      // In any case, initialize a new object to host the data.
      this.data = {
        nextId: 1,
      };
    }

    this._processLoadedData();
  },

  /**
   * Synchronously work on the data just loaded into memory.
   */
  _processLoadedData: function ()
  {
    // Create any arrays that are not present in the saved file.
    if (!this.data.logins) {
      this.data.logins = [];
    }
    if (!this.data.disabledHosts) {
      this.data.disabledHosts = [];
    }

    // Indicate that the current version of the code has touched the file.
    this.data.version = kDataVersion;

    this.dataReady = true;
  },

  /**
   * Called when the data changed, this triggers asynchronous serialization.
   */
  saveSoon: function ()
  {
    return this._saver.arm();
  },

  /**
   * DeferredTask that handles the save operation.
   */
  _saver: null,

  /**
   * Saves persistent data from memory to the file.
   *
   * If an error occurs, the previous file is not deleted.
   *
   * @return {Promise}
   * @resolves When the operation finished successfully.
   * @rejects JavaScript exception.
   */
  save: function ()
  {
    return Task.spawn(function () {
      // Create or overwrite the file.
      let bytes = gTextEncoder.encode(JSON.stringify(this.data));
      yield OS.File.writeAtomic(this.path, bytes,
                                { tmpPath: this.path + ".tmp" });
    }.bind(this));
  },
};
