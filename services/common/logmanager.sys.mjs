/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict;";

import { Log } from "resource://gre/modules/Log.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FileUtils: "resource://gre/modules/FileUtils.sys.mjs",
  NetUtil: "resource://gre/modules/NetUtil.sys.mjs",
});

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

const STREAM_SEGMENT_SIZE = 4096;
const PR_UINT32_MAX = 0xffffffff;

/**
 * Append to an nsIStorageStream
 *
 * This writes logging output to an in-memory stream which can later be read
 * back as an nsIInputStream. It can be used to avoid expensive I/O operations
 * during logging. Instead, one can periodically consume the input stream and
 * e.g. write it to disk asynchronously.
 */
class StorageStreamAppender extends Log.Appender {
  constructor(formatter) {
    super(formatter);
    this._name = "StorageStreamAppender";

    this._converterStream = null; // holds the nsIConverterOutputStream
    this._outputStream = null; // holds the underlying nsIOutputStream

    this._ss = null;
  }

  get outputStream() {
    if (!this._outputStream) {
      // First create a raw stream. We can bail out early if that fails.
      this._outputStream = this.newOutputStream();
      if (!this._outputStream) {
        return null;
      }

      // Wrap the raw stream in an nsIConverterOutputStream. We can reuse
      // the instance if we already have one.
      if (!this._converterStream) {
        this._converterStream = Cc[
          "@mozilla.org/intl/converter-output-stream;1"
        ].createInstance(Ci.nsIConverterOutputStream);
      }
      this._converterStream.init(this._outputStream, "UTF-8");
    }
    return this._converterStream;
  }

  newOutputStream() {
    let ss = (this._ss = Cc["@mozilla.org/storagestream;1"].createInstance(
      Ci.nsIStorageStream
    ));
    ss.init(STREAM_SEGMENT_SIZE, PR_UINT32_MAX, null);
    return ss.getOutputStream(0);
  }

  getInputStream() {
    if (!this._ss) {
      return null;
    }
    return this._ss.newInputStream(0);
  }

  reset() {
    if (!this._outputStream) {
      return;
    }
    this.outputStream.close();
    this._outputStream = null;
    this._ss = null;
  }

  doAppend(formatted) {
    if (!formatted) {
      return;
    }
    try {
      this.outputStream.writeString(formatted + "\n");
    } catch (ex) {
      if (ex.result == Cr.NS_BASE_STREAM_CLOSED) {
        // The underlying output stream is closed, so let's open a new one
        // and try again.
        this._outputStream = null;
      }
      try {
        this.outputStream.writeString(formatted + "\n");
      } catch (ex) {
        // Ah well, we tried, but something seems to be hosed permanently.
      }
    }
  }
}

// A storage appender that is flushable to a file on disk.  Policies for
// when to flush, to what file, log rotation etc are up to the consumer
// (although it does maintain a .sawError property to help the consumer decide
// based on its policies)
class FlushableStorageAppender extends StorageStreamAppender {
  constructor(formatter) {
    super(formatter);
    this.sawError = false;
  }

  append(message) {
    if (message.level >= Log.Level.Error) {
      this.sawError = true;
    }
    StorageStreamAppender.prototype.append.call(this, message);
  }

  reset() {
    super.reset();
    this.sawError = false;
  }

  // Flush the current stream to a file. Somewhat counter-intuitively, you
  // must pass a log which will be written to with details of the operation.
  async flushToFile(subdirArray, filename, log) {
    let inStream = this.getInputStream();
    this.reset();
    if (!inStream) {
      log.debug("Failed to flush log to a file - no input stream");
      return;
    }
    log.debug("Flushing file log");
    log.trace("Beginning stream copy to " + filename + ": " + Date.now());
    try {
      await this._copyStreamToFile(inStream, subdirArray, filename, log);
      log.trace("onCopyComplete", Date.now());
    } catch (ex) {
      log.error("Failed to copy log stream to file", ex);
    }
  }

  /**
   * Copy an input stream to the named file, doing everything off the main
   * thread.
   * subDirArray is an array of path components, relative to the profile
   * directory, where the file will be created.
   * outputFileName is the filename to create.
   * Returns a promise that is resolved on completion or rejected with an error.
   */
  async _copyStreamToFile(inputStream, subdirArray, outputFileName, log) {
    let outputDirectory = PathUtils.join(PathUtils.profileDir, ...subdirArray);
    await IOUtils.makeDirectory(outputDirectory);
    let fullOutputFileName = PathUtils.join(outputDirectory, outputFileName);

    let outputStream = Cc[
      "@mozilla.org/network/file-output-stream;1"
    ].createInstance(Ci.nsIFileOutputStream);

    outputStream.init(
      new lazy.FileUtils.File(fullOutputFileName),
      -1,
      -1,
      Ci.nsIFileOutputStream.DEFER_OPEN
    );

    await new Promise(resolve =>
      lazy.NetUtil.asyncCopy(inputStream, outputStream, () => resolve())
    );

    outputStream.close();
    log.trace("finished copy to", fullOutputFileName);
  }
}

// The public LogManager object.
export function LogManager(prefRoot, logNames, logFilePrefix) {
  this._prefObservers = [];
  this.init(prefRoot, logNames, logFilePrefix);
}

LogManager.StorageStreamAppender = StorageStreamAppender;

LogManager.prototype = {
  _cleaningUpFileLogs: false,

  init(prefRoot, logNames, logFilePrefix) {
    this._prefs = Services.prefs.getBranch(prefRoot);
    this._prefsBranch = prefRoot;

    this.logFilePrefix = logFilePrefix;
    if (!formatter) {
      // Create a formatter and various appenders to attach to the logs.
      formatter = new Log.BasicFormatter();
      consoleAppender = new Log.ConsoleAppender(formatter);
      dumpAppender = new Log.DumpAppender(formatter);
    }

    allBranches.add(this._prefsBranch);
    // We create a preference observer for all our prefs so they are magically
    // reflected if the pref changes after creation.
    let setupAppender = (
      appender,
      prefName,
      defaultLevel,
      findSmallest = false
    ) => {
      let observer = newVal => {
        let level = Log.Level[newVal] || defaultLevel;
        if (findSmallest) {
          // As some of our appenders have global impact (ie, there is only one
          // place 'dump' goes to), we need to find the smallest value from all
          // prefs controlling this appender.
          // For example, if consumerA has dump=Debug then consumerB sets
          // dump=Error, we need to keep dump=Debug so consumerA is respected.
          for (let branch of allBranches) {
            let lookPrefBranch = Services.prefs.getBranch(branch);
            let lookVal = Log.Level[lookPrefBranch.getCharPref(prefName, null)];
            if (lookVal && lookVal < level) {
              level = lookVal;
            }
          }
        }
        appender.level = level;
      };
      this._prefs.addObserver(prefName, observer);
      this._prefObservers.push([prefName, observer]);
      // and call the observer now with the current pref value.
      observer(this._prefs.getCharPref(prefName, null));
      return observer;
    };

    this._observeConsolePref = setupAppender(
      consoleAppender,
      "log.appender.console",
      Log.Level.Fatal,
      true
    );
    this._observeDumpPref = setupAppender(
      dumpAppender,
      "log.appender.dump",
      Log.Level.Error,
      true
    );

    // The file appender doesn't get the special singleton behaviour.
    let fapp = (this._fileAppender = new FlushableStorageAppender(formatter));
    // the stream gets a default of Debug as the user must go out of their way
    // to see the stuff spewed to it.
    this._observeStreamPref = setupAppender(
      fapp,
      "log.appender.file.level",
      Log.Level.Debug
    );

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
    for (let [prefName, observer] of this._prefObservers) {
      this._prefs.removeObserver(prefName, observer);
    }
    this._prefObservers = [];
    try {
      allBranches.delete(this._prefsBranch);
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
  async resetFileLog() {
    try {
      let flushToFile;
      let reasonPrefix;
      let reason;
      if (this._fileAppender.sawError) {
        reason = this.ERROR_LOG_WRITTEN;
        flushToFile = this._prefs.getBoolPref(
          "log.appender.file.logOnError",
          true
        );
        reasonPrefix = "error";
      } else {
        reason = this.SUCCESS_LOG_WRITTEN;
        flushToFile = this._prefs.getBoolPref(
          "log.appender.file.logOnSuccess",
          false
        );
        reasonPrefix = "success";
      }

      // might as well avoid creating an input stream if we aren't going to use it.
      if (!flushToFile) {
        this._fileAppender.reset();
        return null;
      }

      // We have reasonPrefix at the start of the filename so all "error"
      // logs are grouped in about:sync-log.
      let filename =
        reasonPrefix + "-" + this.logFilePrefix + "-" + Date.now() + ".txt";
      await this._fileAppender.flushToFile(
        this._logFileSubDirectoryEntries,
        filename,
        this._log
      );
      // It's not completely clear to markh why we only do log cleanups
      // for errors, but for now the Sync semantics have been copied...
      // (one theory is that only cleaning up on error makes it less
      // likely old error logs would be removed, but that's not true if
      // there are occasional errors - let's address this later!)
      if (reason == this.ERROR_LOG_WRITTEN && !this._cleaningUpFileLogs) {
        this._log.trace("Running cleanup.");
        try {
          await this.cleanupLogs();
        } catch (err) {
          this._log.error("Failed to cleanup logs", err);
        }
      }
      return reason;
    } catch (ex) {
      this._log.error("Failed to resetFileLog", ex);
      return null;
    }
  },

  /**
   * Finds all logs older than maxErrorAge and deletes them using async I/O.
   */
  cleanupLogs() {
    let maxAge = this._prefs.getIntPref(
      "log.appender.file.maxErrorAge",
      DEFAULT_MAX_ERROR_AGE
    );
    let threshold = Date.now() - 1000 * maxAge;
    this._log.debug("Log cleanup threshold time: " + threshold);

    let shouldDelete = fileInfo => {
      return fileInfo.lastModified < threshold;
    };
    return this._deleteLogFiles(shouldDelete);
  },

  /**
   * Finds all logs and removes them.
   */
  removeAllLogs() {
    return this._deleteLogFiles(() => true);
  },

  // Delete some log files. A callback is invoked for each found log file to
  // determine if that file should be removed.
  async _deleteLogFiles(cbShouldDelete) {
    this._cleaningUpFileLogs = true;
    let logDir = lazy.FileUtils.getDir(
      "ProfD",
      this._logFileSubDirectoryEntries
    );
    for (const path of await IOUtils.getChildren(logDir.path)) {
      const name = PathUtils.filename(path);

      if (!name.startsWith("error-") && !name.startsWith("success-")) {
        continue;
      }

      try {
        const info = await IOUtils.stat(path);
        if (!cbShouldDelete(info)) {
          continue;
        }

        this._log.trace(` > Cleanup removing ${name} (${info.lastModified})`);
        await IOUtils.remove(path);
        this._log.trace(`Deleted ${name}`);
      } catch (ex) {
        this._log.debug(
          `Encountered error trying to clean up old log file ${name}`,
          ex
        );
      }
    }
    this._cleaningUpFileLogs = false;
    this._log.debug("Done deleting files.");
    // This notification is used only for tests.
    Services.obs.notifyObservers(
      null,
      "services-tests:common:log-manager:cleanup-logs"
    );
  },
};
