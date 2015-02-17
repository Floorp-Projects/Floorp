/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, 'Services',
  'resource://gre/modules/Services.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Preferences',
  'resource://gre/modules/Preferences.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'FileUtils',
  'resource://gre/modules/FileUtils.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Log',
  'resource://gre/modules/Log.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Task',
  'resource://gre/modules/Task.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'OS',
  'resource://gre/modules/osfile.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'CommonUtils',
  'resource://services-common/utils.js');

this.EXPORTED_SYMBOLS = [
  "LogManager",
];

const DEFAULT_MAX_ERROR_AGE = 20 * 24 * 60 * 60; // 20 days

// "shared" logs (ie, where the same log name is used by multiple LogManager
// instances) are a fact of life here - eg, FirefoxAccounts logs are used by
// both Sync and Reading-list.
// However, different instances have different pref branches, so we need to
// handle when one pref branch says "Debug" and the other says "Error"
// So we (a) keep singleton console and dump appenders and (b) keep track
// of the minimum (ie, most verbose) level and use that.
// This avoids (a) the most recent setter winning (as that is indeterminate)
// and (b) multiple dump/console appenders being added to the same log multiple
// times, which would cause messages to appear twice.

// Singletons used by each instance.
let formatter;
let dumpAppender;
let consoleAppender;

// A set of all preference roots used by all instances.
let allBranches = new Set();

// The public LogManager object.
function LogManager(prefRoot, logNames, logFilePrefix) {
  this.init(prefRoot, logNames, logFilePrefix);
}

LogManager.prototype = {
  REASON_SUCCESS: "success",
  REASON_ERROR: "error",

  _cleaningUpFileLogs: false,

  _prefObservers: [],

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
          // We need to find the smallest value from all prefs controlling this appender.
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

    this._observeConsolePref = setupAppender(consoleAppender, "log.appender.console", Log.Level.Error, true);
    this._observeDumpPref = setupAppender(dumpAppender, "log.appender.dump", Log.Level.Error, true);

    // The file appender doesn't get the special singleton behaviour.
    let fapp = this._fileAppender = new Log.StorageStreamAppender(formatter);
    // the stream gets a default of Debug as the user must go out of there way
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

  get _logFileDirectory() {
    // At this point we don't allow a custom directory for the logs so
    // about:sync-log can be used. We could fix this later if necessary.
    return FileUtils.getDir("ProfD", ["weave", "logs"]);
  },

  /**
   * Copy an input stream to the named file, doing everything off the main
   * thread.
   * outputFile is an nsIFile, but is used only for the name.
   * Returns a promise that is resolved with the file modification date on
   * completion or rejected if there is an error.
   */
  _copyStreamToFile: Task.async(function* (inputStream, outputFile) {
    // The log data could be large, so we don't want to pass it all in a single
    // message, so use BUFFER_SIZE chunks.
    const BUFFER_SIZE = 8192;

    // get a binary stream
    let binaryStream = Cc['@mozilla.org/binaryinputstream;1'].createInstance(Ci.nsIBinaryInputStream);
    binaryStream.setInputStream(inputStream);
    yield OS.File.makeDir(outputFile.parent.path, { ignoreExisting: true });
    let output = yield OS.File.open(outputFile.path, { write: true} );
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
      inputStream.close();
      binaryStream.close();
      yield output.close();
    }
    this._log.trace("finished copy to", outputFile.path);
    return (yield OS.File.stat(outputFile.path)).lastModificationDate;
  }),

  /**
   * Possibly generate a log file for all accumulated log messages and refresh
   * the input & output streams.
   * Returns a promise that resolves on completion or rejects if the file could
   * not be written.
   */
  resetFileLog(reason) {
    return new Promise((resolve, reject) => {
      try {
        let flushToFile;
        let reasonPrefix;
        switch (reason) {
          case this.REASON_SUCCESS:
            flushToFile = this._prefs.get("log.appender.file.logOnSuccess");
            reasonPrefix = "success";
            break;
          case this.REASON_ERROR:
            flushToFile = this._prefs.get("log.appender.file.logOnError");
            reasonPrefix = "error";
            break;
          default:
            return reject(new Error("Invalid reason"));
        }

        let inStream = this._fileAppender.getInputStream();
        this._fileAppender.reset();
        if (flushToFile && inStream) {
          this._log.debug("Flushing file log");
          let filename = this.logFilePrefix + "-" + reasonPrefix + "-" + Date.now() + ".txt";
          let file = this._logFileDirectory;
          file.append(filename);
          this._log.trace("Beginning stream copy to " + file.leafName + ": " +
                          Date.now());
          this._copyStreamToFile(inStream, file).then(
            modDate => {
              this._log.trace("onCopyComplete: " + Date.now());
              this._log.trace("Output file timestamp: " + modDate + " (" + modDate.getTime() + ")");
            },
            err => {
              this._log.error("Failed to copy log stream to file", err)
              reject(err)
            }
          ).then(
            () => {
              // It's not completely clear to markh why we only do log cleanups
              // for errors, but for now the Sync semantics have been copied...
              // (one theory is that only cleaning up on error makes it less
              // likely old error logs would be removed, but that's not true if
              // there are occasional errors - let's address this later!)
              if (reason == this.REASON_ERROR &&
                  !this._cleaningUpFileLogs) {
                this._log.trace("Scheduling cleanup.");
                // Note we don't return or wait on this promise - it continues
                // in the background
                this.cleanupLogs().then(null, err => {
                  this._log.error("Failed to cleanup logs", err);
                });
              }
              resolve();
            }
          );
        } else {
          resolve();
        }
      } catch (ex) {
        this._log.error("Failed to resetFileLog", ex)
        reject(ex);
      }
    })
  },

  /**
   * Finds all logs older than maxErrorAge and deletes them using async I/O.
   */
  cleanupLogs: Task.async(function* () {
    this._cleaningUpFileLogs = true;
    let iterator = new OS.File.DirectoryIterator(this._logFileDirectory.path);
    let maxAge = this._prefs.get("log.appender.file.maxErrorAge", DEFAULT_MAX_ERROR_AGE);
    let threshold = Date.now() - 1000 * maxAge;

    this._log.debug("Log cleanup threshold time: " + threshold);
    yield iterator.forEach(Task.async(function* (entry) {
      if (!entry.name.startsWith(this.logFilePrefix + "-")) {
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
    this._cleaningUpFileLogs = false;
    this._log.debug("Done deleting files.");
    // This notification is used only for tests.
    Services.obs.notifyObservers(null, "services-tests:common:log-manager:cleanup-logs", null);
  }),
}
