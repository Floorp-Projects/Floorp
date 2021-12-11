/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const myScope = this;

const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Log: "resource://gre/modules/Log.jsm",
  TelemetryController: "resource://gre/modules/TelemetryController.jsm",
});

var EXPORTED_SYMBOLS = ["CrashManager", "getCrashManager"];

/**
 * How long to wait after application startup before crash event files are
 * automatically aggregated.
 *
 * We defer aggregation for performance reasons, as we don't want too many
 * services competing for I/O immediately after startup.
 */
const AGGREGATE_STARTUP_DELAY_MS = 57000;

const MILLISECONDS_IN_DAY = 24 * 60 * 60 * 1000;

// Converts Date to days since UNIX epoch.
// This was copied from /services/metrics.storage.jsm. The implementation
// does not account for leap seconds.
function dateToDays(date) {
  return Math.floor(date.getTime() / MILLISECONDS_IN_DAY);
}

/**
 * Get a field from the specified object and remove it.
 *
 * @param obj {Object} The object holding the field
 * @param field {String} The name of the field to be parsed and removed
 *
 * @returns {String} the field contents as a string, null if none was found
 */
function getAndRemoveField(obj, field) {
  let value = null;

  if (field in obj) {
    value = obj[field];
    delete obj[field];
  }

  return value;
}

/**
 * Parse the string stored in the specified field as JSON and then remove the
 * field from the object.
 *
 * @param obj {Object} The object holding the field
 * @param field {String} The name of the field to be parsed and removed
 *
 * @returns {Object} the parsed object, null if none was found
 */
function parseAndRemoveField(obj, field) {
  let value = null;

  if (field in obj) {
    try {
      value = JSON.parse(obj[field]);
    } catch (e) {
      Cu.reportError(e);
    }

    delete obj[field];
  }

  return value;
}

/**
 * A gateway to crash-related data.
 *
 * This type is generic and can be instantiated any number of times.
 * However, most applications will typically only have one instance
 * instantiated and that instance will point to profile and user appdata
 * directories.
 *
 * Instances are created by passing an object with properties.
 * Recognized properties are:
 *
 *   pendingDumpsDir (string) (required)
 *     Where dump files that haven't been uploaded are located.
 *
 *   submittedDumpsDir (string) (required)
 *     Where records of uploaded dumps are located.
 *
 *   eventsDirs (array)
 *     Directories (defined as strings) where events files are written. This
 *     instance will collects events from files in the directories specified.
 *
 *   storeDir (string)
 *     Directory we will use for our data store. This instance will write
 *     data files into the directory specified.
 *
 *   telemetryStoreSizeKey (string)
 *     Telemetry histogram to report store size under.
 */
var CrashManager = function(options) {
  for (let k in options) {
    let value = options[k];

    switch (k) {
      case "pendingDumpsDir":
      case "submittedDumpsDir":
      case "eventsDirs":
      case "storeDir":
        let key = "_" + k;
        delete this[key];
        Object.defineProperty(this, key, { value });
        break;
      case "telemetryStoreSizeKey":
        this._telemetryStoreSizeKey = value;
        break;

      default:
        throw new Error("Unknown property in options: " + k);
    }
  }

  // Promise for in-progress aggregation operation. We store it on the
  // object so it can be returned for in-progress operations.
  this._aggregatePromise = null;

  // Map of crash ID / promise tuples used to track adding new crashes.
  this._crashPromises = new Map();

  // Promise for the crash ping used only for testing.
  this._pingPromise = null;

  // The CrashStore currently attached to this object.
  this._store = null;

  // A Task to retrieve the store. This is needed to avoid races when
  // _getStore() is called multiple times in a short interval.
  this._getStoreTask = null;

  // The timer controlling the expiration of the CrashStore instance.
  this._storeTimer = null;

  // This is a semaphore that prevents the store from being freed by our
  // timer-based resource freeing mechanism.
  this._storeProtectedCount = 0;
};

CrashManager.prototype = Object.freeze({
  // A crash in the main process.
  PROCESS_TYPE_MAIN: "main",

  // A crash in a content process.
  PROCESS_TYPE_CONTENT: "content",

  // A crash in a Gecko media plugin process.
  PROCESS_TYPE_GMPLUGIN: "gmplugin",

  // A crash in the GPU process.
  PROCESS_TYPE_GPU: "gpu",

  // A crash in the VR process.
  PROCESS_TYPE_VR: "vr",

  // A crash in the RDD process.
  PROCESS_TYPE_RDD: "rdd",

  // A crash in the socket process.
  PROCESS_TYPE_SOCKET: "socket",

  // A real crash.
  CRASH_TYPE_CRASH: "crash",

  // A hang.
  CRASH_TYPE_HANG: "hang",

  // Submission result values.
  SUBMISSION_RESULT_OK: "ok",
  SUBMISSION_RESULT_FAILED: "failed",

  DUMP_REGEX: /^([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})\.dmp$/i,
  SUBMITTED_REGEX: /^bp-(?:hr-)?([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})\.txt$/i,
  ALL_REGEX: /^(.*)$/,

  // How long the store object should persist in memory before being
  // automatically garbage collected.
  STORE_EXPIRATION_MS: 60 * 1000,

  // Number of days after which a crash with no activity will get purged.
  PURGE_OLDER_THAN_DAYS: 180,

  // The following are return codes for individual event file processing.
  // File processed OK.
  EVENT_FILE_SUCCESS: "ok",
  // The event appears to be malformed.
  EVENT_FILE_ERROR_MALFORMED: "malformed",
  // The event is obsolete.
  EVENT_FILE_ERROR_OBSOLETE: "obsolete",
  // The type of event is unknown.
  EVENT_FILE_ERROR_UNKNOWN_EVENT: "unknown-event",

  _lazyGetDir(field, path, leaf) {
    delete this[field];
    let value = PathUtils.join(path, leaf);
    Object.defineProperty(this, field, { value });
    return value;
  },

  get _crDir() {
    return this._lazyGetDir(
      "_crDir",
      Services.dirsvc.get("UAppData", Ci.nsIFile).path,
      "Crash Reports"
    );
  },

  get _storeDir() {
    return this._lazyGetDir(
      "_storeDir",
      Services.dirsvc.get("ProfD", Ci.nsIFile).path,
      "crashes"
    );
  },

  get _pendingDumpsDir() {
    return this._lazyGetDir("_pendingDumpsDir", this._crDir, "pending");
  },

  get _submittedDumpsDir() {
    return this._lazyGetDir("_submittedDumpsDir", this._crDir, "submitted");
  },

  get _eventsDirs() {
    delete this._eventsDirs;
    let value = [
      PathUtils.join(this._crDir, "events"),
      PathUtils.join(this._storeDir, "events"),
    ];
    Object.defineProperty(this, "_eventsDirs", { value });
    return value;
  },

  /**
   * Obtain a list of all dumps pending upload.
   *
   * The returned value is a promise that resolves to an array of objects
   * on success. Each element in the array has the following properties:
   *
   *   id (string)
   *      The ID of the crash (a UUID).
   *
   *   path (string)
   *      The filename of the crash (<UUID.dmp>)
   *
   *   date (Date)
   *      When this dump was created
   *
   * The returned arry is sorted by the modified time of the file backing
   * the entry, oldest to newest.
   *
   * @return Promise<Array>
   */
  pendingDumps() {
    return this._getDirectoryEntries(this._pendingDumpsDir, this.DUMP_REGEX);
  },

  /**
   * Obtain a list of all dump files corresponding to submitted crashes.
   *
   * The returned value is a promise that resolves to an Array of
   * objects. Each object has the following properties:
   *
   *   path (string)
   *     The path of the file this entry comes from.
   *
   *   id (string)
   *     The crash UUID.
   *
   *   date (Date)
   *     The (estimated) date this crash was submitted.
   *
   * The returned array is sorted by the modified time of the file backing
   * the entry, oldest to newest.
   *
   * @return Promise<Array>
   */
  submittedDumps() {
    return this._getDirectoryEntries(
      this._submittedDumpsDir,
      this.SUBMITTED_REGEX
    );
  },

  /**
   * Aggregates "loose" events files into the unified "database."
   *
   * This function should be called periodically to collect metadata from
   * all events files into the central data store maintained by this manager.
   *
   * Once events have been stored in the backing store the corresponding
   * source files are deleted.
   *
   * Only one aggregation operation is allowed to occur at a time. If this
   * is called when an existing aggregation is in progress, the promise for
   * the original call will be returned.
   *
   * @return promise<int> The number of event files that were examined.
   */
  aggregateEventsFiles() {
    if (this._aggregatePromise) {
      return this._aggregatePromise;
    }

    return (this._aggregatePromise = (async () => {
      if (this._aggregatePromise) {
        return this._aggregatePromise;
      }

      try {
        let unprocessedFiles = await this._getUnprocessedEventsFiles();

        let deletePaths = [];
        let needsSave = false;

        this._storeProtectedCount++;
        for (let entry of unprocessedFiles) {
          try {
            let result = await this._processEventFile(entry);

            switch (result) {
              case this.EVENT_FILE_SUCCESS:
                needsSave = true;
              // Fall through.

              case this.EVENT_FILE_ERROR_MALFORMED:
              case this.EVENT_FILE_ERROR_OBSOLETE:
                deletePaths.push(entry.path);
                break;

              case this.EVENT_FILE_ERROR_UNKNOWN_EVENT:
                break;

              default:
                Cu.reportError(
                  "Unhandled crash event file return code. Please " +
                    "file a bug: " +
                    result
                );
            }
          } catch (ex) {
            if (ex instanceof DOMException) {
              this._log.warn("I/O error reading " + entry.path, ex);
            } else {
              // We should never encounter an exception. This likely represents
              // a coding error because all errors should be detected and
              // converted to return codes.
              //
              // If we get here, report the error and delete the source file
              // so we don't see it again.
              Cu.reportError(
                "Exception when processing crash event file: " +
                  Log.exceptionStr(ex)
              );
              deletePaths.push(entry.path);
            }
          }
        }

        if (needsSave) {
          let store = await this._getStore();
          await store.save();
        }

        for (let path of deletePaths) {
          try {
            await IOUtils.remove(path);
          } catch (ex) {
            this._log.warn("Error removing event file (" + path + ")", ex);
          }
        }

        return unprocessedFiles.length;
      } finally {
        this._aggregatePromise = false;
        this._storeProtectedCount--;
      }
    })());
  },

  /**
   * Prune old crash data.
   *
   * @param date
   *        (Date) The cutoff point for pruning. Crashes without data newer
   *        than this will be pruned.
   */
  pruneOldCrashes(date) {
    return (async () => {
      let store = await this._getStore();
      store.pruneOldCrashes(date);
      await store.save();
    })();
  },

  /**
   * Run tasks that should be periodically performed.
   */
  runMaintenanceTasks() {
    return (async () => {
      await this.aggregateEventsFiles();

      let offset = this.PURGE_OLDER_THAN_DAYS * MILLISECONDS_IN_DAY;
      await this.pruneOldCrashes(new Date(Date.now() - offset));
    })();
  },

  /**
   * Schedule maintenance tasks for some point in the future.
   *
   * @param delay
   *        (integer) Delay in milliseconds when maintenance should occur.
   */
  scheduleMaintenance(delay) {
    let deferred = PromiseUtils.defer();

    setTimeout(() => {
      this.runMaintenanceTasks().then(deferred.resolve, deferred.reject);
    }, delay);

    return deferred.promise;
  },

  /**
   * Record the occurrence of a crash.
   *
   * This method skips event files altogether and writes directly and
   * immediately to the manager's data store.
   *
   * @param processType (string) One of the PROCESS_TYPE constants.
   * @param crashType (string) One of the CRASH_TYPE constants.
   * @param id (string) Crash ID. Likely a UUID.
   * @param date (Date) When the crash occurred.
   * @param metadata (dictionary) Crash metadata, may be empty.
   *
   * @return promise<null> Resolved when the store has been saved.
   */
  addCrash(processType, crashType, id, date, metadata) {
    let promise = (async () => {
      let store = await this._getStore();
      if (store.addCrash(processType, crashType, id, date, metadata)) {
        await store.save();
      }

      let deferred = this._crashPromises.get(id);

      if (deferred) {
        this._crashPromises.delete(id);
        deferred.resolve();
      }

      // Send a telemetry ping for each non-main process crash
      if (
        processType === this.PROCESS_TYPE_CONTENT ||
        processType === this.PROCESS_TYPE_GPU ||
        processType === this.PROCESS_TYPE_VR ||
        processType === this.PROCESS_TYPE_RDD ||
        processType === this.PROCESS_TYPE_SOCKET
      ) {
        this._sendCrashPing(id, processType, date, metadata);
      }
    })();

    return promise;
  },

  /**
   * Returns a promise that is resolved only the crash with the specified id
   * has been fully recorded.
   *
   * @param id (string) Crash ID. Likely a UUID.
   *
   * @return promise<null> Resolved when the crash is present.
   */
  async ensureCrashIsPresent(id) {
    let store = await this._getStore();
    let crash = store.getCrash(id);

    if (crash) {
      return Promise.resolve();
    }

    let deferred = PromiseUtils.defer();

    this._crashPromises.set(id, deferred);
    return deferred.promise;
  },

  /**
   * Record the remote ID for a crash.
   *
   * @param crashID (string) Crash ID. Likely a UUID.
   * @param remoteID (Date) Server/Breakpad ID.
   *
   * @return boolean True if the remote ID was recorded.
   */
  async setRemoteCrashID(crashID, remoteID) {
    let store = await this._getStore();
    if (store.setRemoteCrashID(crashID, remoteID)) {
      await store.save();
    }
  },

  /**
   * Generate a submission ID for use with addSubmission{Attempt,Result}.
   */
  generateSubmissionID() {
    return (
      "sub-" +
      Services.uuid
        .generateUUID()
        .toString()
        .slice(1, -1)
    );
  },

  /**
   * Record the occurrence of a submission attempt for a crash.
   *
   * @param crashID (string) Crash ID. Likely a UUID.
   * @param submissionID (string) Submission ID. Likely a UUID.
   * @param date (Date) When the attempt occurred.
   *
   * @return boolean True if the attempt was recorded and false if not.
   */
  async addSubmissionAttempt(crashID, submissionID, date) {
    let store = await this._getStore();
    if (store.addSubmissionAttempt(crashID, submissionID, date)) {
      await store.save();
    }
  },

  /**
   * Record the occurrence of a submission result for a crash.
   *
   * @param crashID (string) Crash ID. Likely a UUID.
   * @param submissionID (string) Submission ID. Likely a UUID.
   * @param date (Date) When the submission result was obtained.
   * @param result (string) One of the SUBMISSION_RESULT constants.
   *
   * @return boolean True if the result was recorded and false if not.
   */
  async addSubmissionResult(crashID, submissionID, date, result) {
    let store = await this._getStore();
    if (store.addSubmissionResult(crashID, submissionID, date, result)) {
      await store.save();
    }
  },

  /**
   * Set the classification of a crash.
   *
   * @param crashID (string) Crash ID. Likely a UUID.
   * @param classifications (array) Crash classifications.
   *
   * @return boolean True if the data was recorded and false if not.
   */
  async setCrashClassifications(crashID, classifications) {
    let store = await this._getStore();
    if (store.setCrashClassifications(crashID, classifications)) {
      await store.save();
    }
  },

  /**
   * Obtain the paths of all unprocessed events files.
   *
   * The promise-resolved array is sorted by file mtime, oldest to newest.
   */
  _getUnprocessedEventsFiles() {
    return (async () => {
      try {
        let entries = [];

        for (let dir of this._eventsDirs) {
          for (let e of await this._getDirectoryEntries(dir, this.ALL_REGEX)) {
            entries.push(e);
          }
        }

        entries.sort((a, b) => {
          return a.date - b.date;
        });

        return entries;
      } catch (e) {
        Cu.reportError(e);
        return [];
      }
    })();
  },

  // See docs/crash-events.rst for the file format specification.
  _processEventFile(entry) {
    return (async () => {
      let data = await IOUtils.read(entry.path);
      let store = await this._getStore();

      let decoder = new TextDecoder();
      data = decoder.decode(data);

      let type, time;
      let start = 0;
      for (let i = 0; i < 2; i++) {
        let index = data.indexOf("\n", start);
        if (index == -1) {
          return this.EVENT_FILE_ERROR_MALFORMED;
        }

        let sub = data.substring(start, index);
        switch (i) {
          case 0:
            type = sub;
            break;
          case 1:
            time = sub;
            try {
              time = parseInt(time, 10);
            } catch (ex) {
              return this.EVENT_FILE_ERROR_MALFORMED;
            }
        }

        start = index + 1;
      }
      let date = new Date(time * 1000);
      let payload = data.substring(start);

      return this._handleEventFilePayload(store, entry, type, date, payload);
    })();
  },

  _filterAnnotations(annotations) {
    let filteredAnnotations = {};
    let crashReporter = Cc["@mozilla.org/toolkit/crash-reporter;1"].getService(
      Ci.nsICrashReporter
    );

    for (let line in annotations) {
      try {
        if (crashReporter.isAnnotationWhitelistedForPing(line)) {
          filteredAnnotations[line] = annotations[line];
        }
      } catch (e) {
        // Silently drop unknown annotations
      }
    }

    return filteredAnnotations;
  },

  _sendCrashPing(crashId, type, date, metadata = {}) {
    // If we have a saved environment, use it. Otherwise report
    // the current environment.
    let reportMeta = Cu.cloneInto(metadata, myScope);
    let crashEnvironment = parseAndRemoveField(
      reportMeta,
      "TelemetryEnvironment"
    );
    let sessionId = getAndRemoveField(reportMeta, "TelemetrySessionId");
    let stackTraces = getAndRemoveField(reportMeta, "StackTraces");
    let minidumpSha256Hash = getAndRemoveField(
      reportMeta,
      "MinidumpSha256Hash"
    );

    // Filter the remaining annotations to remove privacy-sensitive ones
    reportMeta = this._filterAnnotations(reportMeta);

    this._pingPromise = TelemetryController.submitExternalPing(
      "crash",
      {
        version: 1,
        crashDate: date.toISOString().slice(0, 10), // YYYY-MM-DD
        crashTime: date.toISOString().slice(0, 13) + ":00:00.000Z", // per-hour resolution
        sessionId,
        crashId,
        minidumpSha256Hash,
        processType: type,
        stackTraces,
        metadata: reportMeta,
        hasCrashEnvironment: crashEnvironment !== null,
      },
      {
        addClientId: true,
        addEnvironment: true,
        overrideEnvironment: crashEnvironment,
      }
    );
  },

  _handleEventFilePayload(store, entry, type, date, payload) {
    // The payload types and formats are documented in docs/crash-events.rst.
    // Do not change the format of an existing type. Instead, invent a new
    // type.
    // DO NOT ADD NEW TYPES WITHOUT DOCUMENTING!
    let lines = payload.split("\n");

    switch (type) {
      case "crash.main.1":
      case "crash.main.2":
        return this.EVENT_FILE_ERROR_OBSOLETE;

      case "crash.main.3":
        let crashID = lines[0];
        let metadata = JSON.parse(lines[1]);
        store.addCrash(
          this.PROCESS_TYPE_MAIN,
          this.CRASH_TYPE_CRASH,
          crashID,
          date,
          metadata
        );

        if (!("CrashPingUUID" in metadata)) {
          // If CrashPingUUID is not present then a ping was not generated
          // by the crashreporter for this crash so we need to send one from
          // here.
          this._sendCrashPing(crashID, this.PROCESS_TYPE_MAIN, date, metadata);
        }

        break;

      case "crash.submission.1":
        if (lines.length == 3) {
          let [crashID, result, remoteID] = lines;
          store.addCrash(
            this.PROCESS_TYPE_MAIN,
            this.CRASH_TYPE_CRASH,
            crashID,
            date
          );

          let submissionID = this.generateSubmissionID();
          let succeeded = result === "true";
          store.addSubmissionAttempt(crashID, submissionID, date);
          store.addSubmissionResult(
            crashID,
            submissionID,
            date,
            succeeded
              ? this.SUBMISSION_RESULT_OK
              : this.SUBMISSION_RESULT_FAILED
          );
          if (succeeded) {
            store.setRemoteCrashID(crashID, remoteID);
          }
        } else {
          return this.EVENT_FILE_ERROR_MALFORMED;
        }
        break;

      default:
        return this.EVENT_FILE_ERROR_UNKNOWN_EVENT;
    }

    return this.EVENT_FILE_SUCCESS;
  },

  /**
   * The resolved promise is an array of objects with the properties:
   *
   *   path -- String filename
   *   id -- regexp.match()[1] (likely the crash ID)
   *   date -- Date mtime of the file
   */
  _getDirectoryEntries(path, re) {
    return (async function() {
      let children = await IOUtils.getChildren(path);
      let entries = [];

      for (const entry of children) {
        let stat = await IOUtils.stat(entry);
        if (stat.type == "directory") {
          continue;
        }

        let filename = PathUtils.filename(entry);
        let match = re.exec(filename);
        if (!match) {
          continue;
        }
        entries.push({
          path: entry,
          id: match[1],
          date: stat.lastModified,
        });
      }

      entries.sort((a, b) => {
        return a.date - b.date;
      });

      return entries;
    })();
  },

  _getStore() {
    if (this._getStoreTask) {
      return this._getStoreTask;
    }

    return (this._getStoreTask = (async () => {
      try {
        if (!this._store) {
          await IOUtils.makeDirectory(this._storeDir, {
            permissions: 0o700,
          });

          let store = new CrashStore(
            this._storeDir,
            this._telemetryStoreSizeKey
          );
          await store.load();

          this._store = store;
          this._storeTimer = Cc["@mozilla.org/timer;1"].createInstance(
            Ci.nsITimer
          );
        }

        // The application can go long periods without interacting with the
        // store. Since the store takes up resources, we automatically "free"
        // the store after inactivity so resources can be returned to the
        // system. We do this via a timer and a mechanism that tracks when the
        // store is being accessed.
        this._storeTimer.cancel();

        // This callback frees resources from the store unless the store
        // is protected from freeing by some other process.
        let timerCB = () => {
          if (this._storeProtectedCount) {
            this._storeTimer.initWithCallback(
              timerCB,
              this.STORE_EXPIRATION_MS,
              this._storeTimer.TYPE_ONE_SHOT
            );
            return;
          }

          // We kill the reference that we hold. GC will kill it later. If
          // someone else holds a reference, that will prevent GC until that
          // reference is gone.
          this._store = null;
          this._storeTimer = null;
        };

        this._storeTimer.initWithCallback(
          timerCB,
          this.STORE_EXPIRATION_MS,
          this._storeTimer.TYPE_ONE_SHOT
        );

        return this._store;
      } finally {
        this._getStoreTask = null;
      }
    })());
  },

  /**
   * Obtain information about all known crashes.
   *
   * Returns an array of CrashRecord instances. Instances are read-only.
   */
  getCrashes() {
    return (async () => {
      let store = await this._getStore();

      return store.crashes;
    })();
  },

  getCrashCountsByDay() {
    return (async () => {
      let store = await this._getStore();

      return store._countsByDay;
    })();
  },
});

var gCrashManager;

/**
 * Interface to storage of crash data.
 *
 * This type handles storage of crash metadata. It exists as a separate type
 * from the crash manager for performance reasons: since all crash metadata
 * needs to be loaded into memory for access, we wish to easily dispose of all
 * associated memory when this data is no longer needed. Having an isolated
 * object whose references can easily be lost faciliates that simple disposal.
 *
 * When metadata is updated, the caller must explicitly persist the changes
 * to disk. This prevents excessive I/O during updates.
 *
 * The store has a mechanism for ensuring it doesn't grow too large. A ceiling
 * is placed on the number of daily events that can occur for events that can
 * occur with relatively high frequency. If we've reached
 * the high water mark and new data arrives, it's silently dropped.
 * However, the count of actual events is always preserved. This allows
 * us to report on the severity of problems beyond the storage threshold.
 *
 * Main process crashes are excluded from limits because they are both
 * important and should be rare.
 *
 * @param storeDir (string)
 *        Directory the store should be located in.
 * @param telemetrySizeKey (string)
 *        The telemetry histogram that should be used to store the size
 *        of the data file.
 */
function CrashStore(storeDir, telemetrySizeKey) {
  this._storeDir = storeDir;
  this._telemetrySizeKey = telemetrySizeKey;

  this._storePath = PathUtils.join(storeDir, "store.json.mozlz4");

  // Holds the read data from disk.
  this._data = null;

  // Maps days since UNIX epoch to a Map of event types to counts.
  // This data structure is populated when the JSON file is loaded
  // and is also updated when new events are added.
  this._countsByDay = new Map();
}

CrashStore.prototype = Object.freeze({
  // Maximum number of events to store per day. This establishes a
  // ceiling on the per-type/per-day records that will be stored.
  HIGH_WATER_DAILY_THRESHOLD: 500,

  /**
   * Reset all data.
   */
  reset() {
    this._data = {
      v: 1,
      crashes: new Map(),
      corruptDate: null,
    };
    this._countsByDay = new Map();
  },

  /**
   * Load data from disk.
   *
   * @return Promise
   */
  load() {
    return (async () => {
      // Loading replaces data.
      this.reset();

      try {
        let decoder = new TextDecoder();
        let data = await IOUtils.read(this._storePath, { decompress: true });
        data = JSON.parse(decoder.decode(data));

        if (data.corruptDate) {
          this._data.corruptDate = new Date(data.corruptDate);
        }

        // actualCounts is used to validate that the derived counts by
        // days stored in the payload matches up to actual data.
        let actualCounts = new Map();

        // In the past, submissions were stored as separate crash records
        // with an id of e.g. "someID-submission". If we find IDs ending
        // with "-submission", we will need to convert the data to be stored
        // as actual submissions.
        //
        // The old way of storing submissions was used from FF33 - FF34. We
        // drop this old data on the floor.
        for (let id in data.crashes) {
          if (id.endsWith("-submission")) {
            continue;
          }

          let crash = data.crashes[id];
          let denormalized = this._denormalize(crash);

          denormalized.submissions = new Map();
          if (crash.submissions) {
            for (let submissionID in crash.submissions) {
              let submission = crash.submissions[submissionID];
              denormalized.submissions.set(
                submissionID,
                this._denormalize(submission)
              );
            }
          }

          this._data.crashes.set(id, denormalized);

          let key =
            dateToDays(denormalized.crashDate) + "-" + denormalized.type;
          actualCounts.set(key, (actualCounts.get(key) || 0) + 1);

          // If we have an OOM size, count the crash as an OOM in addition to
          // being a main process crash.
          if (
            denormalized.metadata &&
            denormalized.metadata.OOMAllocationSize
          ) {
            let oomKey = key + "-oom";
            actualCounts.set(oomKey, (actualCounts.get(oomKey) || 0) + 1);
          }
        }

        // The validation in this loop is arguably not necessary. We perform
        // it as a defense against unknown bugs.
        for (let dayKey in data.countsByDay) {
          let day = parseInt(dayKey, 10);
          for (let type in data.countsByDay[day]) {
            this._ensureCountsForDay(day);

            let count = data.countsByDay[day][type];
            let key = day + "-" + type;

            // If the payload says we have data for a given day but we
            // don't, the payload is wrong. Ignore it.
            if (!actualCounts.has(key)) {
              continue;
            }

            // If we encountered more data in the payload than what the
            // data structure says, use the proper value.
            count = Math.max(count, actualCounts.get(key));

            this._countsByDay.get(day).set(type, count);
          }
        }
      } catch (ex) {
        // Missing files (first use) are allowed.
        if (!(ex instanceof DOMException) || ex.name != "NotFoundError") {
          // If we can't load for any reason, mark a corrupt date in the instance
          // and swallow the error.
          //
          // The marking of a corrupted file is intentionally not persisted to
          // disk yet. Instead, we wait until the next save(). This is to give
          // non-permanent failures the opportunity to recover on their own.
          this._data.corruptDate = new Date();
        }
      }
    })();
  },

  /**
   * Save data to disk.
   *
   * @return Promise<null>
   */
  save() {
    return (async () => {
      if (!this._data) {
        return;
      }

      let normalized = {
        // The version should be incremented whenever the format
        // changes.
        v: 1,
        // Maps crash IDs to objects defining the crash.
        crashes: {},
        // Maps days since UNIX epoch to objects mapping event types to
        // counts. This is a mirror of this._countsByDay. e.g.
        // {
        //    15000: {
        //        "main-crash": 2,
        //        "plugin-crash": 1
        //    }
        // }
        countsByDay: {},

        // When the store was last corrupted.
        corruptDate: null,
      };

      if (this._data.corruptDate) {
        normalized.corruptDate = this._data.corruptDate.getTime();
      }

      for (let [id, crash] of this._data.crashes) {
        let c = this._normalize(crash);

        c.submissions = {};
        for (let [submissionID, submission] of crash.submissions) {
          c.submissions[submissionID] = this._normalize(submission);
        }

        normalized.crashes[id] = c;
      }

      for (let [day, m] of this._countsByDay) {
        normalized.countsByDay[day] = {};
        for (let [type, count] of m) {
          normalized.countsByDay[day][type] = count;
        }
      }

      let encoder = new TextEncoder();
      let data = encoder.encode(JSON.stringify(normalized));
      let size = await IOUtils.write(this._storePath, data, {
        tmpPath: this._storePath + ".tmp",
        compress: true,
      });
      if (this._telemetrySizeKey) {
        Services.telemetry.getHistogramById(this._telemetrySizeKey).add(size);
      }
    })();
  },

  /**
   * Normalize an object into one fit for serialization.
   *
   * This function along with _denormalize() serve to hack around the
   * default handling of Date JSON serialization because Date serialization
   * is undefined by JSON.
   *
   * Fields ending with "Date" are assumed to contain Date instances.
   * We convert these to milliseconds since epoch on output and back to
   * Date on input.
   */
  _normalize(o) {
    let normalized = {};

    for (let k in o) {
      let v = o[k];
      if (v && k.endsWith("Date")) {
        normalized[k] = v.getTime();
      } else {
        normalized[k] = v;
      }
    }

    return normalized;
  },

  /**
   * Convert a serialized object back to its native form.
   */
  _denormalize(o) {
    let n = {};

    for (let k in o) {
      let v = o[k];
      if (v && k.endsWith("Date")) {
        n[k] = new Date(parseInt(v, 10));
      } else {
        n[k] = v;
      }
    }

    return n;
  },

  /**
   * Prune old crash data.
   *
   * Crashes without recent activity are pruned from the store so the
   * size of the store is not unbounded. If there is activity on a crash,
   * that activity will keep the crash and all its data around for longer.
   *
   * @param date
   *        (Date) The cutoff at which data will be pruned. If an entry
   *        doesn't have data newer than this, it will be pruned.
   */
  pruneOldCrashes(date) {
    for (let crash of this.crashes) {
      let newest = crash.newestDate;
      if (!newest || newest.getTime() < date.getTime()) {
        this._data.crashes.delete(crash.id);
      }
    }
  },

  /**
   * Date the store was last corrupted and required a reset.
   *
   * May be null (no corruption has ever occurred) or a Date instance.
   */
  get corruptDate() {
    return this._data.corruptDate;
  },

  /**
   * The number of distinct crashes tracked.
   */
  get crashesCount() {
    return this._data.crashes.size;
  },

  /**
   * All crashes tracked.
   *
   * This is an array of CrashRecord.
   */
  get crashes() {
    let crashes = [];
    for (let [, crash] of this._data.crashes) {
      crashes.push(new CrashRecord(crash));
    }

    return crashes;
  },

  /**
   * Obtain a particular crash from its ID.
   *
   * A CrashRecord will be returned if the crash exists. null will be returned
   * if the crash is unknown.
   */
  getCrash(id) {
    for (let crash of this.crashes) {
      if (crash.id == id) {
        return crash;
      }
    }

    return null;
  },

  _ensureCountsForDay(day) {
    if (!this._countsByDay.has(day)) {
      this._countsByDay.set(day, new Map());
    }
  },

  /**
   * Ensure the crash record is present in storage.
   *
   * Returns the crash record if we're allowed to store it or null
   * if we've hit the high water mark.
   *
   * @param processType
   *        (string) One of the PROCESS_TYPE constants.
   * @param crashType
   *        (string) One of the CRASH_TYPE constants.
   * @param id
   *        (string) The crash ID.
   * @param date
   *        (Date) When this crash occurred.
   * @param metadata
   *        (dictionary) Crash metadata, may be empty.
   *
   * @return null | object crash record
   */
  _ensureCrashRecord(processType, crashType, id, date, metadata) {
    if (!id) {
      // Crashes are keyed on ID, so it's not really helpful to store crashes
      // without IDs.
      return null;
    }

    let type = processType + "-" + crashType;

    if (!this._data.crashes.has(id)) {
      let day = dateToDays(date);
      this._ensureCountsForDay(day);

      let count = (this._countsByDay.get(day).get(type) || 0) + 1;
      this._countsByDay.get(day).set(type, count);

      if (
        count > this.HIGH_WATER_DAILY_THRESHOLD &&
        processType != CrashManager.prototype.PROCESS_TYPE_MAIN
      ) {
        return null;
      }

      // If we have an OOM size, count the crash as an OOM in addition to
      // being a main process crash.
      if (metadata && metadata.OOMAllocationSize) {
        let oomType = type + "-oom";
        let oomCount = (this._countsByDay.get(day).get(oomType) || 0) + 1;
        this._countsByDay.get(day).set(oomType, oomCount);
      }

      this._data.crashes.set(id, {
        id,
        remoteID: null,
        type,
        crashDate: date,
        submissions: new Map(),
        classifications: [],
        metadata,
      });
    }

    let crash = this._data.crashes.get(id);
    crash.type = type;
    crash.crashDate = date;

    return crash;
  },

  /**
   * Record the occurrence of a crash.
   *
   * @param processType (string) One of the PROCESS_TYPE constants.
   * @param crashType (string) One of the CRASH_TYPE constants.
   * @param id (string) Crash ID. Likely a UUID.
   * @param date (Date) When the crash occurred.
   * @param metadata (dictionary) Crash metadata, may be empty.
   *
   * @return boolean True if the crash was recorded and false if not.
   */
  addCrash(processType, crashType, id, date, metadata) {
    return !!this._ensureCrashRecord(
      processType,
      crashType,
      id,
      date,
      metadata
    );
  },

  /**
   * @return boolean True if the remote ID was recorded and false if not.
   */
  setRemoteCrashID(crashID, remoteID) {
    let crash = this._data.crashes.get(crashID);
    if (!crash || !remoteID) {
      return false;
    }

    crash.remoteID = remoteID;
    return true;
  },

  getCrashesOfType(processType, crashType) {
    let crashes = [];
    for (let crash of this.crashes) {
      if (crash.isOfType(processType, crashType)) {
        crashes.push(crash);
      }
    }

    return crashes;
  },

  /**
   * Ensure the submission record is present in storage.
   * @returns [submission, crash]
   */
  _ensureSubmissionRecord(crashID, submissionID) {
    let crash = this._data.crashes.get(crashID);
    if (!crash || !submissionID) {
      return null;
    }

    if (!crash.submissions.has(submissionID)) {
      crash.submissions.set(submissionID, {
        requestDate: null,
        responseDate: null,
        result: null,
      });
    }

    return [crash.submissions.get(submissionID), crash];
  },

  /**
   * @return boolean True if the attempt was recorded.
   */
  addSubmissionAttempt(crashID, submissionID, date) {
    let [submission, crash] = this._ensureSubmissionRecord(
      crashID,
      submissionID
    );
    if (!submission) {
      return false;
    }

    submission.requestDate = date;
    Services.telemetry
      .getKeyedHistogramById("PROCESS_CRASH_SUBMIT_ATTEMPT")
      .add(crash.type, 1);
    return true;
  },

  /**
   * @return boolean True if the response was recorded.
   */
  addSubmissionResult(crashID, submissionID, date, result) {
    let crash = this._data.crashes.get(crashID);
    if (!crash || !submissionID) {
      return false;
    }
    let submission = crash.submissions.get(submissionID);
    if (!submission) {
      return false;
    }

    submission.responseDate = date;
    submission.result = result;
    Services.telemetry
      .getKeyedHistogramById("PROCESS_CRASH_SUBMIT_SUCCESS")
      .add(crash.type, result == "ok");
    return true;
  },

  /**
   * @return boolean True if the classifications were set.
   */
  setCrashClassifications(crashID, classifications) {
    let crash = this._data.crashes.get(crashID);
    if (!crash) {
      return false;
    }

    crash.classifications = classifications;
    return true;
  },
});

/**
 * Represents an individual crash with metadata.
 *
 * This is a wrapper around the low-level anonymous JS objects that define
 * crashes. It exposes a consistent and helpful API.
 *
 * Instances of this type should only be constructured inside this module,
 * not externally. The constructor is not considered a public API.
 *
 * @param o (object)
 *        The crash's entry from the CrashStore.
 */
function CrashRecord(o) {
  this._o = o;
}

CrashRecord.prototype = Object.freeze({
  get id() {
    return this._o.id;
  },

  get remoteID() {
    return this._o.remoteID;
  },

  get crashDate() {
    return this._o.crashDate;
  },

  /**
   * Obtain the newest date in this record.
   *
   * This is a convenience getter. The returned value is used to determine when
   * to expire a record.
   */
  get newestDate() {
    // We currently only have 1 date, so this is easy.
    return this._o.crashDate;
  },

  get oldestDate() {
    return this._o.crashDate;
  },

  get type() {
    return this._o.type;
  },

  isOfType(processType, crashType) {
    return processType + "-" + crashType == this.type;
  },

  get submissions() {
    return this._o.submissions;
  },

  get classifications() {
    return this._o.classifications;
  },

  get metadata() {
    return this._o.metadata;
  },
});

XPCOMUtils.defineLazyGetter(CrashManager, "_log", () =>
  Log.repository.getLogger("Crashes.CrashManager")
);

/**
 * Obtain the global CrashManager instance used by the running application.
 *
 * CrashManager is likely only ever instantiated once per application lifetime.
 * The main reason it's implemented as a reusable type is to facilitate testing.
 */
XPCOMUtils.defineLazyGetter(CrashManager, "Singleton", function() {
  if (gCrashManager) {
    return gCrashManager;
  }

  gCrashManager = new CrashManager({
    telemetryStoreSizeKey: "CRASH_STORE_COMPRESSED_BYTES",
  });

  // Automatically aggregate event files shortly after startup. This
  // ensures it happens with some frequency.
  //
  // There are performance considerations here. While this is doing
  // work and could negatively impact performance, the amount of work
  // is kept small per run by periodically aggregating event files.
  // Furthermore, well-behaving installs should not have much work
  // here to do. If there is a lot of work, that install has bigger
  // issues beyond reduced performance near startup.
  gCrashManager.scheduleMaintenance(AGGREGATE_STARTUP_DELAY_MS);

  return gCrashManager;
});

function getCrashManager() {
  return CrashManager.Singleton;
}
