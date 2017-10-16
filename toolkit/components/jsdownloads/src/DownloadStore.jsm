/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Handles serialization of Download objects and persistence into a file, so
 * that the state of downloads can be restored across sessions.
 *
 * The file is stored in JSON format, without indentation.  With indentation
 * applied, the file would look like this:
 *
 * {
 *   "list": [
 *     {
 *       "source": "http://www.example.com/download.txt",
 *       "target": "/home/user/Downloads/download.txt"
 *     },
 *     {
 *       "source": {
 *         "url": "http://www.example.com/download.txt",
 *         "referrer": "http://www.example.com/referrer.html"
 *       },
 *       "target": "/home/user/Downloads/download-2.txt"
 *     }
 *   ]
 * }
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "DownloadStore",
];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Downloads",
                                  "resource://gre/modules/Downloads.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

XPCOMUtils.defineLazyGetter(this, "gTextDecoder", function() {
  return new TextDecoder();
});

XPCOMUtils.defineLazyGetter(this, "gTextEncoder", function() {
  return new TextEncoder();
});

/**
 * Handles serialization of Download objects and persistence into a file, so
 * that the state of downloads can be restored across sessions.
 *
 * @param aList
 *        DownloadList object to be populated or serialized.
 * @param aPath
 *        String containing the file path where data should be saved.
 */
this.DownloadStore = function(aList, aPath) {
  this.list = aList;
  this.path = aPath;
};

this.DownloadStore.prototype = {
  /**
   * DownloadList object to be populated or serialized.
   */
  list: null,

  /**
   * String containing the file path where data should be saved.
   */
  path: "",

  /**
   * This function is called with a Download object as its first argument, and
   * should return true if the item should be saved.
   */
  onsaveitem: () => true,

  /**
   * Loads persistent downloads from the file to the list.
   *
   * @return {Promise}
   * @resolves When the operation finished successfully.
   * @rejects JavaScript exception.
   */
  load: function DS_load() {
    return (async () => {
      let bytes;
      try {
        bytes = await OS.File.read(this.path);
      } catch (ex) {
        if (!(ex instanceof OS.File.Error) || !ex.becauseNoSuchFile) {
          throw ex;
        }
        // If the file does not exist, there are no downloads to load.
        return;
      }

      let storeData = JSON.parse(gTextDecoder.decode(bytes));

      // Create live downloads based on the static snapshot.
      for (let downloadData of storeData.list) {
        try {
          let download = await Downloads.createDownload(downloadData);
          try {
            if (!download.succeeded && !download.canceled && !download.error) {
              // Try to restart the download if it was in progress during the
              // previous session.  Ignore errors.
              download.start().catch(() => {});
            } else {
              // If the download was not in progress, try to update the current
              // progress from disk.  This is relevant in case we retained
              // partially downloaded data.
              await download.refresh();
            }
          } finally {
            // Add the download to the list if we succeeded in creating it,
            // after we have updated its initial state.
            await this.list.add(download);
          }
        } catch (ex) {
          // If an item is unrecognized, don't prevent others from being loaded.
          Cu.reportError(ex);
        }
      }
    })();
  },

  /**
   * Saves persistent downloads from the list to the file.
   *
   * If an error occurs, the previous file is not deleted.
   *
   * @return {Promise}
   * @resolves When the operation finished successfully.
   * @rejects JavaScript exception.
   */
  save: function DS_save() {
    return (async () => {
      let downloads = await this.list.getAll();

      // Take a static snapshot of the current state of all the downloads.
      let storeData = { list: [] };
      let atLeastOneDownload = false;
      for (let download of downloads) {
        try {
          if (!this.onsaveitem(download)) {
            continue;
          }

          let serializable = download.toSerializable();
          if (!serializable) {
            // This item cannot be persisted across sessions.
            continue;
          }
          storeData.list.push(serializable);
          atLeastOneDownload = true;
        } catch (ex) {
          // If an item cannot be converted to a serializable form, don't
          // prevent others from being saved.
          Cu.reportError(ex);
        }
      }

      if (atLeastOneDownload) {
        // Create or overwrite the file if there are downloads to save.
        let bytes = gTextEncoder.encode(JSON.stringify(storeData));
        await OS.File.writeAtomic(this.path, bytes,
                                  { tmpPath: this.path + ".tmp" });
      } else {
        // Remove the file if there are no downloads to save at all.
        try {
          await OS.File.remove(this.path);
        } catch (ex) {
          if (!(ex instanceof OS.File.Error) ||
              !(ex.becauseNoSuchFile || ex.becauseAccessDenied)) {
            throw ex;
          }
          // On Windows, we may get an access denied error instead of a no such
          // file error if the file existed before, and was recently deleted.
        }
      }
    })();
  },
};
