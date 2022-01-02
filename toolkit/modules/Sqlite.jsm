/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Sqlite"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  Services: "resource://gre/modules/Services.jsm",
  Log: "resource://gre/modules/Log.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "FinalizationWitnessService",
  "@mozilla.org/toolkit/finalizationwitness;1",
  "nsIFinalizationWitnessService"
);

// Regular expression used by isInvalidBoundLikeQuery
var likeSqlRegex = /\bLIKE\b\s(?![@:?])/i;

// Counts the number of created connections per database basename(). This is
// used for logging to distinguish connection instances.
var connectionCounters = new Map();

// Tracks identifiers of wrapped connections, that are Storage connections
// opened through mozStorage and then wrapped by Sqlite.jsm to use its syntactic
// sugar API.  Since these connections have an unknown origin, we use this set
// to differentiate their behavior.
var wrappedConnections = new Set();

/**
 * Once `true`, reject any attempt to open or close a database.
 */
var isClosed = false;

var Debugging = {
  // Tests should fail if a connection auto closes.  The exception is
  // when finalization itself is tested, in which case this flag
  // should be set to false.
  failTestsOnAutoClose: true,
};

/**
 * Helper function to check whether LIKE is implemented using proper bindings.
 *
 * @param sql
 *        (string) The SQL query to be verified.
 * @return boolean value telling us whether query was correct or not
 */
function isInvalidBoundLikeQuery(sql) {
  return likeSqlRegex.test(sql);
}

// Displays a script error message
function logScriptError(message) {
  let consoleMessage = Cc["@mozilla.org/scripterror;1"].createInstance(
    Ci.nsIScriptError
  );
  let stack = new Error();
  consoleMessage.init(
    message,
    stack.fileName,
    null,
    stack.lineNumber,
    0,
    Ci.nsIScriptError.errorFlag,
    "component javascript"
  );
  Services.console.logMessage(consoleMessage);

  // This `Promise.reject` will cause tests to fail.  The debugging
  // flag can be used to suppress this for tests that explicitly
  // test auto closes.
  if (Debugging.failTestsOnAutoClose) {
    Promise.reject(new Error(message));
  }
}

/**
 * Gets connection identifier from its database file name.
 *
 * @param fileName
 *        A database file string name.
 * @return the connection identifier.
 */
function getIdentifierByFileName(fileName) {
  let number = connectionCounters.get(fileName) || 0;
  connectionCounters.set(fileName, number + 1);
  return fileName + "#" + number;
}

/**
 * Convert mozIStorageError to common NS_ERROR_*
 * The conversion is mostly based on the one in
 * mozStoragePrivateHelpers::ConvertResultCode, plus a few additions.
 *
 * @param {integer} result a mozIStorageError result code.
 * @returns {integer} an NS_ERROR_* result code.
 */
function convertStorageErrorResult(result) {
  switch (result) {
    case Ci.mozIStorageError.PERM:
    case Ci.mozIStorageError.AUTH:
    case Ci.mozIStorageError.CANTOPEN:
      return Cr.NS_ERROR_FILE_ACCESS_DENIED;
    case Ci.mozIStorageError.LOCKED:
      return Cr.NS_ERROR_FILE_IS_LOCKED;
    case Ci.mozIStorageError.READONLY:
      return Cr.NS_ERROR_FILE_READ_ONLY;
    case Ci.mozIStorageError.ABORT:
    case Ci.mozIStorageError.INTERRUPT:
      return Cr.NS_ERROR_ABORT;
    case Ci.mozIStorageError.TOOBIG:
    case Ci.mozIStorageError.FULL:
      return Cr.NS_ERROR_FILE_NO_DEVICE_SPACE;
    case Ci.mozIStorageError.NOMEM:
      return Cr.NS_ERROR_OUT_OF_MEMORY;
    case Ci.mozIStorageError.BUSY:
      return Cr.NS_ERROR_STORAGE_BUSY;
    case Ci.mozIStorageError.CONSTRAINT:
      return Cr.NS_ERROR_STORAGE_CONSTRAINT;
    case Ci.mozIStorageError.NOLFS:
    case Ci.mozIStorageError.IOERR:
      return Cr.NS_ERROR_STORAGE_IOERR;
    case Ci.mozIStorageError.SCHEMA:
    case Ci.mozIStorageError.MISMATCH:
    case Ci.mozIStorageError.MISUSE:
    case Ci.mozIStorageError.RANGE:
      return Ci.NS_ERROR_UNEXPECTED;
    case Ci.mozIStorageError.CORRUPT:
    case Ci.mozIStorageError.EMPTY:
    case Ci.mozIStorageError.FORMAT:
    case Ci.mozIStorageError.NOTADB:
      return Cr.NS_ERROR_FILE_CORRUPTED;
    default:
      return Cr.NS_ERROR_FAILURE;
  }
}
/**
 * Barriers used to ensure that Sqlite.jsm is shutdown after all
 * its clients.
 */
XPCOMUtils.defineLazyGetter(this, "Barriers", () => {
  let Barriers = {
    /**
     * Public barrier that clients may use to add blockers to the
     * shutdown of Sqlite.jsm. Triggered by profile-before-change.
     * Once all blockers of this barrier are lifted, we close the
     * ability to open new connections.
     */
    shutdown: new AsyncShutdown.Barrier(
      "Sqlite.jsm: wait until all clients have completed their task"
    ),

    /**
     * Private barrier blocked by connections that are still open.
     * Triggered after Barriers.shutdown is lifted and `isClosed` is
     * set to `true`.
     */
    connections: new AsyncShutdown.Barrier(
      "Sqlite.jsm: wait until all connections are closed"
    ),
  };

  /**
   * Observer for the event which is broadcasted when the finalization
   * witness `_witness` of `OpenedConnection` is garbage collected.
   *
   * The observer is passed the connection identifier of the database
   * connection that is being finalized.
   */
  let finalizationObserver = function(subject, topic, identifier) {
    let connectionData = ConnectionData.byId.get(identifier);

    if (connectionData === undefined) {
      logScriptError(
        "Error: Attempt to finalize unknown Sqlite connection: " +
          identifier +
          "\n"
      );
      return;
    }

    ConnectionData.byId.delete(identifier);
    logScriptError(
      "Warning: Sqlite connection '" +
        identifier +
        "' was not properly closed. Auto-close triggered by garbage collection.\n"
    );
    connectionData.close();
  };
  Services.obs.addObserver(finalizationObserver, "sqlite-finalization-witness");

  /**
   * Ensure that Sqlite.jsm:
   * - informs its clients before shutting down;
   * - lets clients open connections during shutdown, if necessary;
   * - waits for all connections to be closed before shutdown.
   */
  AsyncShutdown.profileBeforeChange.addBlocker(
    "Sqlite.jsm shutdown blocker",
    async function() {
      await Barriers.shutdown.wait();
      // At this stage, all clients have had a chance to open (and close)
      // their databases. Some previous close operations may still be pending,
      // so we need to wait until they are complete before proceeding.

      // Prevent any new opening.
      isClosed = true;

      // Now, wait until all databases are closed
      await Barriers.connections.wait();

      // Everything closed, no finalization events to catch
      Services.obs.removeObserver(
        finalizationObserver,
        "sqlite-finalization-witness"
      );
    },

    function status() {
      if (isClosed) {
        // We are waiting for the connections to close. The interesting
        // status is therefore the list of connections still pending.
        return {
          description: "Waiting for connections to close",
          state: Barriers.connections.state,
        };
      }

      // We are still in the first stage: waiting for the barrier
      // to be lifted. The interesting status is therefore that of
      // the barrier.
      return {
        description: "Waiting for the barrier to be lifted",
        state: Barriers.shutdown.state,
      };
    }
  );

  return Barriers;
});

/**
 * Connection data with methods necessary for closing the connection.
 *
 * To support auto-closing in the event of garbage collection, this
 * data structure contains all the connection data of an opened
 * connection and all of the methods needed for sucessfully closing
 * it.
 *
 * By putting this information in its own separate object, it is
 * possible to store an additional reference to it without preventing
 * a garbage collection of a finalization witness in
 * OpenedConnection. When the witness detects a garbage collection,
 * this object can be used to close the connection.
 *
 * This object contains more methods than just `close`.  When
 * OpenedConnection needs to use the methods in this object, it will
 * dispatch its method calls here.
 */
function ConnectionData(connection, identifier, options = {}) {
  this._log = Log.repository.getLoggerWithMessagePrefix(
    "Sqlite.jsm",
    `Connection ${identifier}: `
  );
  this._log.manageLevelFromPref("toolkit.sqlitejsm.loglevel");
  this._log.debug("Opened");

  this._dbConn = connection;

  // This is a unique identifier for the connection, generated through
  // getIdentifierByFileName.  It may be used for logging or as a key in Maps.
  this._identifier = identifier;

  this._open = true;

  this._cachedStatements = new Map();
  this._anonymousStatements = new Map();
  this._anonymousCounter = 0;

  // A map from statement index to mozIStoragePendingStatement, to allow for
  // canceling prior to finalizing the mozIStorageStatements.
  this._pendingStatements = new Map();

  // Increments for each executed statement for the life of the connection.
  this._statementCounter = 0;

  // Increments whenever we request a unique operation id.
  this._operationsCounter = 0;

  if ("defaultTransactionType" in options) {
    this.defaultTransactionType = options.defaultTransactionType;
  } else {
    this.defaultTransactionType = convertStorageTransactionType(
      this._dbConn.defaultTransactionType
    );
  }
  // Tracks whether this instance initiated a transaction.
  this._initiatedTransaction = false;
  // Manages a chain of transactions promises, so that new transactions
  // always happen in queue to the previous ones.  It never rejects.
  this._transactionQueue = Promise.resolve();

  this._idleShrinkMS = options.shrinkMemoryOnConnectionIdleMS;
  if (this._idleShrinkMS) {
    this._idleShrinkTimer = Cc["@mozilla.org/timer;1"].createInstance(
      Ci.nsITimer
    );
    // We wait for the first statement execute to start the timer because
    // shrinking now would not do anything.
  }

  // Deferred whose promise is resolved when the connection closing procedure
  // is complete.
  this._deferredClose = PromiseUtils.defer();
  this._closeRequested = false;

  // An AsyncShutdown barrier used to make sure that we wait until clients
  // are done before shutting down the connection.
  this._barrier = new AsyncShutdown.Barrier(
    `${this._identifier}: waiting for clients`
  );

  Barriers.connections.client.addBlocker(
    this._identifier + ": waiting for shutdown",
    this._deferredClose.promise,
    () => ({
      identifier: this._identifier,
      isCloseRequested: this._closeRequested,
      hasDbConn: !!this._dbConn,
      initiatedTransaction: this._initiatedTransaction,
      pendingStatements: this._pendingStatements.size,
      statementCounter: this._statementCounter,
    })
  );

  // We avoid creating a timer for every transaction, because in most cases they
  // are not canceled and they are only used as a timeout.
  // Instead the timer is reused when it's sufficiently close to the previous
  // creation time (see `_getTimeoutPromise` for more info).
  this._timeoutPromise = null;
  // The last timestamp when we should consider using `this._timeoutPromise`.
  this._timeoutPromiseExpires = 0;
}

/**
 * Map of connection identifiers to ConnectionData objects
 *
 * The connection identifier is a human-readable name of the
 * database. Used by finalization witnesses to be able to close opened
 * connections on garbage collection.
 *
 * Key: _identifier of ConnectionData
 * Value: ConnectionData object
 */
ConnectionData.byId = new Map();

ConnectionData.prototype = Object.freeze({
  /**
   * Run a task, ensuring that its execution will not be interrupted by shutdown.
   *
   * As the operations of this module are asynchronous, a sequence of operations,
   * or even an individual operation, can still be pending when the process shuts
   * down. If any of this operations is a write, this can cause data loss, simply
   * because the write has not been completed (or even started) by shutdown.
   *
   * To avoid this risk, clients are encouraged to use `executeBeforeShutdown` for
   * any write operation, as follows:
   *
   * myConnection.executeBeforeShutdown("Bookmarks: Removing a bookmark",
   *   async function(db) {
   *     // The connection will not be closed and shutdown will not proceed
   *     // until this task has completed.
   *
   *     // `db` exposes the same API as `myConnection` but provides additional
   *     // logging support to help debug hard-to-catch shutdown timeouts.
   *
   *     await db.execute(...);
   * }));
   *
   * @param {string} name A human-readable name for the ongoing operation, used
   *  for logging and debugging purposes.
   * @param {function(db)} task A function that takes as argument a Sqlite.jsm
   *  db and returns a Promise.
   */
  executeBeforeShutdown(parent, name, task) {
    if (!name) {
      throw new TypeError("Expected a human-readable name as first argument");
    }
    if (typeof task != "function") {
      throw new TypeError("Expected a function as second argument");
    }
    if (this._closeRequested) {
      throw new Error(
        `${this._identifier}: cannot execute operation ${name}, the connection is already closing`
      );
    }

    // Status, used for AsyncShutdown crash reports.
    let status = {
      // The latest command started by `task`, either as a
      // sql string, or as one of "<not started>" or "<closing>".
      command: "<not started>",

      // `true` if `command` was started but not completed yet.
      isPending: false,
    };

    // An object with the same API as `this` but with
    // additional logging. To keep logging simple, we
    // assume that `task` is not running several queries
    // concurrently.
    let loggedDb = Object.create(parent, {
      execute: {
        value: async (sql, ...rest) => {
          status.isPending = true;
          status.command = sql;
          try {
            return await this.execute(sql, ...rest);
          } finally {
            status.isPending = false;
          }
        },
      },
      close: {
        value: async () => {
          status.isPending = true;
          status.command = "<close>";
          try {
            return await this.close();
          } finally {
            status.isPending = false;
          }
        },
      },
      executeCached: {
        value: async (sql, ...rest) => {
          status.isPending = true;
          status.command = "cached: " + sql;
          try {
            return await this.executeCached(sql, ...rest);
          } finally {
            status.isPending = false;
          }
        },
      },
    });

    let promiseResult = task(loggedDb);
    if (
      !promiseResult ||
      typeof promiseResult != "object" ||
      !("then" in promiseResult)
    ) {
      throw new TypeError("Expected a Promise");
    }
    let key = `${this._identifier}: ${name} (${this._getOperationId()})`;
    let promiseComplete = promiseResult.catch(() => {});
    this._barrier.client.addBlocker(key, promiseComplete, {
      fetchState: () => status,
    });

    return (async () => {
      try {
        return await promiseResult;
      } finally {
        this._barrier.client.removeBlocker(key, promiseComplete);
      }
    })();
  },
  close() {
    this._closeRequested = true;

    if (!this._dbConn) {
      return this._deferredClose.promise;
    }

    this._log.debug("Request to close connection.");
    this._clearIdleShrinkTimer();

    return this._barrier.wait().then(() => {
      if (!this._dbConn) {
        return undefined;
      }
      return this._finalize();
    });
  },

  clone(readOnly = false) {
    this.ensureOpen();

    this._log.debug("Request to clone connection.");

    let options = {
      connection: this._dbConn,
      readOnly,
    };
    if (this._idleShrinkMS) {
      options.shrinkMemoryOnConnectionIdleMS = this._idleShrinkMS;
    }

    return cloneStorageConnection(options);
  },
  _getOperationId() {
    return this._operationsCounter++;
  },
  _finalize() {
    this._log.debug("Finalizing connection.");
    // Cancel any pending statements.
    for (let [, /* k */ statement] of this._pendingStatements) {
      statement.cancel();
    }
    this._pendingStatements.clear();

    // We no longer need to track these.
    this._statementCounter = 0;

    // Next we finalize all active statements.
    for (let [, /* k */ statement] of this._anonymousStatements) {
      statement.finalize();
    }
    this._anonymousStatements.clear();

    for (let [, /* k */ statement] of this._cachedStatements) {
      statement.finalize();
    }
    this._cachedStatements.clear();

    // This guards against operations performed between the call to this
    // function and asyncClose() finishing. See also bug 726990.
    this._open = false;

    // We must always close the connection at the Sqlite.jsm-level, not
    // necessarily at the mozStorage-level.
    let markAsClosed = () => {
      this._log.debug("Closed");
      // Now that the connection is closed, no need to keep
      // a blocker for Barriers.connections.
      Barriers.connections.client.removeBlocker(this._deferredClose.promise);
      this._deferredClose.resolve();
    };
    if (wrappedConnections.has(this._identifier)) {
      wrappedConnections.delete(this._identifier);
      this._dbConn = null;
      markAsClosed();
    } else {
      this._log.debug("Calling asyncClose().");
      try {
        this._dbConn.asyncClose(markAsClosed);
      } catch (ex) {
        // If for any reason asyncClose fails, we must still remove the
        // shutdown blockers and resolve _deferredClose.
        markAsClosed();
      } finally {
        this._dbConn = null;
      }
    }
    return this._deferredClose.promise;
  },

  executeCached(sql, params = null, onRow = null) {
    this.ensureOpen();

    if (!sql) {
      throw new Error("sql argument is empty.");
    }

    let statement = this._cachedStatements.get(sql);
    if (!statement) {
      statement = this._dbConn.createAsyncStatement(sql);
      this._cachedStatements.set(sql, statement);
    }

    this._clearIdleShrinkTimer();

    return new Promise((resolve, reject) => {
      try {
        this._executeStatement(sql, statement, params, onRow).then(
          result => {
            this._startIdleShrinkTimer();
            resolve(result);
          },
          error => {
            this._startIdleShrinkTimer();
            reject(error);
          }
        );
      } catch (ex) {
        this._startIdleShrinkTimer();
        throw ex;
      }
    });
  },

  execute(sql, params = null, onRow = null) {
    if (typeof sql != "string") {
      throw new Error("Must define SQL to execute as a string: " + sql);
    }

    this.ensureOpen();

    let statement = this._dbConn.createAsyncStatement(sql);
    let index = this._anonymousCounter++;

    this._anonymousStatements.set(index, statement);
    this._clearIdleShrinkTimer();

    let onFinished = () => {
      this._anonymousStatements.delete(index);
      statement.finalize();
      this._startIdleShrinkTimer();
    };

    return new Promise((resolve, reject) => {
      try {
        this._executeStatement(sql, statement, params, onRow).then(
          rows => {
            onFinished();
            resolve(rows);
          },
          error => {
            onFinished();
            reject(error);
          }
        );
      } catch (ex) {
        onFinished();
        throw ex;
      }
    });
  },

  get transactionInProgress() {
    return this._open && this._dbConn.transactionInProgress;
  },

  executeTransaction(func, type) {
    // Identify the caller for debugging purposes.
    let caller = new Error().stack
      .split("\n", 3)
      .pop()
      .match(/^([^@]+@).*\/([^\/:]+)[:0-9]*$/);
    caller = caller[1] + caller[2];
    this._log.debug(`Transaction (type ${type}) requested by: ${caller}`);

    if (type == OpenedConnection.prototype.TRANSACTION_DEFAULT) {
      type = this.defaultTransactionType;
    } else if (!OpenedConnection.TRANSACTION_TYPES.includes(type)) {
      throw new Error("Unknown transaction type: " + type);
    }
    this.ensureOpen();

    // If a transaction yields on a never resolved promise, or is mistakenly
    // nested, it could hang the transactions queue forever.  Thus we timeout
    // the execution after a meaningful amount of time, to ensure in any case
    // we'll proceed after a while.
    let timeoutPromise = this._getTimeoutPromise();

    let promise = this._transactionQueue.then(() => {
      if (this._closeRequested) {
        throw new Error("Transaction canceled due to a closed connection.");
      }

      let transactionPromise = (async () => {
        // At this point we should never have an in progress transaction, since
        // they are enqueued.
        if (this._initiatedTransaction) {
          this._log.error(
            "Unexpected transaction in progress when trying to start a new one."
          );
        }
        try {
          // We catch errors in statement execution to detect nested transactions.
          try {
            await this.execute("BEGIN " + type + " TRANSACTION");
            this._log.debug(`Begin transaction`);
            this._initiatedTransaction = true;
          } catch (ex) {
            // Unfortunately, if we are wrapping an existing connection, a
            // transaction could have been started by a client of the same
            // connection that doesn't use Sqlite.jsm (e.g. C++ consumer).
            // The best we can do is proceed without a transaction and hope
            // things won't break.
            if (wrappedConnections.has(this._identifier)) {
              this._log.warn(
                "A new transaction could not be started cause the wrapped connection had one in progress",
                ex
              );
            } else {
              this._log.warn(
                "A transaction was already in progress, likely a nested transaction",
                ex
              );
              throw ex;
            }
          }

          let result;
          try {
            result = await Promise.race([func(), timeoutPromise]);
          } catch (ex) {
            // It's possible that the exception has been caused by trying to
            // close the connection in the middle of a transaction.
            if (this._closeRequested) {
              this._log.warn(
                "Connection closed while performing a transaction",
                ex
              );
            } else {
              // Otherwise the function didn't resolve before the timeout, or
              // generated an unexpected error. Then we rollback.
              if (ex.becauseTimedOut) {
                let caller_module = caller.split(":", 1)[0];
                Services.telemetry.keyedScalarAdd(
                  "mozstorage.sqlitejsm_transaction_timeout",
                  caller_module,
                  1
                );
                this._log.error(
                  `The transaction requested by ${caller} timed out. Rolling back`,
                  ex
                );
              } else {
                this._log.error(
                  `Error during transaction requested by ${caller}. Rolling back`,
                  ex
                );
              }
              // If we began a transaction, we must rollback it.
              if (this._initiatedTransaction) {
                try {
                  await this.execute("ROLLBACK TRANSACTION");
                  this._initiatedTransaction = false;
                  this._log.debug(`Roll back transaction`);
                } catch (inner) {
                  this._log.error("Could not roll back transaction", inner);
                }
              }
            }
            // Rethrow the exception.
            throw ex;
          }

          // See comment above about connection being closed during transaction.
          if (this._closeRequested) {
            this._log.warn(
              "Connection closed before committing the transaction."
            );
            throw new Error(
              "Connection closed before committing the transaction."
            );
          }

          // If we began a transaction, we must commit it.
          if (this._initiatedTransaction) {
            try {
              await this.execute("COMMIT TRANSACTION");
              this._log.debug(`Commit transaction`);
            } catch (ex) {
              this._log.warn("Error committing transaction", ex);
              throw ex;
            }
          }

          return result;
        } finally {
          this._initiatedTransaction = false;
        }
      })();

      return Promise.race([transactionPromise, timeoutPromise]);
    });
    // Atomically update the queue before anyone else has a chance to enqueue
    // further transactions.
    this._transactionQueue = promise.catch(ex => {
      this._log.error(ex);
    });

    // Make sure that we do not shutdown the connection during a transaction.
    this._barrier.client.addBlocker(
      `Transaction (${this._getOperationId()})`,
      this._transactionQueue
    );
    return promise;
  },

  shrinkMemory() {
    this._log.debug("Shrinking memory usage.");
    let onShrunk = this._clearIdleShrinkTimer.bind(this);
    return this.execute("PRAGMA shrink_memory").then(onShrunk, onShrunk);
  },

  discardCachedStatements() {
    let count = 0;
    for (let [, /* k */ statement] of this._cachedStatements) {
      ++count;
      statement.finalize();
    }
    this._cachedStatements.clear();
    this._log.debug("Discarded " + count + " cached statements.");
    return count;
  },

  interrupt() {
    this._log.debug("Trying to interrupt.");
    this.ensureOpen();
    this._dbConn.interrupt();
  },

  /**
   * Helper method to bind parameters of various kinds through
   * reflection.
   */
  _bindParameters(statement, params) {
    if (!params) {
      return;
    }

    function bindParam(obj, key, val) {
      let isBlob =
        val && typeof val == "object" && val.constructor.name == "Uint8Array";
      let args = [key, val];
      if (isBlob) {
        args.push(val.length);
      }
      let methodName = `bind${isBlob ? "Blob" : ""}By${
        typeof key == "number" ? "Index" : "Name"
      }`;
      obj[methodName](...args);
    }

    if (Array.isArray(params)) {
      // It's an array of separate params.
      if (params.length && typeof params[0] == "object" && params[0] !== null) {
        let paramsArray = statement.newBindingParamsArray();
        for (let p of params) {
          let bindings = paramsArray.newBindingParams();
          for (let [key, value] of Object.entries(p)) {
            bindParam(bindings, key, value);
          }
          paramsArray.addParams(bindings);
        }

        statement.bindParameters(paramsArray);
        return;
      }

      // Indexed params.
      for (let i = 0; i < params.length; i++) {
        bindParam(statement, i, params[i]);
      }
      return;
    }

    // Named params.
    if (params && typeof params == "object") {
      for (let k in params) {
        bindParam(statement, k, params[k]);
      }
      return;
    }

    throw new Error(
      "Invalid type for bound parameters. Expected Array or " +
        "object. Got: " +
        params
    );
  },

  _executeStatement(sql, statement, params, onRow) {
    if (statement.state != statement.MOZ_STORAGE_STATEMENT_READY) {
      throw new Error("Statement is not ready for execution.");
    }

    if (onRow && typeof onRow != "function") {
      throw new Error("onRow must be a function. Got: " + onRow);
    }

    this._bindParameters(statement, params);

    let index = this._statementCounter++;

    let deferred = PromiseUtils.defer();
    let userCancelled = false;
    let errors = [];
    let rows = [];
    let handledRow = false;

    // Don't incur overhead for serializing params unless the messages go
    // somewhere.
    if (this._log.level <= Log.Level.Trace) {
      let msg = "Stmt #" + index + " " + sql;

      if (params) {
        msg += " - " + JSON.stringify(params);
      }
      this._log.trace(msg);
    } else {
      this._log.debug("Stmt #" + index + " starting");
    }

    let self = this;
    let pending = statement.executeAsync({
      handleResult(resultSet) {
        // .cancel() may not be immediate and handleResult() could be called
        // after a .cancel().
        for (
          let row = resultSet.getNextRow();
          row && !userCancelled;
          row = resultSet.getNextRow()
        ) {
          if (!onRow) {
            rows.push(row);
            continue;
          }

          handledRow = true;

          try {
            onRow(row, () => {
              userCancelled = true;
              pending.cancel();
            });
          } catch (e) {
            self._log.warn("Exception when calling onRow callback", e);
          }
        }
      },

      handleError(error) {
        self._log.warn(
          "Error when executing SQL (" + error.result + "): " + error.message
        );
        errors.push(error);
      },

      handleCompletion(reason) {
        self._log.debug("Stmt #" + index + " finished.");
        self._pendingStatements.delete(index);

        switch (reason) {
          case Ci.mozIStorageStatementCallback.REASON_FINISHED:
          case Ci.mozIStorageStatementCallback.REASON_CANCELED:
            // If there is an onRow handler, we always instead resolve to a
            // boolean indicating whether the onRow handler was called or not.
            let result = onRow ? handledRow : rows;
            deferred.resolve(result);
            break;

          case Ci.mozIStorageStatementCallback.REASON_ERROR:
            let error = new Error(
              "Error(s) encountered during statement execution: " +
                errors.map(e => e.message).join(", ")
            );
            error.errors = errors;

            // Forward the error result.
            // Corruption is the most critical one so it's handled apart.
            if (errors.some(e => e.result == Ci.mozIStorageError.CORRUPT)) {
              error.result = Cr.NS_ERROR_FILE_CORRUPTED;
            } else {
              // Just use the first error result in the other cases.
              error.result = convertStorageErrorResult(errors[0]?.result);
            }

            deferred.reject(error);
            break;

          default:
            deferred.reject(
              new Error("Unknown completion reason code: " + reason)
            );
            break;
        }
      },
    });

    this._pendingStatements.set(index, pending);
    return deferred.promise;
  },

  ensureOpen() {
    if (!this._open) {
      throw new Error("Connection is not open.");
    }
  },

  _clearIdleShrinkTimer() {
    if (!this._idleShrinkTimer) {
      return;
    }

    this._idleShrinkTimer.cancel();
  },

  _startIdleShrinkTimer() {
    if (!this._idleShrinkTimer) {
      return;
    }

    this._idleShrinkTimer.initWithCallback(
      this.shrinkMemory.bind(this),
      this._idleShrinkMS,
      this._idleShrinkTimer.TYPE_ONE_SHOT
    );
  },

  /**
   * Returns a promise that will resolve after a time comprised between 80% of
   * `TRANSACTIONS_TIMEOUT_MS` and `TRANSACTIONS_TIMEOUT_MS`. Use
   * this method instead of creating several individual timers that may survive
   * longer than necessary.
   */
  _getTimeoutPromise() {
    if (this._timeoutPromise && Cu.now() <= this._timeoutPromiseExpires) {
      return this._timeoutPromise;
    }
    let timeoutPromise = new Promise((resolve, reject) => {
      setTimeout(() => {
        // Clear out this._timeoutPromise if it hasn't changed since we set it.
        if (this._timeoutPromise == timeoutPromise) {
          this._timeoutPromise = null;
        }
        let e = new Error(
          "Transaction timeout, most likely caused by unresolved pending work."
        );
        e.becauseTimedOut = true;
        reject(e);
      }, Sqlite.TRANSACTIONS_TIMEOUT_MS);
    });
    this._timeoutPromise = timeoutPromise;
    this._timeoutPromiseExpires =
      Cu.now() + Sqlite.TRANSACTIONS_TIMEOUT_MS * 0.2;
    return this._timeoutPromise;
  },
});

/**
 * Opens a connection to a SQLite database.
 *
 * The following parameters can control the connection:
 *
 *   path -- (string) The filesystem path of the database file to open. If the
 *       file does not exist, a new database will be created.
 *
 *   sharedMemoryCache -- (bool) Whether multiple connections to the database
 *       share the same memory cache. Sharing the memory cache likely results
 *       in less memory utilization. However, sharing also requires connections
 *       to obtain a lock, possibly making database access slower. Defaults to
 *       true.
 *
 *   shrinkMemoryOnConnectionIdleMS -- (integer) If defined, the connection
 *       will attempt to minimize its memory usage after this many
 *       milliseconds of connection idle. The connection is idle when no
 *       statements are executing. There is no default value which means no
 *       automatic memory minimization will occur. Please note that this is
 *       *not* a timer on the idle service and this could fire while the
 *       application is active.
 *
 *   readOnly -- (bool) Whether to open the database with SQLITE_OPEN_READONLY
 *       set. If used, writing to the database will fail. Defaults to false.
 *
 *   ignoreLockingMode -- (bool) Whether to ignore locks on the database held
 *       by other connections. If used, implies readOnly. Defaults to false.
 *       USE WITH EXTREME CAUTION. This mode WILL produce incorrect results or
 *       return "false positive" corruption errors if other connections write
 *       to the DB at the same time.
 *
 * FUTURE options to control:
 *
 *   special named databases
 *   pragma TEMP STORE = MEMORY
 *   TRUNCATE JOURNAL
 *   SYNCHRONOUS = full
 *
 * @param options
 *        (Object) Parameters to control connection and open options.
 *
 * @return Promise<OpenedConnection>
 */
function openConnection(options) {
  let log = Log.repository.getLoggerWithMessagePrefix(
    "Sqlite.jsm",
    `ConnectionOpener: `
  );
  log.manageLevelFromPref("toolkit.sqlitejsm.loglevel");

  if (!options.path) {
    throw new Error("path not specified in connection options.");
  }

  if (isClosed) {
    throw new Error(
      "Sqlite.jsm has been shutdown. Cannot open connection to: " + options.path
    );
  }

  // Retains absolute paths and normalizes relative as relative to profile.
  let path = options.path;
  let file;
  try {
    file = FileUtils.File(path);
  } catch (ex) {
    // For relative paths, we will get an exception from trying to initialize
    // the file. We must then join this path to the profile directory.
    if (ex.result == Cr.NS_ERROR_FILE_UNRECOGNIZED_PATH) {
      path = PathUtils.joinRelative(
        Services.dirsvc.get("ProfD", Ci.nsIFile).path,
        options.path
      );
      file = FileUtils.File(path);
    } else {
      throw ex;
    }
  }

  let sharedMemoryCache =
    "sharedMemoryCache" in options ? options.sharedMemoryCache : true;

  let openedOptions = {};

  if ("shrinkMemoryOnConnectionIdleMS" in options) {
    if (!Number.isInteger(options.shrinkMemoryOnConnectionIdleMS)) {
      throw new Error(
        "shrinkMemoryOnConnectionIdleMS must be an integer. " +
          "Got: " +
          options.shrinkMemoryOnConnectionIdleMS
      );
    }

    openedOptions.shrinkMemoryOnConnectionIdleMS =
      options.shrinkMemoryOnConnectionIdleMS;
  }

  if ("defaultTransactionType" in options) {
    let defaultTransactionType = options.defaultTransactionType;
    if (!OpenedConnection.TRANSACTION_TYPES.includes(defaultTransactionType)) {
      throw new Error(
        "Unknown default transaction type: " + defaultTransactionType
      );
    }

    openedOptions.defaultTransactionType = defaultTransactionType;
  }

  let identifier = getIdentifierByFileName(PathUtils.filename(path));

  log.debug("Opening database: " + path + " (" + identifier + ")");

  return new Promise((resolve, reject) => {
    let dbOptions = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
      Ci.nsIWritablePropertyBag
    );
    if (!sharedMemoryCache) {
      dbOptions.setProperty("shared", false);
    }
    if (options.readOnly) {
      dbOptions.setProperty("readOnly", true);
    }
    if (options.ignoreLockingMode) {
      dbOptions.setProperty("ignoreLockingMode", true);
      dbOptions.setProperty("readOnly", true);
    }

    dbOptions = dbOptions.enumerator.hasMoreElements() ? dbOptions : null;

    Services.storage.openAsyncDatabase(
      file,
      dbOptions,
      (status, connection) => {
        if (!connection) {
          log.error(`Could not open connection to ${path}: ${status}`);
          let error = new Components.Exception(
            `Could not open connection to ${path}: ${status}`,
            status
          );
          reject(error);
          return;
        }
        log.debug("Connection opened");
        try {
          resolve(
            new OpenedConnection(
              connection.QueryInterface(Ci.mozIStorageAsyncConnection),
              identifier,
              openedOptions
            )
          );
        } catch (ex) {
          log.error("Could not open database", ex);
          connection.asyncClose();
          reject(ex);
        }
      }
    );
  });
}

/**
 * Creates a clone of an existing and open Storage connection.  The clone has
 * the same underlying characteristics of the original connection and is
 * returned in form of an OpenedConnection handle.
 *
 * The following parameters can control the cloned connection:
 *
 *   connection -- (mozIStorageAsyncConnection) The original Storage connection
 *       to clone.  It's not possible to clone connections to memory databases.
 *
 *   readOnly -- (boolean) - If true the clone will be read-only.  If the
 *       original connection is already read-only, the clone will be, regardless
 *       of this option.  If the original connection is using the shared cache,
 *       this parameter will be ignored and the clone will be as privileged as
 *       the original connection.
 *   shrinkMemoryOnConnectionIdleMS -- (integer) If defined, the connection
 *       will attempt to minimize its memory usage after this many
 *       milliseconds of connection idle. The connection is idle when no
 *       statements are executing. There is no default value which means no
 *       automatic memory minimization will occur. Please note that this is
 *       *not* a timer on the idle service and this could fire while the
 *       application is active.
 *
 *
 * @param options
 *        (Object) Parameters to control connection and clone options.
 *
 * @return Promise<OpenedConnection>
 */
function cloneStorageConnection(options) {
  let log = Log.repository.getLoggerWithMessagePrefix(
    "Sqlite.jsm",
    `ConnectionCloner: `
  );
  log.manageLevelFromPref("toolkit.sqlitejsm.loglevel");

  let source = options && options.connection;
  if (!source) {
    throw new TypeError("connection not specified in clone options.");
  }
  if (!(source instanceof Ci.mozIStorageAsyncConnection)) {
    throw new TypeError("Connection must be a valid Storage connection.");
  }

  if (isClosed) {
    throw new Error(
      "Sqlite.jsm has been shutdown. Cannot clone connection to: " +
        source.databaseFile.path
    );
  }

  let openedOptions = {};

  if ("shrinkMemoryOnConnectionIdleMS" in options) {
    if (!Number.isInteger(options.shrinkMemoryOnConnectionIdleMS)) {
      throw new TypeError(
        "shrinkMemoryOnConnectionIdleMS must be an integer. " +
          "Got: " +
          options.shrinkMemoryOnConnectionIdleMS
      );
    }
    openedOptions.shrinkMemoryOnConnectionIdleMS =
      options.shrinkMemoryOnConnectionIdleMS;
  }

  let path = source.databaseFile.path;
  let identifier = getIdentifierByFileName(PathUtils.filename(path));

  log.debug("Cloning database: " + path + " (" + identifier + ")");

  return new Promise((resolve, reject) => {
    source.asyncClone(!!options.readOnly, (status, connection) => {
      if (!connection) {
        log.error("Could not clone connection: " + status);
        reject(new Error("Could not clone connection: " + status));
        return;
      }
      log.debug("Connection cloned");
      try {
        let conn = connection.QueryInterface(Ci.mozIStorageAsyncConnection);
        resolve(new OpenedConnection(conn, identifier, openedOptions));
      } catch (ex) {
        log.error("Could not clone database", ex);
        connection.asyncClose();
        reject(ex);
      }
    });
  });
}

/**
 * Wraps an existing and open Storage connection with Sqlite.jsm API.  The
 * wrapped connection clone has the same underlying characteristics of the
 * original connection and is returned in form of an OpenedConnection handle.
 *
 * Clients are responsible for closing both the Sqlite.jsm wrapper and the
 * underlying mozStorage connection.
 *
 * The following parameters can control the wrapped connection:
 *
 *   connection -- (mozIStorageAsyncConnection) The original Storage connection
 *       to wrap.
 *
 * @param options
 *        (Object) Parameters to control connection and wrap options.
 *
 * @return Promise<OpenedConnection>
 */
function wrapStorageConnection(options) {
  let log = Log.repository.getLoggerWithMessagePrefix(
    "Sqlite.jsm",
    `ConnectionCloner: `
  );
  log.manageLevelFromPref("toolkit.sqlitejsm.loglevel");

  let connection = options && options.connection;
  if (!connection || !(connection instanceof Ci.mozIStorageAsyncConnection)) {
    throw new TypeError("connection not specified or invalid.");
  }

  if (isClosed) {
    throw new Error(
      "Sqlite.jsm has been shutdown. Cannot wrap connection to: " +
        connection.databaseFile.path
    );
  }

  let identifier = getIdentifierByFileName(connection.databaseFile.leafName);

  log.debug("Wrapping database: " + identifier);
  return new Promise(resolve => {
    try {
      let conn = connection.QueryInterface(Ci.mozIStorageAsyncConnection);
      let wrapper = new OpenedConnection(conn, identifier);
      // We must not handle shutdown of a wrapped connection, since that is
      // already handled by the opener.
      wrappedConnections.add(identifier);
      resolve(wrapper);
    } catch (ex) {
      log.error("Could not wrap database", ex);
      throw ex;
    }
  });
}

/**
 * Handle on an opened SQLite database.
 *
 * This is essentially a glorified wrapper around mozIStorageConnection.
 * However, it offers some compelling advantages.
 *
 * The main functions on this type are `execute` and `executeCached`. These are
 * ultimately how all SQL statements are executed. It's worth explaining their
 * differences.
 *
 * `execute` is used to execute one-shot SQL statements. These are SQL
 * statements that are executed one time and then thrown away. They are useful
 * for dynamically generated SQL statements and clients who don't care about
 * performance (either their own or wasting resources in the overall
 * application). Because of the performance considerations, it is recommended
 * to avoid `execute` unless the statement you are executing will only be
 * executed once or seldomly.
 *
 * `executeCached` is used to execute a statement that will presumably be
 * executed multiple times. The statement is parsed once and stuffed away
 * inside the connection instance. Subsequent calls to `executeCached` will not
 * incur the overhead of creating a new statement object. This should be used
 * in preference to `execute` when a specific SQL statement will be executed
 * multiple times.
 *
 * Instances of this type are not meant to be created outside of this file.
 * Instead, first open an instance of `UnopenedSqliteConnection` and obtain
 * an instance of this type by calling `open`.
 *
 * FUTURE IMPROVEMENTS
 *
 *   Ability to enqueue operations. Currently there can be race conditions,
 *   especially as far as transactions are concerned. It would be nice to have
 *   an enqueueOperation(func) API that serially executes passed functions.
 *
 *   Support for SAVEPOINT (named/nested transactions) might be useful.
 *
 * @param connection
 *        (mozIStorageConnection) Underlying SQLite connection.
 * @param identifier
 *        (string) The unique identifier of this database. It may be used for
 *        logging or as a key in Maps.
 * @param options [optional]
 *        (object) Options to control behavior of connection. See
 *        `openConnection`.
 */
function OpenedConnection(connection, identifier, options = {}) {
  // Store all connection data in a field distinct from the
  // witness. This enables us to store an additional reference to this
  // field without preventing garbage collection of
  // OpenedConnection. On garbage collection, we will still be able to
  // close the database using this extra reference.
  this._connectionData = new ConnectionData(connection, identifier, options);

  // Store the extra reference in a map with connection identifier as
  // key.
  ConnectionData.byId.set(
    this._connectionData._identifier,
    this._connectionData
  );

  // Make a finalization witness. If this object is garbage collected
  // before its `forget` method has been called, an event with topic
  // "sqlite-finalization-witness" is broadcasted along with the
  // connection identifier string of the database.
  this._witness = FinalizationWitnessService.make(
    "sqlite-finalization-witness",
    this._connectionData._identifier
  );
}

OpenedConnection.TRANSACTION_TYPES = ["DEFERRED", "IMMEDIATE", "EXCLUSIVE"];

// Converts a `mozIStorageAsyncConnection::TRANSACTION_*` constant into the
// corresponding `OpenedConnection.TRANSACTION_TYPES` constant.
function convertStorageTransactionType(type) {
  if (!(type in OpenedConnection.TRANSACTION_TYPES)) {
    throw new Error("Unknown storage transaction type: " + type);
  }
  return OpenedConnection.TRANSACTION_TYPES[type];
}

OpenedConnection.prototype = Object.freeze({
  TRANSACTION_DEFAULT: "DEFAULT",
  TRANSACTION_DEFERRED: "DEFERRED",
  TRANSACTION_IMMEDIATE: "IMMEDIATE",
  TRANSACTION_EXCLUSIVE: "EXCLUSIVE",

  /**
   * Returns a handle to the underlying `mozIStorageAsyncConnection`. This is
   * ⚠️ **extremely unsafe** ⚠️ because `Sqlite.jsm` continues to manage the
   * connection's lifecycle, including transactions and shutdown blockers.
   * Misusing the raw connection can easily lead to data loss, memory leaks,
   * and errors.
   *
   * Consumers of the raw connection **must not** close or re-wrap it,
   * and should not run statements concurrently with `Sqlite.jsm`.
   *
   * It's _much_ safer to open a `mozIStorage{Async}Connection` yourself,
   * and access it from JavaScript via `Sqlite.wrapStorageConnection`.
   * `unsafeRawConnection` is an escape hatch for cases where you can't
   * do that.
   *
   * Please do _not_ add new uses of `unsafeRawConnection` without review
   * from a storage peer.
   */
  get unsafeRawConnection() {
    return this._connectionData._dbConn;
  },

  /**
   * Returns the maximum number of bound parameters for statements executed
   * on this connection.
   *
   * @type {number}
   */
  get variableLimit() {
    return this.unsafeRawConnection.variableLimit;
  },

  /**
   * The integer schema version of the database.
   *
   * This is 0 if not schema version has been set.
   *
   * @return Promise<int>
   */
  getSchemaVersion(schemaName = "main") {
    return this.execute(`PRAGMA ${schemaName}.user_version`).then(result =>
      result[0].getInt32(0)
    );
  },

  setSchemaVersion(value, schemaName = "main") {
    if (!Number.isInteger(value)) {
      // Guarding against accidental SQLi
      throw new TypeError("Schema version must be an integer. Got " + value);
    }
    this._connectionData.ensureOpen();
    return this.execute(`PRAGMA ${schemaName}.user_version = ${value}`);
  },

  /**
   * Close the database connection.
   *
   * This must be performed when you are finished with the database.
   *
   * Closing the database connection has the side effect of forcefully
   * cancelling all active statements. Therefore, callers should ensure that
   * all active statements have completed before closing the connection, if
   * possible.
   *
   * The returned promise will be resolved once the connection is closed.
   * Successive calls to close() return the same promise.
   *
   * IMPROVEMENT: Resolve the promise to a closed connection which can be
   * reopened.
   *
   * @return Promise<>
   */
  close() {
    // Unless cleanup has already been done by a previous call to
    // `close`, delete the database entry from map and tell the
    // finalization witness to forget.
    if (ConnectionData.byId.has(this._connectionData._identifier)) {
      ConnectionData.byId.delete(this._connectionData._identifier);
      this._witness.forget();
    }
    return this._connectionData.close();
  },

  /**
   * Clones this connection to a new Sqlite one.
   *
   * The following parameters can control the cloned connection:
   *
   * @param readOnly
   *        (boolean) - If true the clone will be read-only.  If the original
   *        connection is already read-only, the clone will be, regardless of
   *        this option.  If the original connection is using the shared cache,
   *        this parameter will be ignored and the clone will be as privileged as
   *        the original connection.
   *
   * @return Promise<OpenedConnection>
   */
  clone(readOnly = false) {
    return this._connectionData.clone(readOnly);
  },

  executeBeforeShutdown(name, task) {
    return this._connectionData.executeBeforeShutdown(this, name, task);
  },

  /**
   * Execute a SQL statement and cache the underlying statement object.
   *
   * This function executes a SQL statement and also caches the underlying
   * derived statement object so subsequent executions are faster and use
   * less resources.
   *
   * This function optionally binds parameters to the statement as well as
   * optionally invokes a callback for every row retrieved.
   *
   * By default, no parameters are bound and no callback will be invoked for
   * every row.
   *
   * Bound parameters can be defined as an Array of positional arguments or
   * an object mapping named parameters to their values. If there are no bound
   * parameters, the caller can pass nothing or null for this argument.
   *
   * Callers are encouraged to pass objects rather than Arrays for bound
   * parameters because they prevent foot guns. With positional arguments, it
   * is simple to modify the parameter count or positions without fixing all
   * users of the statement. Objects/named parameters are a little safer
   * because changes in order alone won't result in bad things happening.
   *
   * When `onRow` is not specified, all returned rows are buffered before the
   * returned promise is resolved. For INSERT or UPDATE statements, this has
   * no effect because no rows are returned from these. However, it has
   * implications for SELECT statements.
   *
   * If your SELECT statement could return many rows or rows with large amounts
   * of data, for performance reasons it is recommended to pass an `onRow`
   * handler. Otherwise, the buffering may consume unacceptable amounts of
   * resources.
   *
   * If the second parameter of an `onRow` handler is called during execution
   * of the `onRow` handler, the execution of the statement is immediately
   * cancelled. Subsequent rows will not be processed and no more `onRow`
   * invocations will be made. The promise is resolved immediately.
   *
   * If an exception is thrown by the `onRow` handler, the exception is logged
   * and processing of subsequent rows occurs as if nothing happened. The
   * promise is still resolved (not rejected).
   *
   * The return value is a promise that will be resolved when the statement
   * has completed fully.
   *
   * The promise will be rejected with an `Error` instance if the statement
   * did not finish execution fully. The `Error` may have an `errors` property.
   * If defined, it will be an Array of objects describing individual errors.
   * Each object has the properties `result` and `message`. `result` is a
   * numeric error code and `message` is a string description of the problem.
   *
   * @param name
   *        (string) The name of the registered statement to execute.
   * @param params optional
   *        (Array or object) Parameters to bind.
   * @param onRow optional
   *        (function) Callback to receive each row from result.
   */
  executeCached(sql, params = null, onRow = null) {
    if (isInvalidBoundLikeQuery(sql)) {
      throw new Error("Please enter a LIKE clause with bindings");
    }
    return this._connectionData.executeCached(sql, params, onRow);
  },

  /**
   * Execute a one-shot SQL statement.
   *
   * If you find yourself feeding the same SQL string in this function, you
   * should *not* use this function and instead use `executeCached`.
   *
   * See `executeCached` for the meaning of the arguments and extended usage info.
   *
   * @param sql
   *        (string) SQL to execute.
   * @param params optional
   *        (Array or Object) Parameters to bind to the statement.
   * @param onRow optional
   *        (function) Callback to receive result of a single row.
   */
  execute(sql, params = null, onRow = null) {
    if (isInvalidBoundLikeQuery(sql)) {
      throw new Error("Please enter a LIKE clause with bindings");
    }
    return this._connectionData.execute(sql, params, onRow);
  },

  /**
   * The default behavior for transactions run on this connection.
   */
  get defaultTransactionType() {
    return this._connectionData.defaultTransactionType;
  },

  /**
   * Whether a transaction is currently in progress.
   *
   * Note that this is true if a transaction is active on the connection,
   * regardless of whether it was started by `Sqlite.jsm` or another consumer.
   * See the explanation above `mozIStorageConnection.transactionInProgress` for
   * why this distinction matters.
   */
  get transactionInProgress() {
    return this._connectionData.transactionInProgress;
  },

  /**
   * Perform a transaction.
   *
   * *****************************************************************************
   * YOU SHOULD _NEVER_ NEST executeTransaction CALLS FOR ANY REASON, NOR
   * DIRECTLY, NOR THROUGH OTHER PROMISES.
   * FOR EXAMPLE, NEVER DO SOMETHING LIKE:
   *   await executeTransaction(async function () {
   *     ...some_code...
   *     await executeTransaction(async function () { // WRONG!
   *       ...some_code...
   *     })
   *     await someCodeThatExecuteTransaction(); // WRONG!
   *     await neverResolvedPromise; // WRONG!
   *   });
   * NESTING CALLS WILL BLOCK ANY FUTURE TRANSACTION UNTIL A TIMEOUT KICKS IN.
   * *****************************************************************************
   *
   * A transaction is specified by a user-supplied function that is an
   * async function. The function receives this connection instance as its argument.
   *
   * The supplied function is expected to return promises. These are often
   * promises created by calling `execute` and `executeCached`. If the
   * generator is exhausted without any errors being thrown, the
   * transaction is committed. If an error occurs, the transaction is
   * rolled back.
   *
   * The returned value from this function is a promise that will be resolved
   * once the transaction has been committed or rolled back. The promise will
   * be resolved to whatever value the supplied function resolves to. If
   * the transaction is rolled back, the promise is rejected.
   *
   * @param func
   *        (function) What to perform as part of the transaction.
   * @param type optional
   *        One of the TRANSACTION_* constants attached to this type.
   */
  executeTransaction(func, type = this.TRANSACTION_DEFAULT) {
    return this._connectionData.executeTransaction(() => func(this), type);
  },

  /**
   * Whether a table exists in the database (both persistent and temporary tables).
   *
   * @param name
   *        (string) Name of the table.
   *
   * @return Promise<bool>
   */
  tableExists(name) {
    return this.execute(
      "SELECT name FROM (SELECT * FROM sqlite_master UNION ALL " +
        "SELECT * FROM sqlite_temp_master) " +
        "WHERE type = 'table' AND name=?",
      [name]
    ).then(function onResult(rows) {
      return Promise.resolve(!!rows.length);
    });
  },

  /**
   * Whether a named index exists (both persistent and temporary tables).
   *
   * @param name
   *        (string) Name of the index.
   *
   * @return Promise<bool>
   */
  indexExists(name) {
    return this.execute(
      "SELECT name FROM (SELECT * FROM sqlite_master UNION ALL " +
        "SELECT * FROM sqlite_temp_master) " +
        "WHERE type = 'index' AND name=?",
      [name]
    ).then(function onResult(rows) {
      return Promise.resolve(!!rows.length);
    });
  },

  /**
   * Free up as much memory from the underlying database connection as possible.
   *
   * @return Promise<>
   */
  shrinkMemory() {
    return this._connectionData.shrinkMemory();
  },

  /**
   * Discard all cached statements.
   *
   * Note that this relies on us being non-interruptible between
   * the insertion or retrieval of a statement in the cache and its
   * execution: we finalize all statements, which is only safe if
   * they will not be executed again.
   *
   * @return (integer) the number of statements discarded.
   */
  discardCachedStatements() {
    return this._connectionData.discardCachedStatements();
  },

  /**
   * Interrupts pending database operations returning at the first opportunity.
   * Statement execution will throw an NS_ERROR_ABORT failure.
   * Can only be used on read-only connections.
   */
  interrupt() {
    this._connectionData.interrupt();
  },
});

var Sqlite = {
  // The maximum time to wait before considering a transaction stuck and
  // issuing a ROLLBACK, see `executeTransaction`. Could be modified by tests.
  TRANSACTIONS_TIMEOUT_MS: 300000, // 5 minutes

  openConnection,
  cloneStorageConnection,
  wrapStorageConnection,
  /**
   * Shutdown barrier client. May be used by clients to perform last-minute
   * cleanup prior to the shutdown of this module.
   *
   * See the documentation of AsyncShutdown.Barrier.prototype.client.
   */
  get shutdown() {
    return Barriers.shutdown.client;
  },
  failTestsOnAutoClose(enabled) {
    Debugging.failTestsOnAutoClose = enabled;
  },
};
