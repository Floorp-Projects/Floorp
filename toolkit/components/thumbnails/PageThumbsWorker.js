/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A worker dedicated for the I/O component of PageThumbs storage.
 *
 * Do not rely on the API of this worker. In a future version, it might be
 * fully replaced by a OS.File global I/O worker.
 */

"use strict";

importScripts("resource://gre/modules/osfile.jsm");

var PromiseWorker = require("resource://gre/modules/workers/PromiseWorker.js");

var File = OS.File;
var Type = OS.Shared.Type;

var worker = new PromiseWorker.AbstractWorker();
worker.dispatch = function(method, args = []) {
  return Agent[method](...args);
};
worker.postMessage = function(message, ...transfers) {
  self.postMessage(message, ...transfers);
};
worker.close = function() {
  self.close();
};

self.addEventListener("message", msg => worker.handleMessage(msg));


var Agent = {
  // Checks if the specified file exists and has an age less than as
  // specifed (in seconds).
  isFileRecent: function Agent_isFileRecent(path, maxAge) {
    try {
      let stat = OS.File.stat(path);
      let maxDate = new Date();
      maxDate.setSeconds(maxDate.getSeconds() - maxAge);
      return stat.lastModificationDate > maxDate;
    } catch (ex) {
      if (!(ex instanceof OS.File.Error)) {
        throw ex;
      }
      // file doesn't exist (or can't be stat'd) - must be stale.
      return false;
    }
  },

  remove: function Agent_removeFile(path) {
    try {
      OS.File.remove(path);
      return true;
    } catch (e) {
      return false;
    }
  },

  expireFilesInDirectory:
  function Agent_expireFilesInDirectory(path, filesToKeep, minChunkSize) {
    let entries = this.getFileEntriesInDirectory(path, filesToKeep);
    let limit = Math.max(minChunkSize, Math.round(entries.length / 2));

    for (let entry of entries) {
      this.remove(entry.path);

      // Check if we reached the limit of files to remove.
      if (--limit <= 0) {
        break;
      }
    }

    return true;
  },

  getFileEntriesInDirectory:
  function Agent_getFileEntriesInDirectory(path, skipFiles) {
    let iter = new OS.File.DirectoryIterator(path);
    if (!iter.exists()) {
      return [];
    }

    let skip = new Set(skipFiles);

    let entries = [];
    for (let entry in iter) {
      if (!entry.isDir && !entry.isSymLink && !skip.has(entry.name)) {
        entries.push(entry);
      }
    }
    return entries;
  },

  moveOrDeleteAllThumbnails:
  function Agent_moveOrDeleteAllThumbnails(pathFrom, pathTo) {
    OS.File.makeDir(pathTo, {ignoreExisting: true});
    if (pathFrom == pathTo) {
      return true;
    }
    let iter = new OS.File.DirectoryIterator(pathFrom);
    if (iter.exists()) {
      for (let entry in iter) {
        if (entry.isDir || entry.isSymLink) {
          continue;
        }


        let from = OS.Path.join(pathFrom, entry.name);
        let to = OS.Path.join(pathTo, entry.name);

        try {
          OS.File.move(from, to, {noOverwrite: true, noCopy: true});
        } catch (e) {
          OS.File.remove(from);
        }
      }
    }
    iter.close();

    try {
      OS.File.removeEmptyDir(pathFrom);
    } catch (e) {
      // This could fail if there's something in
      // the folder we're not permitted to remove.
    }

    return true;
  },

  writeAtomic: function Agent_writeAtomic(path, buffer, options) {
    return File.writeAtomic(path,
      buffer,
      options);
  },

  makeDir: function Agent_makeDir(path, options) {
    return File.makeDir(path, options);
  },

  copy: function Agent_copy(source, dest, options) {
    return File.copy(source, dest, options);
  },

  wipe: function Agent_wipe(path) {
    let iterator = new File.DirectoryIterator(path);
    try {
      for (let entry in iterator) {
        try {
          File.remove(entry.path);
        } catch (ex) {
          // If a file cannot be removed, we should still continue.
          // This can happen at least for any of the following reasons:
          // - access denied;
          // - file has been removed recently during a previous wipe
          //  and the file system has not flushed that yet (yes, this
          //  can happen under Windows);
          // - file has been removed by the user or another process.
        }
      }
    } finally {
      iterator.close();
    }
  },

  exists: function Agent_exists(path) {
    return File.exists(path);
  },
};

