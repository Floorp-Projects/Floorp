/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/chrome-worker */

/**
 * A worker dedicated for the I/O component of PageThumbs storage.
 */

"use strict";

importScripts("resource://gre/modules/workers/require.js");

var PromiseWorker = require("resource://gre/modules/workers/PromiseWorker.js");

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
self.addEventListener("unhandledrejection", function(error) {
  throw error.reason;
});

var Agent = {
  // Checks if the specified file exists and has an age less than as
  // specifed (in seconds).
  async isFileRecent(path, maxAge) {
    try {
      let stat = await IOUtils.stat(path);
      let maxDate = new Date();
      maxDate.setSeconds(maxDate.getSeconds() - maxAge);
      return stat.lastModified > maxDate;
    } catch (ex) {
      if (!(ex instanceof DOMException)) {
        throw ex;
      }
      // file doesn't exist (or can't be stat'd) - must be stale.
      return false;
    }
  },

  async remove(path) {
    try {
      await IOUtils.remove(path);
      return true;
    } catch (e) {
      return false;
    }
  },

  async expireFilesInDirectory(path, filesToKeep, minChunkSize) {
    let entries = await this.getFileEntriesInDirectory(path, filesToKeep);
    let limit = Math.max(minChunkSize, Math.round(entries.length / 2));

    for (let entry of entries) {
      await this.remove(entry);

      // Check if we reached the limit of files to remove.
      if (--limit <= 0) {
        break;
      }
    }

    return true;
  },

  async getFileEntriesInDirectory(path, skipFiles) {
    let children = await IOUtils.getChildren(path);
    let skip = new Set(skipFiles);

    let entries = [];
    for (let entry of children) {
      let stat = await IOUtils.stat(entry);
      if (stat.type === "regular" && !skip.has(PathUtils.filename(entry))) {
        entries.push(entry);
      }
    }
    return entries;
  },

  async moveOrDeleteAllThumbnails(pathFrom, pathTo) {
    await IOUtils.makeDirectory(pathTo);
    if (pathFrom == pathTo) {
      return true;
    }
    let children = await IOUtils.getChildren(pathFrom);
    for (let entry of children) {
      let stat = await IOUtils.stat(entry);
      if (stat.type !== "regular") {
        continue;
      }

      let fileName = PathUtils.filename(entry);
      let from = PathUtils.join(pathFrom, fileName);
      let to = PathUtils.join(pathTo, fileName);

      try {
        await IOUtils.move(from, to, { noOverwrite: true });
      } catch (e) {
        await IOUtils.remove(from);
      }
    }

    try {
      await IOUtils.remove(pathFrom, { recursive: true });
    } catch (e) {
      // This could fail if there's something in
      // the folder we're not permitted to remove.
    }

    return true;
  },

  writeAtomic(path, buffer, options) {
    return IOUtils.write(path, buffer, options);
  },

  makeDir(path, options) {
    return IOUtils.makeDirectory(path, options);
  },

  copy(source, dest, options) {
    return IOUtils.copy(source, dest, options);
  },

  async wipe(path) {
    let children = await IOUtils.getChildren(path);
    for (let entry of children) {
      try {
        await IOUtils.remove(entry);
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
  },

  exists(path) {
    return IOUtils.exists(path);
  },
};
