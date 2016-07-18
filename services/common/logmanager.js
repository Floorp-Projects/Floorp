/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict;"

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Log",
  "resource://gre/modules/Log.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CommonUtils",
  "resource://services-common/utils.js");

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Task.jsm");

this.EXPORTED_SYMBOLS = [
  "LogManager",
];

const DEFAULT_MAX_ERROR_AGE = 20 * 24 * 60 * 60; // 20 days

// "shared" logs (ie, where the same log name is used by multiple LogManager
// instances) are a fact of life here - eg, FirefoxAccounts logs are used by
// both Sync and Reading List.
// However, different instances have different pref branches, so we need to
// handle when one pref branch says "Debug" and the other says "Error"
// So we (a) keep singleton console and dump appenders and (b) keep track
// of the minimum (ie, most verbose) level and use that.
// This avoids (a) the most recent setter winning (as that is indeterminate)
// and (b) multiple dump/console appenders being added to the same log multiple
// times, which would cause messages to appear twice.

// Singletons used by each instance.
var formatter;
var dumpAppender;
var consoleAppender;

// A set of all preference roots used by all instances.
var allBranches = new Set();

// A storage appender that is flushable to a file on disk.  Policies for
// when to flush, to what file, log rotation etc are up to the consumer
// (although it does maintain a .sawError property to help the consumer decide
// based on its policies)
function FlushableStorageAppender(formatter) {
  Log.StorageStreamAppender.call(this, formatter);
  this.sawError = false;
}

FlushableStorageAppender.prototype = {
  __proto__: Log.StorageStreamAppender.prototype,

  append(message) {
    if (message.level >= Log.Level.Error) {
      this.sawError = true;
    }
    Log.StorageStreamAppender.prototype.append.call(this, message);
  },

  reset() {
    Log.StorageStreamAppender.prototype.reset.call(this);
    this.sawError = false;
  },

  // Flush the current stream to a file. Somewhat counter-intuitively, you
  // must pass a log which will be written to with details of the operation.
  flushToFile: Task.async(function* (subdirArray, filename, log) {
    let inStream = this.getInputStream();
    this.reset();
    if (!inStream) {
      log.debug("Failed to flush log to a file - no input stream");
      return;
    }
    log.debug("Flushing file log");
    log.trace("Beginning stream copy to " + filename + ": " + Date.now());
    try {
      yield this._copyStreamToFile(inStream, subdirArray, filename, log);
      log.trace("onCopyComplete", Date.now());
    } catch (ex) {
      log.error("Failed to copy log stream to file", ex);
    }
  }),

  /**
   * Copy an input stream to the named file, doing everything off the main
   * thread.
   * subDirArray is an array of path components, relative to the profile
   * directory, where the file will be created.
   * outputFileName is the filename to create.
   * Returns a promise that is resolved on completion or rejected with an error.
   */
  _copyStreamToFile: Task.async(function* (inputStream, subdirArray, outputFileName, log) {
    // The log data could be large, so we don't want to pass it all in a single
    // message, so use BUFFER_SIZE chunks.
    const BUFFER_SIZE = 8192;

    // get a binary stream
    let binaryStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(Ci.nsIBinaryInputStream);
    binaryStream.setInputStream(inputStream);

    let outputDirectory = OS.Path.join(OS.Constants.Path.profileDir, ...subdirArray);
    yield OS.File.makeDir(outputDirectory, { ignoreExisting: true, from: OS.Constants.Path.profileDir });
    let fullOutputFileName = OS.Path.join(outputDirectory, outputFileName);
    let output = yield OS.File.open(fullOutputFileName, { write: true} );
    try {
      while (true) {
        let available = binaryStream.available();
        if (!available) {
          break;
        }
        let chunk = binaryStream.readByteArray(Math.min(available, BUFFER_SIZE));
        yield output.write(new Uint8Array(chunk));
      }
    } finally {
      try {
        binaryStream.close(); // inputStream is closed by the binaryStream
        yield output.close();
      } catch (ex) {
        log.error("Failed to close the input stream", ex);
      }
    }
    log.trace("finished copy to", fullOutputFileName);
  }),
}

// The public LogManager object.
function LogManager(prefRoot, logNames, logFilePrefix) {
  this._prefObservers = [];
  this.init(prefRoot, logNames, logFilePrefix);
}

LogManager.prototype = {
  _cleaningUpFileLogs: false,

  init(prefRoot, logNames, logFilePrefix) {
    if (prefRoot instanceof Preferences) {
      this._prefs = prefRoot;
    } else {
      this._prefs = new Preferences(prefRoot);
    }

    this.logFilePrefix = logFilePrefix;
    if (!formatter) {
      // Create a formatter and various appenders to attach to the logs.
      formatter = new Log.BasicFormatter();
      consoleAppender = new Log.ConsoleAppender(formatter);
      dumpAppender = new Log.DumpAppender(formatter);
    }

    allBranches.add(this._prefs._branchStr);
    // We create a preference observer for all our prefs so they are magically
    // reflected if the pref changes after creation.
    let setupAppender = (appender, prefName, defaultLevel, findSmallest = false) => {
      let observer = newVal => {
        let level = Log.Level[newVal] || defaultLevel;
        if (findSmallest) {
          // As some of our appenders have global impact (ie, there is only one
          // place 'dump' goes to), we need to find the smallest value from all
          // prefs controlling this appender.
          // For example, if consumerA has dump=Debug then consumerB sets
          // dump=Error, we need to keep dump=Debug so consumerA is respected.
          for (let branch of allBranches) {
            let lookPrefBranch = new Preferences(branch);
            let lookVal = Log.Level[lookPrefBranch.get(prefName)];
            if (lookVal && lookVal < level) {
              level = lookVal;
            }
          }
        }
        appender.level = level;
      }
      this._prefs.observe(prefName, observer, this);
      this._prefObservers.push([prefName, observer]);
      // and call the observer now with the current pref value.
      observer(this._prefs.get(prefName));
      return observer;
    }

    this._observeConsolePref = setupAppender(consoleAppender, "log.appender.console", Log.Level.Fatal, true);
    this._observeDumpPref = setupAppender(dumpAppender, "log.appender.dump", Log.Level.Error, true);

    // The file appender doesn't get the special singleton behaviour.
    let fapp = this._fileAppender = new FlushableStorageAppender(formatter);
    // the stream gets a default of Debug as the user must go out of their way
    // to see the stuff spewed to it.
    this._observeStreamPref = setupAppender(fapp, "log.appender.file.level", Log.Level.Debug);

    // now attach the appenders to all our logs.
    for (let logName of logNames) {
      let log = Log.repository.getLogger(logName);
      for (let appender of [fapp, dumpAppender, consoleAppender]) {
        log.addAppender(appender);
      }
    }
    // and use the first specified log as a "root" for our log.
    this._log = Log.repository.getLogger(logNames[0] + ".LogManager");
  },

  /**
   * Cleanup this instance
   */
  finalize() {
    for (let [name, pref] of this._prefObservers) {
      this._prefs.ignore(name, pref, this);
    }
    this._prefObservers = [];
    try {
      allBranches.delete(this._prefs._branchStr);
    } catch (e) {}
    this._prefs = null;
  },

  get _logFileSubDirectoryEntries() {
    // At this point we don't allow a custom directory for the logs, nor allow
    // it to be outside the profile directory.
    // This returns an array of the the relative directory entries below the
    // profile dir, and is the directory about:sync-log uses.
    return ["weave", "logs"];
  },

  get sawError() {
    return this._fileAppender.sawError;
  },

  // Result values for resetFileLog.
  SUCCESS_LOG_WRITTEN: "success-log-written",
  ERROR_LOG_WRITTEN: "error-log-written",

  /**
   * Possibly generate a log file for all accumulated log messages and refresh
   * the input & output streams.
   * Whether a "success" or "error" log is written is determined based on
   * whether an "Error" log entry was written to any of the logs.
   * Returns a promise that resolves on completion with either null (for no
   * file written or on error), SUCCESS_LOG_WRITTEN if a "success" log was
   * written, or ERROR_LOG_WRITTEN if an "error" log was written.
   */
  resetFileLog: Task.async(function* () {
    try {
      let flushToFile;
      let reasonPrefix;
      let reason;
      if (this._fileAppender.sawError) {
        reason = this.ERROR_LOG_WRITTEN;
        flushToFile = this._prefs.get("log.appender.file.logOnError", true);
        reasonPrefix = "error";
      } else {
        reason = this.SUCCESS_LOG_WRITTEN;
        flushToFile = this._prefs.get("log.appender.file.logOnSuccess", false);
        reasonPrefix = "success";
      }

      // might as well avoid creating an input stream if we aren't going to use it.
      if (!flushToFile) {
        this._fileAppender.reset();
        return null;
      }

      // We have reasonPrefix at the start of the filename so all "error"
      // logs are grouped in about:sync-log.
      let filename = reasonPrefix + "-" + this.logFilePrefix + "-" + Date.now() + ".txt";
      yield this._fileAppender.flushToFile(this._logFileSubDirectoryEntries, filename, this._log);

      // It's not completely clear to markh why we only do log cleanups
      // for errors, but for now the Sync semantics have been copied...
      // (one theory is that only cleaning up on error makes it less
      // likely old error logs would be removed, but that's not true if
      // there are occasional errors - let's address this later!)
      if (reason == this.ERROR_LOG_WRITTEN && !this._cleaningUpFileLogs) {
        this._log.trace("Scheduling cleanup.");
        // Note we don't return/yield or otherwise wait on this promise - it
        // continues in the background
        this.cleanupLogs().catch(err => {
          this._log.error("Failed to cleanup logs", err);
        });
      }
      return reason;
    } catch (ex) {
      this._log.error("Failed to resetFileLog", ex);
      return null;
    }
  }),

  /**
   * Finds all logs older than maxErrorAge and deletes them using async I/O.
   */
  cleanupLogs: Task.async(function* () {
    this._cleaningUpFileLogs = true;
    let logDir = FileUtils.getDir("ProfD", this._logFileSubDirectoryEntries);
    let iterator = new OS.File.DirectoryIterator(logDir.path);
    let maxAge = this._prefs.get("log.appender.file.maxErrorAge", DEFAULT_MAX_ERROR_AGE);
    let threshold = Date.now() - 1000 * maxAge;

    this._log.debug("Log cleanup threshold time: " + threshold);
    yield iterator.forEach(Task.async(function* (entry) {
      // Note that we don't check this.logFilePrefix is in the name - we cleanup
      // all files in this directory regardless of that prefix so old logfiles
      // for prefixes no longer in use are still cleaned up. See bug 1279145.
      if (!entry.name.startsWith("error-") &&
          !entry.name.startsWith("success-")) {
        return;
      }
      try {
        // need to call .stat() as the enumerator doesn't give that to us on *nix.
        let info = yield OS.File.stat(entry.path);
        if (info.lastModificationDate.getTime() >= threshold) {
          return;
        }
        this._log.trace(" > Cleanup removing " + entry.name +
                        " (" + info.lastModificationDate.getTime() + ")");
        yield OS.File.remove(entry.path);
        this._log.trace("Deleted " + entry.name);
      } catch (ex) {
        this._log.debug("Encountered error trying to clean up old log file "
                        + entry.name, ex);
      }
    }.bind(this)));
    iterator.close();
    this._cleaningUpFileLogs = false;
    this._log.debug("Done deleting files.");
    // This notification is used only for tests.
    Services.obs.notifyObservers(null, "services-tests:common:log-manager:cleanup-logs", null);
  }),
}
