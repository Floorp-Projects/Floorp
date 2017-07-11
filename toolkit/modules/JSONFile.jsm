/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Handles serialization of the data and persistence into a file.
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
 * the browser is closed. The data cannot be modified during shutdown.
 *
 * The file is stored in JSON format, without indentation, using UTF-8 encoding.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "JSONFile",
];

// Globals

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AsyncShutdown",
                                  "resource://gre/modules/AsyncShutdown.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DeferredTask",
                                  "resource://gre/modules/DeferredTask.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

XPCOMUtils.defineLazyGetter(this, "gTextDecoder", function() {
  return new TextDecoder();
});

XPCOMUtils.defineLazyGetter(this, "gTextEncoder", function() {
  return new TextEncoder();
});

const FileInputStream =
      Components.Constructor("@mozilla.org/network/file-input-stream;1",
                             "nsIFileInputStream", "init");

/**
 * Delay between a change to the data and the related save operation.
 */
const kSaveDelayMs = 1500;

// JSONFile

/**
 * Handles serialization of the data and persistence into a file.
 *
 * @param config An object containing following members:
 *        - path: String containing the file path where data should be saved.
 *        - dataPostProcessor: Function triggered when data is just loaded. The
 *                             data object will be passed as the first argument
 *                             and should be returned no matter it's modified or
 *                             not. Its failure leads to the failure of load()
 *                             and ensureDataReady().
 *        - saveDelayMs: Number indicating the delay (in milliseconds) between a
 *                       change to the data and the related save operation. The
 *                       default value will be applied if omitted.
 *        - beforeSave: Promise-returning function triggered just before the
 *                      data is written to disk. This can be used to create any
 *                      intermediate directories before saving. The file will
 *                      not be saved if the promise rejects or the function
 *                      throws an exception.
 *        - finalizeAt: An `AsyncShutdown` phase or barrier client that should
 *                      automatically finalize the file when triggered. Defaults
 *                      to `profileBeforeChange`; exposed as an option for
 *                      testing.
 *        - compression: A compression algorithm to use when reading and
 *                       writing the data.
 */
function JSONFile(config) {
  this.path = config.path;

  if (typeof config.dataPostProcessor === "function") {
    this._dataPostProcessor = config.dataPostProcessor;
  }
  if (typeof config.beforeSave === "function") {
    this._beforeSave = config.beforeSave;
  }

  if (config.saveDelayMs === undefined) {
    config.saveDelayMs = kSaveDelayMs;
  }
  this._saver = new DeferredTask(() => this._save(), config.saveDelayMs);

  this._options = {};
  if (config.compression) {
    this._options.compression = config.compression;
  }

  this._finalizeAt = config.finalizeAt || AsyncShutdown.profileBeforeChange;
  this._finalizeInternalBound = this._finalizeInternal.bind(this);
  this._finalizeAt.addBlocker("JSON store: writing data",
                              this._finalizeInternalBound);
}

JSONFile.prototype = {
  /**
   * String containing the file path where data should be saved.
   */
  path: "",

  /**
   * True when data has been loaded.
   */
  dataReady: false,

  /**
   * DeferredTask that handles the save operation.
   */
  _saver: null,

  /**
   * Internal data object.
   */
  _data: null,

  /**
   * Internal fields used during finalization.
   */
  _finalizeAt: null,
  _finalizePromise: null,
  _finalizeInternalBound: null,

  /**
   * Serializable object containing the data. This is populated directly with
   * the data loaded from the file, and is saved without modifications.
   *
   * The raw data should be manipulated synchronously, without waiting for the
   * event loop or for promise resolution, so that the saved file is always
   * consistent.
   */
  get data() {
    if (!this.dataReady) {
      throw new Error("Data is not ready.");
    }
    return this._data;
  },

  /**
   * Sets the loaded data to a new object. This will overwrite any persisted
   * data on the next save.
   */
  set data(data) {
    this._data = data;
    this.dataReady = true;
  },

  /**
   * Loads persistent data from the file to memory.
   *
   * @return {Promise}
   * @resolves When the operation finished successfully.
   * @rejects JavaScript exception when dataPostProcessor fails. It never fails
   *          if there is no dataPostProcessor.
   */
  async load() {
    let data = {};

    try {
      let bytes = await OS.File.read(this.path, this._options);

      // If synchronous loading happened in the meantime, exit now.
      if (this.dataReady) {
        return;
      }

      data = JSON.parse(gTextDecoder.decode(bytes));
    } catch (ex) {
      // If an exception occurred because the file did not exist, we should
      // just start with new data.  Other errors may indicate that the file is
      // corrupt, thus we move it to a backup location before allowing it to
      // be overwritten by an empty file.
      if (!(ex instanceof OS.File.Error && ex.becauseNoSuchFile)) {
        Cu.reportError(ex);

        // Move the original file to a backup location, ignoring errors.
        try {
          let openInfo = await OS.File.openUnique(this.path + ".corrupt",
                                                  { humanReadable: true });
          await openInfo.file.close();
          await OS.File.move(this.path, openInfo.path);
        } catch (e2) {
          Cu.reportError(e2);
        }
      }

      // In some rare cases it's possible for data to have been added to
      // our database between the call to OS.File.read and when we've been
      // notified that there was a problem with it. In that case, leave the
      // synchronously-added data alone.
      if (this.dataReady) {
        return;
      }
    }

    this._processLoadedData(data);
  },

  /**
   * Loads persistent data from the file to memory, synchronously. An exception
   * can be thrown only if dataPostProcessor exists and fails.
   */
  ensureDataReady() {
    if (this.dataReady) {
      return;
    }

    let data = {};

    try {
      // This reads the file and automatically detects the UTF-8 encoding.
      let inputStream = new FileInputStream(new FileUtils.File(this.path),
                                            FileUtils.MODE_RDONLY,
                                            FileUtils.PERMS_FILE, 0);
      try {
        let json = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
        data = json.decodeFromStream(inputStream, inputStream.available());
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
    }

    this._processLoadedData(data);
  },

  /**
   * Called when the data changed, this triggers asynchronous serialization.
   */
  saveSoon() {
    return this._saver.arm();
  },

  /**
   * Saves persistent data from memory to the file.
   *
   * If an error occurs, the previous file is not deleted.
   *
   * @return {Promise}
   * @resolves When the operation finished successfully.
   * @rejects JavaScript exception.
   */
  async _save() {
    let json;
    try {
      json = JSON.stringify(this._data);
    } catch (e) {
      // If serialization fails, try fallback safe JSON converter.
      if (typeof this._data.toJSONSafe == "function") {
        json = JSON.stringify(this._data.toJSONSafe());
      } else {
        throw e;
      }
    }

    // Create or overwrite the file.
    let bytes = gTextEncoder.encode(json);
    if (this._beforeSave) {
      await Promise.resolve(this._beforeSave());
    }
    await OS.File.writeAtomic(this.path, bytes,
                              Object.assign(
                                { tmpPath: this.path + ".tmp" },
                                this._options));
  },

  /**
   * Synchronously work on the data just loaded into memory.
   */
  _processLoadedData(data) {
    if (this._finalizePromise) {
      // It's possible for `load` to race with `finalize`. In that case, don't
      // process or set the loaded data.
      return;
    }
    this.data = this._dataPostProcessor ? this._dataPostProcessor(data) : data;
  },

  /**
   * Finishes persisting data to disk and resets all state for this file.
   *
   * @return {Promise}
   * @resolves When the object is finalized.
   */
  _finalizeInternal() {
    if (this._finalizePromise) {
      // Finalization already in progress; return the pending promise. This is
      // possible if `finalize` is called concurrently with shutdown.
      return this._finalizePromise;
    }
    this._finalizePromise = (async () => {
      await this._saver.finalize();
      this._data = null;
      this.dataReady = false;
    })();
    return this._finalizePromise;
  },

  /**
   * Ensures that all data is persisted to disk, and prevents future calls to
   * `saveSoon`. This is called automatically on shutdown, but can also be
   * called explicitly when the file is no longer needed.
   */
  async finalize() {
    if (this._finalizePromise) {
      throw new Error(`The file ${this.path} has already been finalized`);
    }
    // Wait for finalization before removing the shutdown blocker.
    await this._finalizeInternal();
    this._finalizeAt.removeBlocker(this._finalizeInternalBound);
  },
};
