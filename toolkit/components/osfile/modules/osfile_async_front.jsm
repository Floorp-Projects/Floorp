/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Asynchronous front-end for OS.File.
 *
 * This front-end is meant to be imported from the main thread. In turn,
 * it spawns one worker (perhaps more in the future) and delegates all
 * disk I/O to this worker.
 *
 * Documentation note: most of the functions and methods in this module
 * return promises. For clarity, we denote as follows a promise that may resolve
 * with type |A| and some value |value| or reject with type |B| and some
 * reason |reason|
 * @resolves {A} value
 * @rejects {B} reason
 */

"use strict";

// Scheduler is exported for test-only usage.
var EXPORTED_SYMBOLS = ["OS", "Scheduler"];

var SharedAll = ChromeUtils.import(
  "resource://gre/modules/osfile/osfile_shared_allthreads.jsm"
);
const { clearInterval, setInterval } = ChromeUtils.import(
  "resource://gre/modules/Timer.jsm"
);

// Boilerplate, to simplify the transition to require()
var LOG = SharedAll.LOG.bind(SharedAll, "Controller");
var isTypedArray = SharedAll.isTypedArray;

// The constructor for file errors.
var SysAll;
if (SharedAll.Constants.Win) {
  SysAll = ChromeUtils.import(
    "resource://gre/modules/osfile/osfile_win_allthreads.jsm"
  );
} else if (SharedAll.Constants.libc) {
  SysAll = ChromeUtils.import(
    "resource://gre/modules/osfile/osfile_unix_allthreads.jsm"
  );
} else {
  throw new Error("I am neither under Windows nor under a Posix system");
}
var OSError = SysAll.Error;
var Type = SysAll.Type;

var Path = ChromeUtils.import("resource://gre/modules/osfile/ospath.jsm");

const lazy = {};

// The library of promises.
ChromeUtils.defineModuleGetter(
  lazy,
  "PromiseUtils",
  "resource://gre/modules/PromiseUtils.jsm"
);

// The implementation of communications
const { BasePromiseWorker } = ChromeUtils.import(
  "resource://gre/modules/PromiseWorker.jsm"
);
const { AsyncShutdown } = ChromeUtils.import(
  "resource://gre/modules/AsyncShutdown.jsm"
);
var Native = ChromeUtils.import(
  "resource://gre/modules/osfile/osfile_native.jsm"
);

// It's possible for osfile.jsm to get imported before the profile is
// set up. In this case, some path constants aren't yet available.
// Here, we make them lazy loaders.

function lazyPathGetter(constProp, dirKey) {
  return function() {
    let path;
    try {
      path = Services.dirsvc.get(dirKey, Ci.nsIFile).path;
      delete SharedAll.Constants.Path[constProp];
      SharedAll.Constants.Path[constProp] = path;
    } catch (ex) {
      // Ignore errors if the value still isn't available. Hopefully
      // the next access will return it.
    }

    return path;
  };
}

for (let [constProp, dirKey] of [
  ["localProfileDir", "ProfLD"],
  ["profileDir", "ProfD"],
  ["userApplicationDataDir", "UAppData"],
  ["winAppDataDir", "AppData"],
  ["winLocalAppDataDir", "LocalAppData"],
  ["winStartMenuProgsDir", "Progs"],
  ["tmpDir", "TmpD"],
  ["homeDir", "Home"],
  ["macUserLibDir", "ULibDir"],
]) {
  if (constProp in SharedAll.Constants.Path) {
    continue;
  }

  LOG(
    "Installing lazy getter for OS.Constants.Path." +
      constProp +
      " because it isn't defined and profile may not be loaded."
  );
  Object.defineProperty(SharedAll.Constants.Path, constProp, {
    get: lazyPathGetter(constProp, dirKey),
  });
}

/**
 * Return a shallow clone of the enumerable properties of an object.
 */
var clone = SharedAll.clone;

/**
 * Extract a shortened version of an object, fit for logging.
 *
 * This function returns a copy of the original object in which all
 * long strings, Arrays, TypedArrays, ArrayBuffers are removed and
 * replaced with placeholders. Use this function to sanitize objects
 * if you wish to log them or to keep them in memory.
 *
 * @param {*} obj The obj to shorten.
 * @return {*} array A shorter object, fit for logging.
 */
function summarizeObject(obj) {
  if (!obj) {
    return null;
  }
  if (typeof obj == "string") {
    if (obj.length > 1024) {
      return { "Long string": obj.length };
    }
    return obj;
  }
  if (typeof obj == "object") {
    if (Array.isArray(obj)) {
      if (obj.length > 32) {
        return { "Long array": obj.length };
      }
      return obj.map(summarizeObject);
    }
    if ("byteLength" in obj) {
      // Assume TypedArray or ArrayBuffer
      return { "Binary Data": obj.byteLength };
    }
    let result = {};
    for (let k of Object.keys(obj)) {
      result[k] = summarizeObject(obj[k]);
    }
    return result;
  }
  return obj;
}

var Scheduler = {
  /**
   * |true| once we have sent at least one message to the worker.
   * This field is unaffected by resetting the worker.
   */
  launched: false,

  /**
   * |true| once shutdown has begun i.e. we should reject any
   * message, including resets.
   */
  shutdown: false,

  /**
   * A promise resolved once all currently pending operations are complete.
   *
   * This promise is never rejected and the result is always undefined.
   */
  queue: Promise.resolve(),

  /**
   * A promise resolved once all currently pending `kill` operations
   * are complete.
   *
   * This promise is never rejected and the result is always undefined.
   */
  _killQueue: Promise.resolve(),

  /**
   * Miscellaneous debugging information
   */
  Debugging: {
    /**
     * The latest message sent and still waiting for a reply.
     */
    latestSent: undefined,

    /**
     * The latest reply received, or null if we are waiting for a reply.
     */
    latestReceived: undefined,

    /**
     * Number of messages sent to the worker. This includes the
     * initial SET_DEBUG, if applicable.
     */
    messagesSent: 0,

    /**
     * Total number of messages ever queued, including the messages
     * sent.
     */
    messagesQueued: 0,

    /**
     * Number of messages received from the worker.
     */
    messagesReceived: 0,
  },

  /**
   * A timer used to automatically shut down the worker after some time.
   */
  resetTimer: null,

  /**
   * A flag indicating whether we had some activities when waiting the
   * timer and if it's not we can shut down the worker.
   */
  hasRecentActivity: false,

  /**
   * The worker to which to send requests.
   *
   * If the worker has never been created or has been reset, this is a
   * fresh worker, initialized with osfile_async_worker.js.
   *
   * @type {PromiseWorker}
   */
  get worker() {
    if (!this._worker) {
      // Either the worker has never been created or it has been
      // reset.  In either case, it is time to instantiate the worker.
      this._worker = new BasePromiseWorker(
        "resource://gre/modules/osfile/osfile_async_worker.js"
      );
      this._worker.log = LOG;
      this._worker.ExceptionHandlers["OS.File.Error"] = OSError.fromMsg;

      let delay = Services.prefs.getIntPref("osfile.reset_worker_delay", 0);
      if (delay) {
        this.resetTimer = setInterval(() => {
          if (this.hasRecentActivity) {
            this.hasRecentActivity = false;
            return;
          }
          clearInterval(this.resetTimer);
          Scheduler.kill({ reset: true, shutdown: false });
        }, delay);
      }
    }
    return this._worker;
  },

  _worker: null,

  /**
   * Restart the OS.File worker killer timer.
   */
  restartTimer(arg) {
    this.hasRecentActivity = true;
  },

  /**
   * Shutdown OS.File.
   *
   * @param {*} options
   * - {boolean} shutdown If |true|, reject any further request. Otherwise,
   *   further requests will resurrect the worker.
   * - {boolean} reset If |true|, instruct the worker to shutdown if this
   *   would not cause leaks. Otherwise, assume that the worker will be shutdown
   *   through some other mean.
   */
  kill({ shutdown, reset }) {
    // Grab the kill queue to make sure that we
    // cannot be interrupted by another call to `kill`.
    let killQueue = this._killQueue;

    // Deactivate the queue, to ensure that no message is sent
    // to an obsolete worker (we reactivate it in the `finally`).
    // This needs to be done right now so that we maintain relative
    // ordering with calls to post(), etc.
    let deferred = lazy.PromiseUtils.defer();
    let savedQueue = this.queue;
    this.queue = deferred.promise;

    return (this._killQueue = (async () => {
      await killQueue;
      // From this point, and until the end of the Task, we are the
      // only call to `kill`, regardless of any `yield`.

      await savedQueue;

      try {
        // Enter critical section: no yield in this block
        // (we want to make sure that we remain the only
        // request in the queue).

        if (!this.launched || this.shutdown || !this._worker) {
          // Nothing to kill
          this.shutdown = this.shutdown || shutdown;
          this._worker = null;
          return null;
        }

        // Exit critical section

        let message = ["Meta_shutdown", [reset]];

        Scheduler.latestReceived = [];
        let stack = new Error().stack;
        Scheduler.latestSent = [Date.now(), stack, ...message];

        // Wait for result
        let resources;
        try {
          resources = await this._worker.post(...message);

          Scheduler.latestReceived = [Date.now(), message];
        } catch (ex) {
          LOG("Could not dispatch Meta_reset", ex);
          // It's most likely a programmer error, but we'll assume that
          // the worker has been shutdown, as it's less risky than the
          // opposite stance.
          resources = {
            openedFiles: [],
            openedDirectoryIterators: [],
            killed: true,
          };

          Scheduler.latestReceived = [Date.now(), message, ex];
        }

        let { openedFiles, openedDirectoryIterators, killed } = resources;
        if (
          !reset &&
          ((openedFiles && openedFiles.length) ||
            (openedDirectoryIterators && openedDirectoryIterators.length))
        ) {
          // The worker still holds resources. Report them.

          let msg = "";
          if (openedFiles.length) {
            msg +=
              "The following files are still open:\n" + openedFiles.join("\n");
          }
          if (openedDirectoryIterators.length) {
            msg +=
              "The following directory iterators are still open:\n" +
              openedDirectoryIterators.join("\n");
          }

          LOG("WARNING: File descriptors leaks detected.\n" + msg);
        }

        // Make sure that we do not leave an invalid |worker| around.
        if (killed || shutdown) {
          this._worker = null;
        }

        this.shutdown = shutdown;

        return resources;
      } finally {
        // Resume accepting messages. If we have set |shutdown| to |true|,
        // any pending/future request will be rejected. Otherwise, any
        // pending/future request will spawn a new worker if necessary.
        deferred.resolve();
      }
    })());
  },

  /**
   * Push a task at the end of the queue.
   *
   * @param {function} code A function returning a Promise.
   * This function will be executed once all the previously
   * pushed tasks have completed.
   * @return {Promise} A promise with the same behavior as
   * the promise returned by |code|.
   */
  push(code) {
    let promise = this.queue.then(code);
    // By definition, |this.queue| can never reject.
    this.queue = promise.catch(() => undefined);
    // Fork |promise| to ensure that uncaught errors are reported
    return promise.then();
  },

  /**
   * Post a message to the worker thread.
   *
   * @param {string} method The name of the method to call.
   * @param {...} args The arguments to pass to the method. These arguments
   * must be clonable.
   * The last argument by convention may be an object `options`, with some of
   * the following fields:
   *   - {number|null} outSerializationDuration A parameter to be filled with
   *     duration of the `this.worker.post` method.
   * @return {Promise} A promise conveying the result/error caused by
   * calling |method| with arguments |args|.
   */
  post: function post(method, args = undefined, closure = undefined) {
    if (this.shutdown) {
      LOG(
        "OS.File is not available anymore. The following request has been rejected.",
        method,
        args
      );
      return Promise.reject(
        new Error("OS.File has been shut down. Rejecting post to " + method)
      );
    }
    let firstLaunch = !this.launched;
    this.launched = true;

    if (firstLaunch && SharedAll.Config.DEBUG) {
      // If we have delayed sending SET_DEBUG, do it now.
      this.worker.post("SET_DEBUG", [true]);
      Scheduler.Debugging.messagesSent++;
    }

    Scheduler.Debugging.messagesQueued++;
    return this.push(async () => {
      if (this.shutdown) {
        LOG(
          "OS.File is not available anymore. The following request has been rejected.",
          method,
          args
        );
        throw new Error(
          "OS.File has been shut down. Rejecting request to " + method
        );
      }

      // Update debugging information. As |args| may be quite
      // expensive, we only keep a shortened version of it.
      Scheduler.Debugging.latestReceived = null;
      Scheduler.Debugging.latestSent = [
        Date.now(),
        method,
        summarizeObject(args),
      ];

      // Don't kill the worker just yet
      Scheduler.restartTimer();

      // The last object inside the args may be an options object.
      let options = null;
      if (
        args &&
        args.length >= 1 &&
        typeof args[args.length - 1] === "object"
      ) {
        options = args[args.length - 1];
      }

      let reply;
      try {
        try {
          Scheduler.Debugging.messagesSent++;
          Scheduler.Debugging.latestSent = Scheduler.Debugging.latestSent.slice(
            0,
            2
          );
          let serializationStartTimeMs = Date.now();
          reply = await this.worker.post(method, args, closure);
          let serializationEndTimeMs = Date.now();
          Scheduler.Debugging.latestReceived = [
            Date.now(),
            summarizeObject(reply),
          ];

          // There were no options for recording the serialization duration.
          if (options && "outSerializationDuration" in options) {
            // The difference might be negative for very fast operations, since Date.now() may not be monotonic.
            let serializationDurationMs = Math.max(
              0,
              serializationEndTimeMs - serializationStartTimeMs
            );

            if (typeof options.outSerializationDuration === "number") {
              options.outSerializationDuration += serializationDurationMs;
            } else {
              options.outSerializationDuration = serializationDurationMs;
            }
          }
          return reply;
        } finally {
          Scheduler.Debugging.messagesReceived++;
        }
      } catch (error) {
        Scheduler.Debugging.latestReceived = [
          Date.now(),
          error.message,
          error.fileName,
          error.lineNumber,
        ];
        throw error;
      } finally {
        if (firstLaunch) {
          Scheduler._updateTelemetry();
        }
        Scheduler.restartTimer();
      }
    });
  },

  /**
   * Post Telemetry statistics.
   *
   * This is only useful on first launch.
   */
  _updateTelemetry() {
    let worker = this.worker;
    let workerTimeStamps = worker.workerTimeStamps;
    if (!workerTimeStamps) {
      // If the first call to OS.File results in an uncaught errors,
      // the timestamps are absent. As this case is a developer error,
      // let's not waste time attempting to extract telemetry from it.
      return;
    }
    let HISTOGRAM_LAUNCH = Services.telemetry.getHistogramById(
      "OSFILE_WORKER_LAUNCH_MS"
    );
    HISTOGRAM_LAUNCH.add(
      worker.workerTimeStamps.entered - worker.launchTimeStamp
    );

    let HISTOGRAM_READY = Services.telemetry.getHistogramById(
      "OSFILE_WORKER_READY_MS"
    );
    HISTOGRAM_READY.add(
      worker.workerTimeStamps.loaded - worker.launchTimeStamp
    );
  },
};

const PREF_OSFILE_LOG = "toolkit.osfile.log";
const PREF_OSFILE_LOG_REDIRECT = "toolkit.osfile.log.redirect";

/**
 * Safely read a PREF_OSFILE_LOG preference.
 * Returns a value read or, in case of an error, oldPref or false.
 *
 * @param bool oldPref
 *        An optional value that the DEBUG flag was set to previously.
 */
function readDebugPref(prefName, oldPref = false) {
  // If neither pref nor oldPref were set, default it to false.
  return Services.prefs.getBoolPref(prefName, oldPref);
}

/**
 * Listen to PREF_OSFILE_LOG changes and update gShouldLog flag
 * appropriately.
 */
Services.prefs.addObserver(PREF_OSFILE_LOG, function prefObserver(
  aSubject,
  aTopic,
  aData
) {
  SharedAll.Config.DEBUG = readDebugPref(
    PREF_OSFILE_LOG,
    SharedAll.Config.DEBUG
  );
  if (Scheduler.launched) {
    // Don't start the worker just to set this preference.
    Scheduler.post("SET_DEBUG", [SharedAll.Config.DEBUG]);
  }
});
SharedAll.Config.DEBUG = readDebugPref(PREF_OSFILE_LOG, false);

Services.prefs.addObserver(PREF_OSFILE_LOG_REDIRECT, function prefObserver(
  aSubject,
  aTopic,
  aData
) {
  SharedAll.Config.TEST = readDebugPref(
    PREF_OSFILE_LOG_REDIRECT,
    OS.Shared.TEST
  );
});
SharedAll.Config.TEST = readDebugPref(PREF_OSFILE_LOG_REDIRECT, false);

/**
 * If |true|, use the native implementaiton of OS.File methods
 * whenever possible. Otherwise, force the use of the JS version.
 */
var nativeWheneverAvailable = true;
const PREF_OSFILE_NATIVE = "toolkit.osfile.native";
Services.prefs.addObserver(PREF_OSFILE_NATIVE, function prefObserver(
  aSubject,
  aTopic,
  aData
) {
  nativeWheneverAvailable = readDebugPref(
    PREF_OSFILE_NATIVE,
    nativeWheneverAvailable
  );
});

// Update worker's DEBUG flag if it's true.
// Don't start the worker just for this, though.
if (SharedAll.Config.DEBUG && Scheduler.launched) {
  Scheduler.post("SET_DEBUG", [true]);
}

// Preference used to configure test shutdown observer.
const PREF_OSFILE_TEST_SHUTDOWN_OBSERVER =
  "toolkit.osfile.test.shutdown.observer";

AsyncShutdown.xpcomWillShutdown.addBlocker(
  "OS.File: flush pending requests, warn about unclosed files, shut down service.",
  async function() {
    // Give clients a last chance to enqueue requests.
    await Barriers.shutdown.wait({ crashAfterMS: null });

    // Wait until all requests are complete and kill the worker.
    await Scheduler.kill({ reset: false, shutdown: true });
  },
  () => {
    let details = Barriers.getDetails();
    details.clients = Barriers.shutdown.state;
    return details;
  }
);

// Attaching an observer for PREF_OSFILE_TEST_SHUTDOWN_OBSERVER to enable or
// disable the test shutdown event observer.
// Note: By default the PREF_OSFILE_TEST_SHUTDOWN_OBSERVER is unset.
// Note: This is meant to be used for testing purposes only.
Services.prefs.addObserver(
  PREF_OSFILE_TEST_SHUTDOWN_OBSERVER,
  function prefObserver() {
    // The temporary phase topic used to trigger the unclosed
    // phase warning.
    let TOPIC = Services.prefs.getCharPref(
      PREF_OSFILE_TEST_SHUTDOWN_OBSERVER,
      ""
    );
    if (TOPIC) {
      // Generate a phase, add a blocker.
      // Note that this can work only if AsyncShutdown itself has been
      // configured for testing by the testsuite.
      let phase = AsyncShutdown._getPhase(TOPIC);
      phase.addBlocker(
        "(for testing purposes) OS.File: warn about unclosed files",
        () => Scheduler.kill({ shutdown: false, reset: false })
      );
    }
  }
);

/**
 * Representation of a file, with asynchronous methods.
 *
 * @param {*} fdmsg The _message_ representing the platform-specific file
 * handle.
 *
 * @constructor
 */
var File = function File(fdmsg) {
  // FIXME: At the moment, |File| does not close on finalize
  // (see bug 777715)
  this._fdmsg = fdmsg;
  this._closeResult = null;
  this._closed = null;
};

File.prototype = {
  /**
   * Close a file asynchronously.
   *
   * This method is idempotent.
   *
   * @return {promise}
   * @resolves {null}
   * @rejects {OS.File.Error}
   */
  close: function close() {
    if (this._fdmsg != null) {
      let msg = this._fdmsg;
      this._fdmsg = null;
      return (this._closeResult = Scheduler.post(
        "File_prototype_close",
        [msg],
        this
      ));
    }
    return this._closeResult;
  },

  /**
   * Fetch information about the file.
   *
   * @return {promise}
   * @resolves {OS.File.Info} The latest information about the file.
   * @rejects {OS.File.Error}
   */
  stat: function stat() {
    return Scheduler.post("File_prototype_stat", [this._fdmsg], this).then(
      File.Info.fromMsg
    );
  },

  /**
   * Write bytes from a buffer to this file.
   *
   * Note that, by default, this function may perform several I/O
   * operations to ensure that the buffer is fully written.
   *
   * @param {Typed array | C pointer} buffer The buffer in which the
   * the bytes are stored. The buffer must be large enough to
   * accomodate |bytes| bytes. Using the buffer before the operation
   * is complete is a BAD IDEA.
   * @param {*=} options Optionally, an object that may contain the
   * following fields:
   * - {number} bytes The number of |bytes| to write from the buffer. If
   * unspecified, this is |buffer.byteLength|. Note that |bytes| is required
   * if |buffer| is a C pointer.
   *
   * @return {number} The number of bytes actually written.
   */
  write: function write(buffer, options = {}) {
    // If |buffer| is a typed array and there is no |bytes| options,
    // we need to extract the |byteLength| now, as it will be lost
    // by communication.
    // Options might be a nullish value, so better check for that before using
    // the |in| operator.
    if (isTypedArray(buffer) && !(options && "bytes" in options)) {
      // Preserve reference to option |outExecutionDuration|, |outSerializationDuration|, if it is passed.
      options = clone(options, [
        "outExecutionDuration",
        "outSerializationDuration",
      ]);
      options.bytes = buffer.byteLength;
    }
    return Scheduler.post(
      "File_prototype_write",
      [this._fdmsg, Type.void_t.in_ptr.toMsg(buffer), options],
      buffer /* Ensure that |buffer| is not gc-ed*/
    );
  },

  /**
   * Read bytes from this file to a new buffer.
   *
   * @param {number=} bytes If unspecified, read all the remaining bytes from
   * this file. If specified, read |bytes| bytes, or less if the file does not
   * contain that many bytes.
   * @param {JSON} options
   * @return {promise}
   * @resolves {Uint8Array} An array containing the bytes read.
   */
  read: function read(nbytes, options = {}) {
    let promise = Scheduler.post("File_prototype_read", [
      this._fdmsg,
      nbytes,
      options,
    ]);
    return promise.then(function onSuccess(data) {
      return new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
    });
  },

  /**
   * Return the current position in the file, as bytes.
   *
   * @return {promise}
   * @resolves {number} The current position in the file,
   * as a number of bytes since the start of the file.
   */
  getPosition: function getPosition() {
    return Scheduler.post("File_prototype_getPosition", [this._fdmsg]);
  },

  /**
   * Set the current position in the file, as bytes.
   *
   * @param {number} pos A number of bytes.
   * @param {number} whence The reference position in the file,
   * which may be either POS_START (from the start of the file),
   * POS_END (from the end of the file) or POS_CUR (from the
   * current position in the file).
   *
   * @return {promise}
   */
  setPosition: function setPosition(pos, whence) {
    return Scheduler.post("File_prototype_setPosition", [
      this._fdmsg,
      pos,
      whence,
    ]);
  },

  /**
   * Flushes the file's buffers and causes all buffered data
   * to be written.
   * Disk flushes are very expensive and therefore should be used carefully,
   * sparingly and only in scenarios where it is vital that data survives
   * system crashes. Even though the function will be executed off the
   * main-thread, it might still affect the overall performance of any running
   * application.
   *
   * @return {promise}
   */
  flush: function flush() {
    return Scheduler.post("File_prototype_flush", [this._fdmsg]);
  },

  /**
   * Set the file's access permissions.  This does nothing on Windows.
   *
   * This operation is likely to fail if applied to a file that was
   * not created by the currently running program (more precisely,
   * if it was created by a program running under a different OS-level
   * user account).  It may also fail, or silently do nothing, if the
   * filesystem containing the file does not support access permissions.
   *
   * @param {*=} options Object specifying the requested permissions:
   *
   * - {number} unixMode The POSIX file mode to set on the file.  If omitted,
   *  the POSIX file mode is reset to the default used by |OS.file.open|.  If
   *  specified, the permissions will respect the process umask as if they
   *  had been specified as arguments of |OS.File.open|, unless the
   *  |unixHonorUmask| parameter tells otherwise.
   * - {bool} unixHonorUmask If omitted or true, any |unixMode| value is
   *  modified by the process umask, as |OS.File.open| would have done.  If
   *  false, the exact value of |unixMode| will be applied.
   */
  setPermissions: function setPermissions(options = {}) {
    return Scheduler.post("File_prototype_setPermissions", [
      this._fdmsg,
      options,
    ]);
  },
};

if (SharedAll.Constants.Sys.Name != "Android") {
  /**
   * Set the last access and modification date of the file.
   * The time stamp resolution is 1 second at best, but might be worse
   * depending on the platform.
   *
   * WARNING: This method is not implemented on Android/B2G. On Android/B2G,
   * you should use File.setDates instead.
   *
   * @return {promise}
   * @rejects {TypeError}
   * @rejects {OS.File.Error}
   */
  File.prototype.setDates = function(accessDate, modificationDate) {
    return Scheduler.post(
      "File_prototype_setDates",
      [this._fdmsg, accessDate, modificationDate],
      this
    );
  };
}

/**
 * Open a file asynchronously.
 *
 * @return {promise}
 * @resolves {OS.File}
 * @rejects {OS.Error}
 */
File.open = function open(path, mode, options) {
  return Scheduler.post(
    "open",
    [Type.path.toMsg(path), mode, options],
    path
  ).then(function onSuccess(msg) {
    return new File(msg);
  });
};

/**
 * Creates and opens a file with a unique name. By default, generate a random HEX number and use it to create a unique new file name.
 *
 * @param {string} path The path to the file.
 * @param {*=} options Additional options for file opening. This
 * implementation interprets the following fields:
 *
 * - {number} humanReadable If |true|, create a new filename appending a decimal number. ie: filename-1.ext, filename-2.ext.
 *  If |false| use HEX numbers ie: filename-A65BC0.ext
 * - {number} maxReadableNumber Used to limit the amount of tries after a failed
 *  file creation. Default is 20.
 *
 * @return {Object} contains A file object{file} and the path{path}.
 * @throws {OS.File.Error} If the file could not be opened.
 */
File.openUnique = function openUnique(path, options) {
  return Scheduler.post(
    "openUnique",
    [Type.path.toMsg(path), options],
    path
  ).then(function onSuccess(msg) {
    return {
      path: msg.path,
      file: new File(msg.file),
    };
  });
};

/**
 * Get the information on the file.
 *
 * @return {promise}
 * @resolves {OS.File.Info}
 * @rejects {OS.Error}
 */
File.stat = function stat(path, options) {
  return Scheduler.post("stat", [Type.path.toMsg(path), options], path).then(
    File.Info.fromMsg
  );
};

/**
 * Set the last access and modification date of the file.
 * The time stamp resolution is 1 second at best, but might be worse
 * depending on the platform.
 *
 * @return {promise}
 * @rejects {TypeError}
 * @rejects {OS.File.Error}
 */
File.setDates = function setDates(path, accessDate, modificationDate) {
  return Scheduler.post(
    "setDates",
    [Type.path.toMsg(path), accessDate, modificationDate],
    this
  );
};

/**
 * Set the file's access permissions.  This does nothing on Windows.
 *
 * This operation is likely to fail if applied to a file that was
 * not created by the currently running program (more precisely,
 * if it was created by a program running under a different OS-level
 * user account).  It may also fail, or silently do nothing, if the
 * filesystem containing the file does not support access permissions.
 *
 * @param {string} path The path to the file.
 * @param {*=} options Object specifying the requested permissions:
 *
 * - {number} unixMode The POSIX file mode to set on the file.  If omitted,
 *  the POSIX file mode is reset to the default used by |OS.file.open|.  If
 *  specified, the permissions will respect the process umask as if they
 *  had been specified as arguments of |OS.File.open|, unless the
 *  |unixHonorUmask| parameter tells otherwise.
 * - {bool} unixHonorUmask If omitted or true, any |unixMode| value is
 *  modified by the process umask, as |OS.File.open| would have done.  If
 *  false, the exact value of |unixMode| will be applied.
 */
File.setPermissions = function setPermissions(path, options = {}) {
  return Scheduler.post("setPermissions", [Type.path.toMsg(path), options]);
};

/**
 * Fetch the current directory
 *
 * @return {promise}
 * @resolves {string} The current directory, as a path usable with OS.Path
 * @rejects {OS.Error}
 */
File.getCurrentDirectory = function getCurrentDirectory() {
  return Scheduler.post("getCurrentDirectory").then(Type.path.fromMsg);
};

/**
 * Change the current directory
 *
 * @param {string} path The OS-specific path to the current directory.
 * You should use the methods of OS.Path and the constants of OS.Constants.Path
 * to build OS-specific paths in a portable manner.
 *
 * @return {promise}
 * @resolves {null}
 * @rejects {OS.Error}
 */
File.setCurrentDirectory = function setCurrentDirectory(path) {
  return Scheduler.post("setCurrentDirectory", [Type.path.toMsg(path)], path);
};

/**
 * Copy a file to a destination.
 *
 * @param {string} sourcePath The platform-specific path at which
 * the file may currently be found.
 * @param {string} destPath The platform-specific path at which the
 * file should be copied.
 * @param {*=} options An object which may contain the following fields:
 *
 * @option {bool} noOverwrite - If true, this function will fail if
 * a file already exists at |destPath|. Otherwise, if this file exists,
 * it will be erased silently.
 *
 * @rejects {OS.File.Error} In case of any error.
 *
 * General note: The behavior of this function is defined only when
 * it is called on a single file. If it is called on a directory, the
 * behavior is undefined and may not be the same across all platforms.
 *
 * General note: The behavior of this function with respect to metadata
 * is unspecified. Metadata may or may not be copied with the file. The
 * behavior may not be the same across all platforms.
 */
File.copy = function copy(sourcePath, destPath, options) {
  return Scheduler.post(
    "copy",
    [Type.path.toMsg(sourcePath), Type.path.toMsg(destPath), options],
    [sourcePath, destPath]
  );
};

/**
 * Move a file to a destination.
 *
 * @param {string} sourcePath The platform-specific path at which
 * the file may currently be found.
 * @param {string} destPath The platform-specific path at which the
 * file should be moved.
 * @param {*=} options An object which may contain the following fields:
 *
 * @option {bool} noOverwrite - If set, this function will fail if
 * a file already exists at |destPath|. Otherwise, if this file exists,
 * it will be erased silently.
 *
 * @returns {Promise}
 * @rejects {OS.File.Error} In case of any error.
 *
 * General note: The behavior of this function is defined only when
 * it is called on a single file. If it is called on a directory, the
 * behavior is undefined and may not be the same across all platforms.
 *
 * General note: The behavior of this function with respect to metadata
 * is unspecified. Metadata may or may not be moved with the file. The
 * behavior may not be the same across all platforms.
 */
File.move = function move(sourcePath, destPath, options) {
  return Scheduler.post(
    "move",
    [Type.path.toMsg(sourcePath), Type.path.toMsg(destPath), options],
    [sourcePath, destPath]
  );
};

/**
 * Create a symbolic link to a source.
 *
 * @param {string} sourcePath The platform-specific path to which
 * the symbolic link should point.
 * @param {string} destPath The platform-specific path at which the
 * symbolic link should be created.
 *
 * @returns {Promise}
 * @rejects {OS.File.Error} In case of any error.
 */
if (!SharedAll.Constants.Win) {
  File.unixSymLink = function unixSymLink(sourcePath, destPath) {
    return Scheduler.post(
      "unixSymLink",
      [Type.path.toMsg(sourcePath), Type.path.toMsg(destPath)],
      [sourcePath, destPath]
    );
  };
}

/**
 * Remove an empty directory.
 *
 * @param {string} path The name of the directory to remove.
 * @param {*=} options Additional options.
 *   - {bool} ignoreAbsent If |true|, do not fail if the
 *     directory does not exist yet.
 */
File.removeEmptyDir = function removeEmptyDir(path, options) {
  return Scheduler.post(
    "removeEmptyDir",
    [Type.path.toMsg(path), options],
    path
  );
};

/**
 * Remove an existing file.
 *
 * @param {string} path The name of the file.
 * @param {*=} options Additional options.
 *   - {bool} ignoreAbsent If |false|, throw an error if the file does
 *     not exist. |true| by default.
 *
 * @throws {OS.File.Error} In case of I/O error.
 */
File.remove = function remove(path, options) {
  return Scheduler.post("remove", [Type.path.toMsg(path), options], path);
};

/**
 * Create a directory and, optionally, its parent directories.
 *
 * @param {string} path The name of the directory.
 * @param {*=} options Additional options.
 *
 * - {string} from If specified, the call to |makeDir| creates all the
 * ancestors of |path| that are descendants of |from|. Note that |path|
 * must be a descendant of |from|, and that |from| and its existing
 * subdirectories present in |path|  must be user-writeable.
 * Example:
 *   makeDir(Path.join(profileDir, "foo", "bar"), { from: profileDir });
 *  creates directories profileDir/foo, profileDir/foo/bar
 * - {bool} ignoreExisting If |false|, throw an error if the directory
 * already exists. |true| by default. Ignored if |from| is specified.
 * - {number} unixMode Under Unix, if specified, a file creation mode,
 * as per libc function |mkdir|. If unspecified, dirs are
 * created with a default mode of 0700 (dir is private to
 * the user, the user can read, write and execute). Ignored under Windows
 * or if the file system does not support file creation modes.
 * - {C pointer} winSecurity Under Windows, if specified, security
 * attributes as per winapi function |CreateDirectory|. If
 * unspecified, use the default security descriptor, inherited from
 * the parent directory. Ignored under Unix or if the file system
 * does not support security descriptors.
 */
File.makeDir = function makeDir(path, options) {
  return Scheduler.post("makeDir", [Type.path.toMsg(path), options], path);
};

/**
 * Return the contents of a file
 *
 * @param {string} path The path to the file.
 * @param {number=} bytes Optionally, an upper bound to the number of bytes
 * to read. DEPRECATED - please use options.bytes instead.
 * @param {JSON} options Additional options.
 * - {boolean} sequential A flag that triggers a population of the page cache
 * with data from a file so that subsequent reads from that file would not
 * block on disk I/O. If |true| or unspecified, inform the system that the
 * contents of the file will be read in order. Otherwise, make no such
 * assumption. |true| by default.
 * - {number} bytes An upper bound to the number of bytes to read.
 * - {string} compression If "lz4" and if the file is compressed using the lz4
 * compression algorithm, decompress the file contents on the fly.
 *
 * @resolves {Uint8Array} A buffer holding the bytes
 * read from the file.
 */
File.read = function read(path, bytes, options = {}) {
  if (typeof bytes == "object") {
    // Passing |bytes| as an argument is deprecated.
    // We should now be passing it as a field of |options|.
    options = bytes || {};
  } else {
    options = clone(options, [
      "outExecutionDuration",
      "outSerializationDuration",
    ]);
    if (typeof bytes != "undefined") {
      options.bytes = bytes;
    }
  }

  if (options.compression || !nativeWheneverAvailable) {
    // We need to use the JS implementation.
    let promise = Scheduler.post(
      "read",
      [Type.path.toMsg(path), bytes, options],
      path
    );
    return promise.then(function onSuccess(data) {
      if (typeof data == "string") {
        return data;
      }
      return new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
    });
  }

  // Otherwise, use the native implementation.
  return Scheduler.push(() => Native.read(path, options));
};

/**
 * Find outs if a file exists.
 *
 * @param {string} path The path to the file.
 *
 * @return {bool} true if the file exists, false otherwise.
 */
File.exists = function exists(path) {
  return Scheduler.post("exists", [Type.path.toMsg(path)], path);
};

/**
 * Write a file, atomically.
 *
 * By opposition to a regular |write|, this operation ensures that,
 * until the contents are fully written, the destination file is
 * not modified.
 *
 * Limitation: In a few extreme cases (hardware failure during the
 * write, user unplugging disk during the write, etc.), data may be
 * corrupted. If your data is user-critical (e.g. preferences,
 * application data, etc.), you may wish to consider adding options
 * |tmpPath| and/or |flush| to reduce the likelihood of corruption, as
 * detailed below. Note that no combination of options can be
 * guaranteed to totally eliminate the risk of corruption.
 *
 * @param {string} path The path of the file to modify.
 * @param {Typed Array | C pointer} buffer A buffer containing the bytes to write.
 * @param {*=} options Optionally, an object determining the behavior
 * of this function. This object may contain the following fields:
 * - {number} bytes The number of bytes to write. If unspecified,
 * |buffer.byteLength|. Required if |buffer| is a C pointer.
 * - {string} tmpPath If |null| or unspecified, write all data directly
 * to |path|. If specified, write all data to a temporary file called
 * |tmpPath| and, once this write is complete, rename the file to
 * replace |path|. Performing this additional operation is a little
 * slower but also a little safer.
 * - {bool} noOverwrite - If set, this function will fail if a file already
 * exists at |path|.
 * - {bool} flush - If |false| or unspecified, return immediately once the
 * write is complete. If |true|, before writing, force the operating system
 * to write its internal disk buffers to the disk. This is considerably slower
 * (not just for the application but for the whole system) but also safer:
 * if the system shuts down improperly (typically due to a kernel freeze
 * or a power failure) or if the device is disconnected before the buffer
 * is flushed, the file has more chances of not being corrupted.
 * - {string} backupTo - If specified, backup the destination file as |backupTo|.
 * Note that this function renames the destination file before overwriting it.
 * If the process or the operating system freezes or crashes
 * during the short window between these operations,
 * the destination file will have been moved to its backup.
 *
 * @return {promise}
 * @resolves {number} The number of bytes actually written.
 */
File.writeAtomic = function writeAtomic(path, buffer, options = {}) {
  const useNativeImplementation =
    nativeWheneverAvailable &&
    !options.compression &&
    !(isTypedArray(buffer) && "byteOffset" in buffer && buffer.byteOffset > 0);
  // Copy |options| to avoid modifying the original object but preserve the
  // reference to |outExecutionDuration|, |outSerializationDuration| option if it is passed.
  options = clone(options, [
    "outExecutionDuration",
    "outSerializationDuration",
  ]);
  // As options.tmpPath is a path, we need to encode it as |Type.path| message, but only
  // if we are not using the native implementation.
  if ("tmpPath" in options && !useNativeImplementation) {
    options.tmpPath = Type.path.toMsg(options.tmpPath);
  }
  if (isTypedArray(buffer) && !("bytes" in options)) {
    options.bytes = buffer.byteLength;
  }
  let refObj = {};
  let promise;
  TelemetryStopwatch.start("OSFILE_WRITEATOMIC_JANK_MS", refObj);
  if (useNativeImplementation) {
    promise = Scheduler.push(() => Native.writeAtomic(path, buffer, options));
  } else {
    promise = Scheduler.post(
      "writeAtomic",
      [Type.path.toMsg(path), Type.void_t.in_ptr.toMsg(buffer), options],
      [options, buffer, path]
    );
  }
  TelemetryStopwatch.finish("OSFILE_WRITEATOMIC_JANK_MS", refObj);
  return promise;
};

File.removeDir = function(path, options = {}) {
  return Scheduler.post("removeDir", [Type.path.toMsg(path), options], path);
};

/**
 * Information on a file, as returned by OS.File.stat or
 * OS.File.prototype.stat
 *
 * @constructor
 */
File.Info = function Info(value) {
  // Note that we can't just do this[k] = value[k] because our
  // prototype defines getters for all of these fields.
  for (let k in value) {
    Object.defineProperty(this, k, { value: value[k] });
  }
};
File.Info.prototype = SysAll.AbstractInfo.prototype;

File.Info.fromMsg = function fromMsg(value) {
  return new File.Info(value);
};

/**
 * Get worker's current DEBUG flag.
 * Note: This is used for testing purposes.
 */
File.GET_DEBUG = function GET_DEBUG() {
  return Scheduler.post("GET_DEBUG");
};

/**
 * Iterate asynchronously through a directory
 *
 * @constructor
 */
var DirectoryIterator = function DirectoryIterator(path, options) {
  /**
   * Open the iterator on the worker thread
   *
   * @type {Promise}
   * @resolves {*} A message accepted by the methods of DirectoryIterator
   * in the worker thread
   */
  this._itmsg = Scheduler.post(
    "new_DirectoryIterator",
    [Type.path.toMsg(path), options],
    path
  );
  this._isClosed = false;
};
DirectoryIterator.prototype = {
  [Symbol.asyncIterator]() {
    return this;
  },

  _itmsg: null,

  /**
   * Determine whether the directory exists.
   *
   * @resolves {boolean}
   */
  async exists() {
    if (this._isClosed) {
      return Promise.resolve(false);
    }
    let iterator = await this._itmsg;
    return Scheduler.post("DirectoryIterator_prototype_exists", [iterator]);
  },
  /**
   * Get the next entry in the directory.
   *
   * @return {Promise}
   * @resolves By definition of the async iterator protocol, either
   * `{value: {File.Entry}, done: false}` if there is an unvisited entry
   * in the directory, or `{value: undefined, done: true}`, otherwise.
   */
  async next() {
    if (this._isClosed) {
      return { value: undefined, done: true };
    }
    return this._next(await this._itmsg);
  },
  /**
   * Get several entries at once.
   *
   * @param {number=} length If specified, the number of entries
   * to return. If unspecified, return all remaining entries.
   * @return {Promise}
   * @resolves {Array} An array containing the |length| next entries.
   */
  async nextBatch(size) {
    if (this._isClosed) {
      return [];
    }
    let iterator = await this._itmsg;
    let array = await Scheduler.post("DirectoryIterator_prototype_nextBatch", [
      iterator,
      size,
    ]);
    return array.map(DirectoryIterator.Entry.fromMsg);
  },
  /**
   * Apply a function to all elements of the directory sequentially.
   *
   * @param {Function} cb This function will be applied to all entries
   * of the directory. It receives as arguments
   *  - the OS.File.Entry corresponding to the entry;
   *  - the index of the entry in the enumeration;
   *  - the iterator itself - return |iterator.close()| to stop the loop.
   *
   * If the callback returns a promise, iteration waits until the
   * promise is resolved before proceeding.
   *
   * @return {Promise} A promise resolved once the loop has reached
   * its end.
   */
  async forEach(cb, options) {
    if (this._isClosed) {
      return undefined;
    }
    let position = 0;
    let iterator = await this._itmsg;
    while (true) {
      if (this._isClosed) {
        return undefined;
      }
      let { value, done } = await this._next(iterator);
      if (done) {
        return undefined;
      }
      await cb(value, position++, this);
    }
  },
  /**
   * Auxiliary method: fetch the next item
   *
   * @resolves `{value: undefined, done: true}` If all entries have already
   * been visited or the iterator has been closed.
   */
  async _next(iterator) {
    if (this._isClosed) {
      return { value: undefined, done: true };
    }
    let {
      value,
      done,
    } = await Scheduler.post("DirectoryIterator_prototype_next", [iterator]);
    if (done) {
      this.close();
      return { value: undefined, done: true };
    }
    return { value: DirectoryIterator.Entry.fromMsg(value), done: false };
  },
  /**
   * Close the iterator
   */
  async close() {
    if (this._isClosed) {
      return undefined;
    }
    this._isClosed = true;
    let iterator = this._itmsg;
    this._itmsg = null;
    return Scheduler.post("DirectoryIterator_prototype_close", [iterator]);
  },
};

DirectoryIterator.Entry = function Entry(value) {
  return value;
};
DirectoryIterator.Entry.prototype = Object.create(
  SysAll.AbstractEntry.prototype
);

DirectoryIterator.Entry.fromMsg = function fromMsg(value) {
  return new DirectoryIterator.Entry(value);
};

File.resetWorker = function() {
  return (async function() {
    let resources = await Scheduler.kill({ shutdown: false, reset: true });
    if (resources && !resources.killed) {
      throw new Error(
        "Could not reset worker, this would leak file descriptors: " +
          JSON.stringify(resources)
      );
    }
  })();
};

// Constants
File.POS_START = SysAll.POS_START;
File.POS_CURRENT = SysAll.POS_CURRENT;
File.POS_END = SysAll.POS_END;

// Exports
File.Error = OSError;
File.DirectoryIterator = DirectoryIterator;

var OS = {};
OS.File = File;
OS.Constants = SharedAll.Constants;
OS.Shared = {
  LOG: SharedAll.LOG,
  Type: SysAll.Type,
  get DEBUG() {
    return SharedAll.Config.DEBUG;
  },
  set DEBUG(x) {
    SharedAll.Config.DEBUG = x;
  },
};
Object.freeze(OS.Shared);
OS.Path = Path;

// Returns a resolved promise when all the queued operation have been completed.
Object.defineProperty(OS.File, "queue", {
  get() {
    return Scheduler.queue;
  },
});

// `true` if this is a content process, `false` otherwise.
// It would be nicer to go through `Services.appinfo`, but some tests need to be
// able to replace that field with a custom implementation before it is first
// called.
const isContent =
  // eslint-disable-next-line mozilla/use-services
  Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).processType ==
  Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT;

/**
 * Shutdown barriers, to let clients register to be informed during shutdown.
 */
var Barriers = {
  shutdown: new AsyncShutdown.Barrier(
    "OS.File: Waiting for clients before full shutdown"
  ),
  /**
   * Return the shutdown state of OS.File
   */
  getDetails() {
    let result = {
      launched: Scheduler.launched,
      shutdown: Scheduler.shutdown,
      worker: !!Scheduler._worker,
      pendingReset: !!Scheduler.resetTimer,
      latestSent: Scheduler.Debugging.latestSent,
      latestReceived: Scheduler.Debugging.latestReceived,
      messagesSent: Scheduler.Debugging.messagesSent,
      messagesReceived: Scheduler.Debugging.messagesReceived,
      messagesQueued: Scheduler.Debugging.messagesQueued,
      DEBUG: SharedAll.Config.DEBUG,
    };
    // Convert dates to strings for better readability
    for (let key of ["latestSent", "latestReceived"]) {
      if (result[key] && typeof result[key][0] == "number") {
        result[key][0] = Date(result[key][0]);
      }
    }
    return result;
  },
};

function setupShutdown(phaseName) {
  Barriers[phaseName] = new AsyncShutdown.Barrier(
    `OS.File: Waiting for clients before ${phaseName}`
  );
  File[phaseName] = Barriers[phaseName].client;

  // Auto-flush OS.File during `phaseName`. This ensures that any I/O
  // that has been queued *before* `phaseName` is properly completed.
  // To ensure that I/O queued *during* `phaseName` change is completed,
  // clients should register using AsyncShutdown.addBlocker.
  AsyncShutdown[phaseName].addBlocker(
    `OS.File: flush I/O queued before ${phaseName}`,
    async function() {
      // Give clients a last chance to enqueue requests.
      await Barriers[phaseName].wait({ crashAfterMS: null });

      // Wait until all currently enqueued requests are completed.
      await Scheduler.queue;
    },
    () => {
      let details = Barriers.getDetails();
      details.clients = Barriers[phaseName].state;
      return details;
    }
  );
}

// profile-before-change only exists in the parent, and OS.File should
// not be used in the child process anyways.
if (!isContent) {
  setupShutdown("profileBeforeChange");
}
File.shutdown = Barriers.shutdown.client;
