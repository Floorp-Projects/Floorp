/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env worker */

if (this.Components) {
  throw new Error("This worker can only be loaded from a worker thread");
}

// Worker thread for osfile asynchronous front-end

(function(exports) {
  "use strict";

  // Timestamps, for use in Telemetry.
  // The object is set to |null| once it has been sent
  // to the main thread.
  let timeStamps = {
    entered: Date.now(),
    loaded: null,
  };

  // NOTE: osfile.jsm imports require.js
  /* import-globals-from /toolkit/components/workerloader/require.js */
  /* import-globals-from /toolkit/components/osfile/osfile.jsm */
  importScripts("resource://gre/modules/osfile.jsm");

  let PromiseWorker = require("resource://gre/modules/workers/PromiseWorker.js");
  let SharedAll = require("resource://gre/modules/osfile/osfile_shared_allthreads.jsm");
  let LOG = SharedAll.LOG.bind(SharedAll, "Agent");

  let worker = new PromiseWorker.AbstractWorker();
  worker.dispatch = function(method, args = []) {
    let startTime = performance.now();
    try {
      return Agent[method](...args);
    } finally {
      let text = method;
      if (args.length && args[0] instanceof Object && args[0].string) {
        // Including the path in the marker text here means it will be part of
        // profiles. It's fine to include personally identifiable information
        // in profiles, because when a profile is captured only the user will
        // see it, and before uploading it a sanitization step will be offered.
        // The 'OS.File' name will help the profiler know that these markers
        // should be sanitized.
        text += " — " + args[0].string;
      }
      ChromeUtils.addProfilerMarker("OS.File", startTime, text);
    }
  };
  worker.log = LOG;
  worker.postMessage = function(message, ...transfers) {
    if (timeStamps) {
      message.timeStamps = timeStamps;
      timeStamps = null;
    }
    self.postMessage(message, ...transfers);
  };
  worker.close = function() {
    self.close();
  };
  let Meta = PromiseWorker.Meta;

  self.addEventListener("message", msg => worker.handleMessage(msg));
  self.addEventListener("unhandledrejection", function(error) {
    throw error.reason;
  });

  /**
   * A data structure used to track opened resources
   */
  let ResourceTracker = function ResourceTracker() {
    // A number used to generate ids
    this._idgen = 0;
    // A map from id to resource
    this._map = new Map();
  };
  ResourceTracker.prototype = {
    /**
     * Get a resource from its unique identifier.
     */
    get(id) {
      let result = this._map.get(id);
      if (result == null) {
        return result;
      }
      return result.resource;
    },
    /**
     * Remove a resource from its unique identifier.
     */
    remove(id) {
      if (!this._map.has(id)) {
        throw new Error("Cannot find resource id " + id);
      }
      this._map.delete(id);
    },
    /**
     * Add a resource, return a new unique identifier
     *
     * @param {*} resource A resource.
     * @param {*=} info Optional information. For debugging purposes.
     *
     * @return {*} A unique identifier. For the moment, this is a number,
     * but this might not remain the case forever.
     */
    add(resource, info) {
      let id = this._idgen++;
      this._map.set(id, { resource, info });
      return id;
    },
    /**
     * Return a list of all open resources i.e. the ones still present in
     * ResourceTracker's _map.
     */
    listOpenedResources: function listOpenedResources() {
      return Array.from(this._map, ([id, resource]) => resource.info.path);
    },
  };

  /**
   * A map of unique identifiers to opened files.
   */
  let OpenedFiles = new ResourceTracker();

  /**
   * Execute a function in the context of a given file.
   *
   * @param {*} id A unique identifier, as used by |OpenFiles|.
   * @param {Function} f A function to call.
   * @param {boolean} ignoreAbsent If |true|, the error is ignored. Otherwise, the error causes an exception.
   * @return The return value of |f()|
   *
   * This function attempts to get the file matching |id|. If
   * the file exists, it executes |f| within the |this| set
   * to the corresponding file. Otherwise, it throws an error.
   */
  let withFile = function withFile(id, f, ignoreAbsent) {
    let file = OpenedFiles.get(id);
    if (file == null) {
      if (!ignoreAbsent) {
        throw OS.File.Error.closed("accessing file");
      }
      return undefined;
    }
    return f.call(file);
  };

  let OpenedDirectoryIterators = new ResourceTracker();
  let withDir = function withDir(fd, f, ignoreAbsent) {
    let file = OpenedDirectoryIterators.get(fd);
    if (file == null) {
      if (!ignoreAbsent) {
        throw OS.File.Error.closed("accessing directory");
      }
      return undefined;
    }
    if (!(file instanceof File.DirectoryIterator)) {
      throw new Error(
        "file is not a directory iterator " + file.__proto__.toSource()
      );
    }
    return f.call(file);
  };

  let Type = exports.OS.Shared.Type;

  let File = exports.OS.File;

  /**
   * The agent.
   *
   * It is in charge of performing method-specific deserialization
   * of messages, calling the function/method of OS.File and serializing
   * back the results.
   */
  let Agent = {
    // Update worker's OS.Shared.DEBUG flag message from controller.
    SET_DEBUG(aDEBUG) {
      SharedAll.Config.DEBUG = aDEBUG;
    },
    // Return worker's current OS.Shared.DEBUG value to controller.
    // Note: This is used for testing purposes.
    GET_DEBUG() {
      return SharedAll.Config.DEBUG;
    },
    /**
     * Execute shutdown sequence, returning data on leaked file descriptors.
     *
     * @param {bool} If |true|, kill the worker if this would not cause
     * leaks.
     */
    Meta_shutdown(kill) {
      let result = {
        openedFiles: OpenedFiles.listOpenedResources(),
        openedDirectoryIterators: OpenedDirectoryIterators.listOpenedResources(),
        killed: false, // Placeholder
      };

      // Is it safe to kill the worker?
      let safe =
        !result.openedFiles.length && !result.openedDirectoryIterators.length;
      result.killed = safe && kill;

      return new Meta(result, { shutdown: result.killed });
    },
    // Functions of OS.File
    stat: function stat(path, options) {
      return exports.OS.File.Info.toMsg(
        exports.OS.File.stat(Type.path.fromMsg(path), options)
      );
    },
    setPermissions: function setPermissions(path, options = {}) {
      return exports.OS.File.setPermissions(Type.path.fromMsg(path), options);
    },
    setDates: function setDates(path, accessDate, modificationDate) {
      return exports.OS.File.setDates(
        Type.path.fromMsg(path),
        accessDate,
        modificationDate
      );
    },
    getCurrentDirectory: function getCurrentDirectory() {
      return exports.OS.Shared.Type.path.toMsg(File.getCurrentDirectory());
    },
    setCurrentDirectory: function setCurrentDirectory(path) {
      File.setCurrentDirectory(exports.OS.Shared.Type.path.fromMsg(path));
    },
    copy: function copy(sourcePath, destPath, options) {
      return File.copy(
        Type.path.fromMsg(sourcePath),
        Type.path.fromMsg(destPath),
        options
      );
    },
    move: function move(sourcePath, destPath, options) {
      return File.move(
        Type.path.fromMsg(sourcePath),
        Type.path.fromMsg(destPath),
        options
      );
    },
    makeDir: function makeDir(path, options) {
      return File.makeDir(Type.path.fromMsg(path), options);
    },
    removeEmptyDir: function removeEmptyDir(path, options) {
      return File.removeEmptyDir(Type.path.fromMsg(path), options);
    },
    remove: function remove(path, options) {
      return File.remove(Type.path.fromMsg(path), options);
    },
    open: function open(path, mode, options) {
      let filePath = Type.path.fromMsg(path);
      let file = File.open(filePath, mode, options);
      return OpenedFiles.add(file, {
        // Adding path information to keep track of opened files
        // to report leaks when debugging.
        path: filePath,
      });
    },
    openUnique: function openUnique(path, options) {
      let filePath = Type.path.fromMsg(path);
      let openedFile = OS.Shared.AbstractFile.openUnique(filePath, options);
      let resourceId = OpenedFiles.add(openedFile.file, {
        // Adding path information to keep track of opened files
        // to report leaks when debugging.
        path: openedFile.path,
      });

      return {
        path: openedFile.path,
        file: resourceId,
      };
    },
    read: function read(path, bytes, options) {
      let data = File.read(Type.path.fromMsg(path), bytes, options);
      if (typeof data == "string") {
        return data;
      }
      return new Meta(
        {
          buffer: data.buffer,
          byteOffset: data.byteOffset,
          byteLength: data.byteLength,
        },
        {
          transfers: [data.buffer],
        }
      );
    },
    exists: function exists(path) {
      return File.exists(Type.path.fromMsg(path));
    },
    writeAtomic: function writeAtomic(path, buffer, options) {
      if (options.tmpPath) {
        options.tmpPath = Type.path.fromMsg(options.tmpPath);
      }
      return File.writeAtomic(
        Type.path.fromMsg(path),
        Type.voidptr_t.fromMsg(buffer),
        options
      );
    },
    removeDir(path, options) {
      return File.removeDir(Type.path.fromMsg(path), options);
    },
    new_DirectoryIterator: function new_DirectoryIterator(path, options) {
      let directoryPath = Type.path.fromMsg(path);
      let iterator = new File.DirectoryIterator(directoryPath, options);
      return OpenedDirectoryIterators.add(iterator, {
        // Adding path information to keep track of opened directory
        // iterators to report leaks when debugging.
        path: directoryPath,
      });
    },
    // Methods of OS.File
    File_prototype_close: function close(fd) {
      return withFile(fd, function do_close() {
        try {
          return this.close();
        } finally {
          OpenedFiles.remove(fd);
        }
      });
    },
    File_prototype_stat: function stat(fd) {
      return withFile(fd, function do_stat() {
        return exports.OS.File.Info.toMsg(this.stat());
      });
    },
    File_prototype_setPermissions: function setPermissions(fd, options = {}) {
      return withFile(fd, function do_setPermissions() {
        return this.setPermissions(options);
      });
    },
    File_prototype_setDates: function setDates(
      fd,
      accessTime,
      modificationTime
    ) {
      return withFile(fd, function do_setDates() {
        return this.setDates(accessTime, modificationTime);
      });
    },
    File_prototype_read: function read(fd, nbytes, options) {
      return withFile(fd, function do_read() {
        let data = this.read(nbytes, options);
        return new Meta(
          {
            buffer: data.buffer,
            byteOffset: data.byteOffset,
            byteLength: data.byteLength,
          },
          {
            transfers: [data.buffer],
          }
        );
      });
    },
    File_prototype_readTo: function readTo(fd, buffer, options) {
      return withFile(fd, function do_readTo() {
        return this.readTo(
          exports.OS.Shared.Type.voidptr_t.fromMsg(buffer),
          options
        );
      });
    },
    File_prototype_write: function write(fd, buffer, options) {
      return withFile(fd, function do_write() {
        return this.write(
          exports.OS.Shared.Type.voidptr_t.fromMsg(buffer),
          options
        );
      });
    },
    File_prototype_setPosition: function setPosition(fd, pos, whence) {
      return withFile(fd, function do_setPosition() {
        return this.setPosition(pos, whence);
      });
    },
    File_prototype_getPosition: function getPosition(fd) {
      return withFile(fd, function do_getPosition() {
        return this.getPosition();
      });
    },
    File_prototype_flush: function flush(fd) {
      return withFile(fd, function do_flush() {
        return this.flush();
      });
    },
    // Methods of OS.File.DirectoryIterator
    DirectoryIterator_prototype_next: function next(dir) {
      return withDir(
        dir,
        function do_next() {
          let { value, done } = this.next();
          if (done) {
            OpenedDirectoryIterators.remove(dir);
            return { value: undefined, done: true };
          }
          return {
            value: File.DirectoryIterator.Entry.toMsg(value),
            done: false,
          };
        },
        false
      );
    },
    DirectoryIterator_prototype_nextBatch: function nextBatch(dir, size) {
      return withDir(
        dir,
        function do_nextBatch() {
          let result;
          try {
            result = this.nextBatch(size);
          } catch (x) {
            OpenedDirectoryIterators.remove(dir);
            throw x;
          }
          return result.map(File.DirectoryIterator.Entry.toMsg);
        },
        false
      );
    },
    DirectoryIterator_prototype_close: function close(dir) {
      return withDir(
        dir,
        function do_close() {
          this.close();
          OpenedDirectoryIterators.remove(dir);
        },
        true
      ); // ignore error to support double-closing |DirectoryIterator|
    },
    DirectoryIterator_prototype_exists: function exists(dir) {
      return withDir(dir, function do_exists() {
        return this.exists();
      });
    },
  };
  if (!SharedAll.Constants.Win) {
    Agent.unixSymLink = function unixSymLink(sourcePath, destPath) {
      return File.unixSymLink(
        Type.path.fromMsg(sourcePath),
        Type.path.fromMsg(destPath)
      );
    };
  }

  timeStamps.loaded = Date.now();
})(this);
