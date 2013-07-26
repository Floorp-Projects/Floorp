/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
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

////////////////////////////////////////////////////////////////////////////////
//// Globals

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Downloads",
                                  "resource://gre/modules/Downloads.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm")
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyGetter(this, "gTextDecoder", function () {
  return new TextDecoder();
});

XPCOMUtils.defineLazyGetter(this, "gTextEncoder", function () {
  return new TextEncoder();
});

////////////////////////////////////////////////////////////////////////////////
//// DownloadStore

/**
 * Handles serialization of Download objects and persistence into a file, so
 * that the state of downloads can be restored across sessions.
 *
 * @param aList
 *        DownloadList object to be populated or serialized.
 * @param aPath
 *        String containing the file path where data should be saved.
 */
function DownloadStore(aList, aPath)
{
  this.list = aList;
  this.path = aPath;
}

DownloadStore.prototype = {
  /**
   * DownloadList object to be populated or serialized.
   */
  list: null,

  /**
   * String containing the file path where data should be saved.
   */
  path: "",

  /**
   * Loads persistent downloads from the file to the list.
   *
   * @return {Promise}
   * @resolves When the operation finished successfully.
   * @rejects JavaScript exception.
   */
  load: function DS_load()
  {
    return Task.spawn(function task_DS_load() {
      let bytes;
      try {
        bytes = yield OS.File.read(this.path);
      } catch (ex if ex instanceof OS.File.Error && ex.becauseNoSuchFile) {
        // If the file does not exist, there are no downloads to load.
        return;
      }

      let storeData = JSON.parse(gTextDecoder.decode(bytes));

      // Create live downloads based on the static snapshot.
      for (let downloadData of storeData.list) {
        try {
          this.list.add(yield Downloads.createDownload(downloadData));
        } catch (ex) {
          // If an item is unrecognized, don't prevent others from being loaded.
          Cu.reportError(ex);
        }
      }
    }.bind(this));
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
  save: function DS_save()
  {
    return Task.spawn(function task_DS_save() {
      let downloads = yield this.list.getAll();

      // Take a static snapshot of the current state of all the downloads.
      let storeData = { list: [] };
      let atLeastOneDownload = false;
      for (let download of downloads) {
        try {
          storeData.list.push(download.toSerializable());
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
        yield OS.File.writeAtomic(this.path, bytes,
                                  { tmpPath: this.path + ".tmp" });
      } else {
        // Remove the file if there are no downloads to save at all.
        try {
          yield OS.File.remove(this.path);
        } catch (ex if ex instanceof OS.File.Error &&
                 (ex.becauseNoSuchFile || ex.becauseAccessDenied)) {
          // On Windows, we may get an access denied error instead of a no such
          // file error if the file existed before, and was recently deleted.
        }
      }
    }.bind(this));
  },
};
