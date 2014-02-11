/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this)
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/Timer.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://services-common/utils.js", this);

this.EXPORTED_SYMBOLS = [
  "CrashManager",
];

/**
 * How long to wait after application startup before crash event files are
 * automatically aggregated.
 *
 * We defer aggregation for performance reasons, as we don't want too many
 * services competing for I/O immediately after startup.
 */
const AGGREGATE_STARTUP_DELAY_MS = 57000;

const MILLISECONDS_IN_DAY = 24 * 60 * 60 * 1000;


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
this.CrashManager = function (options) {
  for (let k of ["pendingDumpsDir", "submittedDumpsDir", "eventsDirs",
    "storeDir"]) {
    if (!(k in options)) {
      throw new Error("Required key not present in options: " + k);
    }
  }

  this._log = Log.repository.getLogger("Crashes.CrashManager");

  for (let k in options) {
    let v = options[k];

    switch (k) {
      case "pendingDumpsDir":
        this._pendingDumpsDir = v;
        break;

      case "submittedDumpsDir":
        this._submittedDumpsDir = v;
        break;

      case "eventsDirs":
        this._eventsDirs = v;
        break;

      case "storeDir":
        this._storeDir = v;
        break;

      case "telemetryStoreSizeKey":
        this._telemetryStoreSizeKey = v;
        break;

      default:
        throw new Error("Unknown property in options: " + k);
    }
  }

  // Promise for in-progress aggregation operation. We store it on the
  // object so it can be returned for in-progress operations.
  this._aggregatePromise = null;

  // The CrashStore currently attached to this object.
  this._store = null;

  // The timer controlling the expiration of the CrashStore instance.
  this._storeTimer = null;

  // This is a semaphore that prevents the store from being freed by our
  // timer-based resource freeing mechanism.
  this._storeProtectedCount = 0;
};

this.CrashManager.prototype = Object.freeze({
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
  // The type of event is unknown.
  EVENT_FILE_ERROR_UNKNOWN_EVENT: "unknown-event",

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
  pendingDumps: function () {
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
  submittedDumps: function () {
    return this._getDirectoryEntries(this._submittedDumpsDir,
                                     this.SUBMITTED_REGEX);
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
  aggregateEventsFiles: function () {
    if (this._aggregatePromise) {
      return this._aggregatePromise;
    }

    return this._aggregatePromise = Task.spawn(function* () {
      if (this._aggregatePromise) {
        return this._aggregatePromise;
      }

      try {
        let unprocessedFiles = yield this._getUnprocessedEventsFiles();

        let deletePaths = [];
        let needsSave = false;

        this._storeProtectedCount++;
        for (let entry of unprocessedFiles) {
          try {
            let result = yield this._processEventFile(entry);

            switch (result) {
              case this.EVENT_FILE_SUCCESS:
                needsSave = true;
                // Fall through.

              case this.EVENT_FILE_ERROR_MALFORMED:
                deletePaths.push(entry.path);
                break;

              case this.EVENT_FILE_ERROR_UNKNOWN_EVENT:
                break;

              default:
                Cu.reportError("Unhandled crash event file return code. Please " +
                               "file a bug: " + result);
            }
          } catch (ex if ex instanceof OS.File.Error) {
            this._log.warn("I/O error reading " + entry.path + ": " +
                           CommonUtils.exceptionStr(ex));
          } catch (ex) {
            // We should never encounter an exception. This likely represents
            // a coding error because all errors should be detected and
            // converted to return codes.
            //
            // If we get here, report the error and delete the source file
            // so we don't see it again.
            Cu.reportError("Exception when processing crash event file: " +
                           CommonUtils.exceptionStr(ex));
            deletePaths.push(entry.path);
          }
        }

        if (needsSave) {
          let store = yield this._getStore();
          yield store.save();
        }

        for (let path of deletePaths) {
          try {
            yield OS.File.remove(path);
          } catch (ex) {
            this._log.warn("Error removing event file (" + path + "): " +
                           CommonUtils.exceptionStr(ex));
          }
        }

        return unprocessedFiles.length;

      } finally {
        this._aggregatePromise = false;
        this._storeProtectedCount--;
      }
    }.bind(this));
  },

  /**
   * Prune old crash data.
   *
   * @param date
   *        (Date) The cutoff point for pruning. Crashes without data newer
   *        than this will be pruned.
   */
  pruneOldCrashes: function (date) {
    return Task.spawn(function* () {
      let store = yield this._getStore();
      store.pruneOldCrashes(date);
      yield store.save();
    }.bind(this));
  },

  /**
   * Run tasks that should be periodically performed.
   */
  runMaintenanceTasks: function () {
    return Task.spawn(function* () {
      yield this.aggregateEventsFiles();

      let offset = this.PURGE_OLDER_THAN_DAYS * MILLISECONDS_IN_DAY;
      yield this.pruneOldCrashes(new Date(Date.now() - offset));
    }.bind(this));
  },

  /**
   * Schedule maintenance tasks for some point in the future.
   *
   * @param delay
   *        (integer) Delay in milliseconds when maintenance should occur.
   */
  scheduleMaintenance: function (delay) {
    let deferred = Promise.defer();

    setTimeout(() => {
      this.runMaintenanceTasks().then(deferred.resolve, deferred.reject);
    }, delay);

    return deferred.promise;
  },

  /**
   * Obtain the paths of all unprocessed events files.
   *
   * The promise-resolved array is sorted by file mtime, oldest to newest.
   */
  _getUnprocessedEventsFiles: function () {
    return Task.spawn(function* () {
      let entries = [];

      for (let dir of this._eventsDirs) {
        for (let e of yield this._getDirectoryEntries(dir, this.ALL_REGEX)) {
          entries.push(e);
        }
      }

      entries.sort((a, b) => { return a.date - b.date; });

      return entries;
    }.bind(this));
  },

  // See docs/crash-events.rst for the file format specification.
  _processEventFile: function (entry) {
    return Task.spawn(function* () {
      let data = yield OS.File.read(entry.path);
      let store = yield this._getStore();

      let decoder = new TextDecoder();
      data = decoder.decode(data);

      let type, time, payload;
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
    }.bind(this));
  },

  _handleEventFilePayload: function (store, entry, type, date, payload) {
      // The payload types and formats are documented in docs/crash-events.rst.
      // Do not change the format of an existing type. Instead, invent a new
      // type.

      let eventMap = {
        "crash.main.1": "addMainProcessCrash",
        "crash.plugin.1": "addPluginCrash",
        "hang.plugin.1": "addPluginHang",
      };

      if (type in eventMap) {
        let lines = payload.split("\n");
        if (lines.length > 1) {
          this._log.warn("Multiple lines unexpected in payload for " +
                         entry.path);
          return this.EVENT_FILE_ERROR_MALFORMED;
        }

        store[eventMap[type]](payload, date);
        return this.EVENT_FILE_SUCCESS;
      }

      // DO NOT ADD NEW TYPES WITHOUT DOCUMENTING!

      return this.EVENT_FILE_ERROR_UNKNOWN_EVENT;
  },

  /**
   * The resolved promise is an array of objects with the properties:
   *
   *   path -- String filename
   *   id -- regexp.match()[1] (likely the crash ID)
   *   date -- Date mtime of the file
   */
  _getDirectoryEntries: function (path, re) {
    return Task.spawn(function* () {
      try {
        yield OS.File.stat(path);
      } catch (ex if ex instanceof OS.File.Error && ex.becauseNoSuchFile) {
          return [];
      }

      let it = new OS.File.DirectoryIterator(path);
      let entries = [];

      try {
        yield it.forEach((entry, index, it) => {
          if (entry.isDir) {
            return;
          }

          let match = re.exec(entry.name);
          if (!match) {
            return;
          }

          return OS.File.stat(entry.path).then((info) => {
            entries.push({
              path: entry.path,
              id: match[1],
              date: info.lastModificationDate,
            });
          });
        });
      } finally {
        it.close();
      }

      entries.sort((a, b) => { return a.date - b.date; });

      return entries;
    }.bind(this));
  },

  _getStore: function () {
    return Task.spawn(function* () {
      if (!this._store) {
        let store = new CrashStore(this._storeDir, this._telemetryStoreSizeKey);
        yield store.load();

        this._store = store;
        this._storeTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      }

      // The application can go long periods without interacting with the
      // store. Since the store takes up resources, we automatically "free"
      // the store after inactivity so resources can be returned to the system.
      // We do this via a timer and a mechanism that tracks when the store
      // is being accessed.
      this._storeTimer.cancel();

      // This callback frees resources from the store unless the store
      // is protected from freeing by some other process.
      let timerCB = function () {
        if (this._storeProtectedCount) {
          this._storeTimer.initWithCallback(timerCB, this.STORE_EXPIRATION_MS,
                                            this._storeTimer.TYPE_ONE_SHOT);
          return;
        }

        // We kill the reference that we hold. GC will kill it later. If
        // someone else holds a reference, that will prevent GC until that
        // reference is gone.
        this._store = null;
        this._storeTimer = null;
      }.bind(this);

      this._storeTimer.initWithCallback(timerCB, this.STORE_EXPIRATION_MS,
                                        this._storeTimer.TYPE_ONE_SHOT);

      return this._store;
    }.bind(this));
  },

  /**
   * Obtain information about all known crashes.
   *
   * Returns an array of CrashRecord instances. Instances are read-only.
   */
  getCrashes: function () {
    return Task.spawn(function* () {
      let store = yield this._getStore();

      return store.crashes;
    }.bind(this));
  },
});

let gCrashManager;

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
 * @param storeDir (string)
 *        Directory the store should be located in.
 * @param telemetrySizeKey (string)
 *        The telemetry histogram that should be used to store the size
 *        of the data file.
 */
function CrashStore(storeDir, telemetrySizeKey) {
  this._storeDir = storeDir;
  this._telemetrySizeKey = telemetrySizeKey;

  this._storePath = OS.Path.join(storeDir, "store.json.mozlz4");

  // Holds the read data from disk.
  this._data = null;
}

CrashStore.prototype = Object.freeze({
  TYPE_MAIN_CRASH: "main-crash",
  TYPE_PLUGIN_CRASH: "plugin-crash",
  TYPE_PLUGIN_HANG: "plugin-hang",

  /**
   * Load data from disk.
   *
   * @return Promise<null>
   */
  load: function () {
    return Task.spawn(function* () {
      this._data = {
        v: 1,
        crashes: new Map(),
        corruptDate: null,
      };

      try {
        let decoder = new TextDecoder();
        let data = yield OS.File.read(this._storePath, null, {compression: "lz4"});
        data = JSON.parse(decoder.decode(data));

        if (data.corruptDate) {
          this._data.corruptDate = new Date(data.corruptDate);
        }

        for (let id in data.crashes) {
          let crash = data.crashes[id];
          let denormalized = this._denormalize(crash);
          this._data.crashes.set(id, denormalized);
        }
      } catch (ex if ex instanceof OS.File.Error && ex.becauseNoSuchFile) {
        // Missing files (first use) are allowed.
      } catch (ex) {
        // If we can't load for any reason, mark a corrupt date in the instance
        // and swallow the error.
        //
        // The marking of a corrupted file is intentionally not persisted to
        // disk yet. Instead, we wait until the next save(). This is to give
        // non-permanent failures the opportunity to recover on their own.
        this._data.corruptDate = new Date();
      }
    }.bind(this));
  },

  /**
   * Save data to disk.
   *
   * @return Promise<null>
   */
  save: function () {
    return Task.spawn(function* () {
      if (!this._data) {
        return;
      }

      let normalized = {
        v: 1,
        crashes: {},
        corruptDate: null,
      };

      if (this._data.corruptDate) {
        normalized.corruptDate = this._data.corruptDate.getTime();
      }

      for (let [id, crash] of this._data.crashes) {
        let c = this._normalize(crash);
        normalized.crashes[id] = c;
      }

      let encoder = new TextEncoder();
      let data = encoder.encode(JSON.stringify(normalized));
      let size = yield OS.File.writeAtomic(this._storePath, data, {
                                           tmpPath: this._storePath + ".tmp",
                                           compression: "lz4"});
      if (this._telemetrySizeKey) {
        Services.telemetry.getHistogramById(this._telemetrySizeKey).add(size);
      }
    }.bind(this));
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
  _normalize: function (o) {
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
  _denormalize: function (o) {
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
  pruneOldCrashes: function (date) {
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
    for (let [id, crash] of this._data.crashes) {
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
  getCrash: function (id) {
    for (let crash of this.crashes) {
      if (crash.id == id) {
        return crash;
      }
    }

    return null;
  },

  _ensureCrashRecord: function (id) {
    if (!this._data.crashes.has(id)) {
      this._data.crashes.set(id, {
        id: id,
        type: null,
        crashDate: null,
      });
    }

    return this._data.crashes.get(id);
  },

  /**
   * Record the occurrence of a crash in the main process.
   *
   * @param id (string) Crash ID. Likely a UUID.
   * @param date (Date) When the crash occurred.
   */
  addMainProcessCrash: function (id, date) {
    let r = this._ensureCrashRecord(id);
    r.type = this.TYPE_MAIN_CRASH;
    r.crashDate = date;
  },

  /**
   * Record the occurrence of a crash in a plugin process.
   *
   * @param id (string) Crash ID. Likely a UUID.
   * @param date (Date) When the crash occurred.
   */
  addPluginCrash: function (id, date) {
    let r = this._ensureCrashRecord(id);
    r.type = this.TYPE_PLUGIN_CRASH;
    r.crashDate = date;
  },

  /**
   * Record the occurrence of a hang in a plugin process.
   *
   * @param id (string) Crash ID. Likely a UUID.
   * @param date (Date) When the hang was reported.
   */
  addPluginHang: function (id, date) {
    let r = this._ensureCrashRecord(id);
    r.type = this.TYPE_PLUGIN_HANG;
    r.crashDate = date;
  },

  get mainProcessCrashes() {
    let crashes = [];
    for (let crash of this.crashes) {
      if (crash.isMainProcessCrash) {
        crashes.push(crash);
      }
    }

    return crashes;
  },

  get pluginCrashes() {
    let crashes = [];
    for (let crash of this.crashes) {
      if (crash.isPluginCrash) {
        crashes.push(crash);
      }
    }

    return crashes;
  },

  get pluginHangs() {
    let crashes = [];
    for (let crash of this.crashes) {
      if (crash.isPluginHang) {
        crashes.push(crash);
      }
    }

    return crashes;
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

  get type() {
    return this._o.type;
  },

  get isMainProcessCrash() {
    return this._o.type == CrashStore.prototype.TYPE_MAIN_CRASH;
  },

  get isPluginCrash() {
    return this._o.type == CrashStore.prototype.TYPE_PLUGIN_CRASH;
  },

  get isPluginHang() {
    return this._o.type == CrashStore.prototype.TYPE_PLUGIN_HANG;
  },
});

/**
 * Obtain the global CrashManager instance used by the running application.
 *
 * CrashManager is likely only ever instantiated once per application lifetime.
 * The main reason it's implemented as a reusable type is to facilitate testing.
 */
XPCOMUtils.defineLazyGetter(this.CrashManager, "Singleton", function () {
  if (gCrashManager) {
    return gCrashManager;
  }

  let crPath = OS.Path.join(OS.Constants.Path.userApplicationDataDir,
                            "Crash Reports");
  let storePath = OS.Path.join(OS.Constants.Path.profileDir, "crashes");

  gCrashManager = new CrashManager({
    pendingDumpsDir: OS.Path.join(crPath, "pending"),
    submittedDumpsDir: OS.Path.join(crPath, "submitted"),
    eventsDirs: [OS.Path.join(crPath, "events"), OS.Path.join(storePath, "events")],
    storeDir: storePath,
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
