/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "Sqlite",
];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);

XPCOMUtils.defineLazyModuleGetter(this, "AsyncShutdown",
                                  "resource://gre/modules/AsyncShutdown.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Log",
                                  "resource://gre/modules/Log.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CommonUtils",
                                  "resource://services-common/utils.js");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");


// Counts the number of created connections per database basename(). This is
// used for logging to distinguish connection instances.
let connectionCounters = new Map();

/**
 * Once `true`, reject any attempt to open or close a database.
 */
let isClosed = false;

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
    shutdown: new AsyncShutdown.Barrier("Sqlite.jsm: wait until all clients have completed their task"),

    /**
     * Private barrier blocked by connections that are still open.
     * Triggered after Barriers.shutdown is lifted and `isClosed` is
     * set to `true`.
     */
    connections: new AsyncShutdown.Barrier("Sqlite.jsm: wait until all connections are closed"),
  };

  /**
   * Ensure that Sqlite.jsm:
   * - informs its clients before shutting down;
   * - lets clients open connections during shutdown, if necessary;
   * - waits for all connections to be closed before shutdown.
   */
  AsyncShutdown.profileBeforeChange.addBlocker("Sqlite.jsm shutdown blocker",
    Task.async(function* () {
      yield Barriers.shutdown.wait();
      // At this stage, all clients have had a chance to open (and close)
      // their databases. Some previous close operations may still be pending,
      // so we need to wait until they are complete before proceeding.

      // Prevent any new opening.
      isClosed = true;

      // Now, wait until all databases are closed
      yield Barriers.connections.wait();
    }),

    function status() {
      if (isClosed) {
        // We are waiting for the connections to close. The interesting
        // status is therefore the list of connections still pending.
        return { description: "Waiting for connections to close",
                 status: Barriers.connections.status };
      }

      // We are still in the first stage: waiting for the barrier
      // to be lifted. The interesting status is therefore that of
      // the barrier.
      return { description: "Waiting for the barrier to be lifted",
               status: Barriers.shutdown.status };
  });

  return Barriers;
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
  let log = Log.repository.getLogger("Sqlite.ConnectionOpener");

  if (!options.path) {
    throw new Error("path not specified in connection options.");
  }

  if (isClosed) {
    throw new Error("Sqlite.jsm has been shutdown. Cannot open connection to: " + options.path);
  }


  // Retains absolute paths and normalizes relative as relative to profile.
  let path = OS.Path.join(OS.Constants.Path.profileDir, options.path);

  let sharedMemoryCache = "sharedMemoryCache" in options ?
                            options.sharedMemoryCache : true;

  let openedOptions = {};

  if ("shrinkMemoryOnConnectionIdleMS" in options) {
    if (!Number.isInteger(options.shrinkMemoryOnConnectionIdleMS)) {
      throw new Error("shrinkMemoryOnConnectionIdleMS must be an integer. " +
                      "Got: " + options.shrinkMemoryOnConnectionIdleMS);
    }

    openedOptions.shrinkMemoryOnConnectionIdleMS =
      options.shrinkMemoryOnConnectionIdleMS;
  }

  let file = FileUtils.File(path);

  let basename = OS.Path.basename(path);
  let number = connectionCounters.get(basename) || 0;
  connectionCounters.set(basename, number + 1);

  let identifier = basename + "#" + number;

  log.info("Opening database: " + path + " (" + identifier + ")");
  let deferred = Promise.defer();
  let options = null;
  if (!sharedMemoryCache) {
    options = Cc["@mozilla.org/hash-property-bag;1"].
      createInstance(Ci.nsIWritablePropertyBag);
    options.setProperty("shared", false);
  }
  Services.storage.openAsyncDatabase(file, options, function(status, connection) {
    if (!connection) {
      log.warn("Could not open connection: " + status);
      deferred.reject(new Error("Could not open connection: " + status));
      return;
    }
    log.info("Connection opened");
    try {
      deferred.resolve(
        new OpenedConnection(connection.QueryInterface(Ci.mozIStorageAsyncConnection), basename, number,
        openedOptions));
    } catch (ex) {
      log.warn("Could not open database: " + CommonUtils.exceptionStr(ex));
      deferred.reject(ex);
    }
  });
  return deferred.promise;
}

/**
 * Creates a clone of an existing and open Storage connection.  The clone has
 * the same underlying characteristics of the original connection and is
 * returned in form of on OpenedConnection handle.
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
  let log = Log.repository.getLogger("Sqlite.ConnectionCloner");

  let source = options && options.connection;
  if (!source) {
    throw new TypeError("connection not specified in clone options.");
  }
  if (!source instanceof Ci.mozIStorageAsyncConnection) {
    throw new TypeError("Connection must be a valid Storage connection.");
  }

  if (isClosed) {
    throw new Error("Sqlite.jsm has been shutdown. Cannot close connection to: " + source.database.path);
  }

  let openedOptions = {};

  if ("shrinkMemoryOnConnectionIdleMS" in options) {
    if (!Number.isInteger(options.shrinkMemoryOnConnectionIdleMS)) {
      throw new TypeError("shrinkMemoryOnConnectionIdleMS must be an integer. " +
                          "Got: " + options.shrinkMemoryOnConnectionIdleMS);
    }
    openedOptions.shrinkMemoryOnConnectionIdleMS =
      options.shrinkMemoryOnConnectionIdleMS;
  }

  let path = source.databaseFile.path;
  let basename = OS.Path.basename(path);
  let number = connectionCounters.get(basename) || 0;
  connectionCounters.set(basename, number + 1);
  let identifier = basename + "#" + number;

  log.info("Cloning database: " + path + " (" + identifier + ")");
  let deferred = Promise.defer();

  source.asyncClone(!!options.readOnly, (status, connection) => {
    if (!connection) {
      log.warn("Could not clone connection: " + status);
      deferred.reject(new Error("Could not clone connection: " + status));
    }
    log.info("Connection cloned");
    try {
      let conn = connection.QueryInterface(Ci.mozIStorageAsyncConnection);
      deferred.resolve(new OpenedConnection(conn, basename, number,
                                            openedOptions));
    } catch (ex) {
      log.warn("Could not clone database: " + CommonUtils.exceptionStr(ex));
      deferred.reject(ex);
    }
  });
  return deferred.promise;
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
 * @param basename
 *        (string) The basename of this database name. Used for logging.
 * @param number
 *        (Number) The connection number to this database.
 * @param options
 *        (object) Options to control behavior of connection. See
 *        `openConnection`.
 */
function OpenedConnection(connection, basename, number, options) {
  this._log = Log.repository.getLoggerWithMessagePrefix("Sqlite.Connection." + basename,
                                                        "Conn #" + number + ": ");

  this._log.info("Opened");

  this._connection = connection;
  this._connectionIdentifier = basename + " Conn #" + number;
  this._open = true;

  this._cachedStatements = new Map();
  this._anonymousStatements = new Map();
  this._anonymousCounter = 0;

  // A map from statement index to mozIStoragePendingStatement, to allow for
  // canceling prior to finalizing the mozIStorageStatements.
  this._pendingStatements = new Map();

  // Increments for each executed statement for the life of the connection.
  this._statementCounter = 0;

  this._inProgressTransaction = null;

  this._idleShrinkMS = options.shrinkMemoryOnConnectionIdleMS;
  if (this._idleShrinkMS) {
    this._idleShrinkTimer = Cc["@mozilla.org/timer;1"]
                              .createInstance(Ci.nsITimer);
    // We wait for the first statement execute to start the timer because
    // shrinking now would not do anything.
  }

  this._deferredClose = Promise.defer();
  this._closeRequested = false;

  Barriers.connections.client.addBlocker(
    this._connectionIdentifier + ": waiting for shutdown",
    this._deferredClose.promise,
    () =>  ({
      identifier: this._connectionIdentifier,
      isCloseRequested: this._closeRequested,
      hasDbConn: !!this._dbConn,
      hasInProgressTransaction: !!this._inProgressTransaction,
      pendingStatements: this._pendingStatements.size,
      statementCounter: this._statementCounter,
    })
  );
}

OpenedConnection.prototype = Object.freeze({
  TRANSACTION_DEFERRED: "DEFERRED",
  TRANSACTION_IMMEDIATE: "IMMEDIATE",
  TRANSACTION_EXCLUSIVE: "EXCLUSIVE",

  TRANSACTION_TYPES: ["DEFERRED", "IMMEDIATE", "EXCLUSIVE"],

  /**
   * The integer schema version of the database.
   *
   * This is 0 if not schema version has been set.
   *
   * @return Promise<int>
   */
  getSchemaVersion: function() {
    let self = this;
    return this.execute("PRAGMA user_version").then(
      function onSuccess(result) {
        if (result == null) {
          return 0;
        }
        return JSON.stringify(result[0].getInt32(0));
      }
    );
  },

  setSchemaVersion: function(value) {
    if (!Number.isInteger(value)) {
      // Guarding against accidental SQLi
      throw new TypeError("Schema version must be an integer. Got " + value);
    }
    this._ensureOpen();
    return this.execute("PRAGMA user_version = " + value);
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
  close: function () {
    this._closeRequested = true;

    if (!this._connection) {
      return this._deferredClose.promise;
    }

    this._log.debug("Request to close connection.");
    this._clearIdleShrinkTimer();

    // We need to take extra care with transactions during shutdown.
    //
    // If we don't have a transaction in progress, we can proceed with shutdown
    // immediately.
    if (!this._inProgressTransaction) {
      this._finalize(this._deferredClose);
      return this._deferredClose.promise;
    }

    // Else if we do have a transaction in progress, we forcefully roll it
    // back. This is an async task, so we wait on it to finish before
    // performing finalization.
    this._log.warn("Transaction in progress at time of close. Rolling back.");

    let onRollback = this._finalize.bind(this, this._deferredClose);

    this.execute("ROLLBACK TRANSACTION").then(onRollback, onRollback);
    this._inProgressTransaction.reject(new Error("Connection being closed."));
    this._inProgressTransaction = null;

    return this._deferredClose.promise;
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
  clone: function (readOnly=false) {
    this._ensureOpen();

    this._log.debug("Request to clone connection.");

    let options = {
      connection: this._connection,
      readOnly: readOnly,
    };
    if (this._idleShrinkMS)
      options.shrinkMemoryOnConnectionIdleMS = this._idleShrinkMS;

    return cloneStorageConnection(options);
  },

  _finalize: function (deferred) {
    this._log.debug("Finalizing connection.");
    // Cancel any pending statements.
    for (let [k, statement] of this._pendingStatements) {
      statement.cancel();
    }
    this._pendingStatements.clear();

    // We no longer need to track these.
    this._statementCounter = 0;

    // Next we finalize all active statements.
    for (let [k, statement] of this._anonymousStatements) {
      statement.finalize();
    }
    this._anonymousStatements.clear();

    for (let [k, statement] of this._cachedStatements) {
      statement.finalize();
    }
    this._cachedStatements.clear();

    // This guards against operations performed between the call to this
    // function and asyncClose() finishing. See also bug 726990.
    this._open = false;

    this._log.debug("Calling asyncClose().");
    this._connection.asyncClose({
      complete: function () {
        this._log.info("Closed");
        this._connection = null;
        // Now that the connection is closed, no need to keep
        // a blocker for Barriers.connections.
        Barriers.connections.client.removeBlocker(deferred.promise);
        deferred.resolve();
      }.bind(this),
    });
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
   * If a `StopIteration` is thrown during execution of an `onRow` handler,
   * the execution of the statement is immediately cancelled. Subsequent
   * rows will not be processed and no more `onRow` invocations will be made.
   * The promise is resolved immediately.
   *
   * If a non-`StopIteration` exception is thrown by the `onRow` handler, the
   * exception is logged and processing of subsequent rows occurs as if nothing
   * happened. The promise is still resolved (not rejected).
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
  executeCached: function (sql, params=null, onRow=null) {
    this._ensureOpen();

    if (!sql) {
      throw new Error("sql argument is empty.");
    }

    let statement = this._cachedStatements.get(sql);
    if (!statement) {
      statement = this._connection.createAsyncStatement(sql);
      this._cachedStatements.set(sql, statement);
    }

    this._clearIdleShrinkTimer();

    let deferred = Promise.defer();

    try {
      this._executeStatement(sql, statement, params, onRow).then(
        function onResult(result) {
          this._startIdleShrinkTimer();
          deferred.resolve(result);
        }.bind(this),
        function onError(error) {
          this._startIdleShrinkTimer();
          deferred.reject(error);
        }.bind(this)
      );
    } catch (ex) {
      this._startIdleShrinkTimer();
      throw ex;
    }

    return deferred.promise;
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
  execute: function (sql, params=null, onRow=null) {
    if (typeof(sql) != "string") {
      throw new Error("Must define SQL to execute as a string: " + sql);
    }

    this._ensureOpen();

    let statement = this._connection.createAsyncStatement(sql);
    let index = this._anonymousCounter++;

    this._anonymousStatements.set(index, statement);
    this._clearIdleShrinkTimer();

    let onFinished = function () {
      this._anonymousStatements.delete(index);
      statement.finalize();
      this._startIdleShrinkTimer();
    }.bind(this);

    let deferred = Promise.defer();

    try {
      this._executeStatement(sql, statement, params, onRow).then(
        function onResult(rows) {
          onFinished();
          deferred.resolve(rows);
        }.bind(this),

        function onError(error) {
          onFinished();
          deferred.reject(error);
        }.bind(this)
      );
    } catch (ex) {
      onFinished();
      throw ex;
    }

    return deferred.promise;
  },

  /**
   * Whether a transaction is currently in progress.
   */
  get transactionInProgress() {
    return this._open && !!this._inProgressTransaction;
  },

  /**
   * Perform a transaction.
   *
   * A transaction is specified by a user-supplied function that is a
   * generator function which can be used by Task.jsm's Task.spawn(). The
   * function receives this connection instance as its argument.
   *
   * The supplied function is expected to yield promises. These are often
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
  executeTransaction: function (func, type=this.TRANSACTION_DEFERRED) {
    if (this.TRANSACTION_TYPES.indexOf(type) == -1) {
      throw new Error("Unknown transaction type: " + type);
    }

    this._ensureOpen();

    if (this._inProgressTransaction) {
      throw new Error("A transaction is already active. Only one transaction " +
                      "can be active at a time.");
    }

    this._log.debug("Beginning transaction");
    let deferred = Promise.defer();
    this._inProgressTransaction = deferred;
    Task.spawn(function doTransaction() {
      // It's tempting to not yield here and rely on the implicit serial
      // execution of issued statements. However, the yield serves an important
      // purpose: catching errors in statement execution.
      yield this.execute("BEGIN " + type + " TRANSACTION");

      let result;
      try {
        result = yield Task.spawn(func(this));
      } catch (ex) {
        // It's possible that a request to close the connection caused the
        // error.
        // Assertion: close() will unset this._inProgressTransaction when
        // called.
        if (!this._inProgressTransaction) {
          this._log.warn("Connection was closed while performing transaction. " +
                         "Received error should be due to closed connection: " +
                         CommonUtils.exceptionStr(ex));
          throw ex;
        }

        this._log.warn("Error during transaction. Rolling back: " +
                       CommonUtils.exceptionStr(ex));
        try {
          yield this.execute("ROLLBACK TRANSACTION");
        } catch (inner) {
          this._log.warn("Could not roll back transaction. This is weird: " +
                         CommonUtils.exceptionStr(inner));
        }

        throw ex;
      }

      // See comment above about connection being closed during transaction.
      if (!this._inProgressTransaction) {
        this._log.warn("Connection was closed while performing transaction. " +
                       "Unable to commit.");
        throw new Error("Connection closed before transaction committed.");
      }

      try {
        yield this.execute("COMMIT TRANSACTION");
      } catch (ex) {
        this._log.warn("Error committing transaction: " +
                       CommonUtils.exceptionStr(ex));
        throw ex;
      }

      throw new Task.Result(result);
    }.bind(this)).then(
      function onSuccess(result) {
        this._inProgressTransaction = null;
        deferred.resolve(result);
      }.bind(this),
      function onError(error) {
        this._inProgressTransaction = null;
        deferred.reject(error);
      }.bind(this)
    );

    return deferred.promise;
  },

  /**
   * Whether a table exists in the database (both persistent and temporary tables).
   *
   * @param name
   *        (string) Name of the table.
   *
   * @return Promise<bool>
   */
  tableExists: function (name) {
    return this.execute(
      "SELECT name FROM (SELECT * FROM sqlite_master UNION ALL " +
                        "SELECT * FROM sqlite_temp_master) " +
      "WHERE type = 'table' AND name=?",
      [name])
      .then(function onResult(rows) {
        return Promise.resolve(rows.length > 0);
      }
    );
  },

  /**
   * Whether a named index exists (both persistent and temporary tables).
   *
   * @param name
   *        (string) Name of the index.
   *
   * @return Promise<bool>
   */
  indexExists: function (name) {
    return this.execute(
      "SELECT name FROM (SELECT * FROM sqlite_master UNION ALL " +
                        "SELECT * FROM sqlite_temp_master) " +
      "WHERE type = 'index' AND name=?",
      [name])
      .then(function onResult(rows) {
        return Promise.resolve(rows.length > 0);
      }
    );
  },

  /**
   * Free up as much memory from the underlying database connection as possible.
   *
   * @return Promise<>
   */
  shrinkMemory: function () {
    this._log.info("Shrinking memory usage.");

    let onShrunk = this._clearIdleShrinkTimer.bind(this);

    return this.execute("PRAGMA shrink_memory").then(onShrunk, onShrunk);
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
  discardCachedStatements: function () {
    let count = 0;
    for (let [k, statement] of this._cachedStatements) {
      ++count;
      statement.finalize();
    }
    this._cachedStatements.clear();
    this._log.debug("Discarded " + count + " cached statements.");
    return count;
  },

  /**
   * Helper method to bind parameters of various kinds through
   * reflection.
   */
  _bindParameters: function (statement, params) {
    if (!params) {
      return;
    }

    if (Array.isArray(params)) {
      // It's an array of separate params.
      if (params.length && (typeof(params[0]) == "object")) {
        let paramsArray = statement.newBindingParamsArray();
        for (let p of params) {
          let bindings = paramsArray.newBindingParams();
          for (let [key, value] of Iterator(p)) {
            bindings.bindByName(key, value);
          }
          paramsArray.addParams(bindings);
        }

        statement.bindParameters(paramsArray);
        return;
      }

      // Indexed params.
      for (let i = 0; i < params.length; i++) {
        statement.bindByIndex(i, params[i]);
      }
      return;
    }

    // Named params.
    if (params && typeof(params) == "object") {
      for (let k in params) {
        statement.bindByName(k, params[k]);
      }
      return;
    }

    throw new Error("Invalid type for bound parameters. Expected Array or " +
                    "object. Got: " + params);
  },

  _executeStatement: function (sql, statement, params, onRow) {
    if (statement.state != statement.MOZ_STORAGE_STATEMENT_READY) {
      throw new Error("Statement is not ready for execution.");
    }

    if (onRow && typeof(onRow) != "function") {
      throw new Error("onRow must be a function. Got: " + onRow);
    }

    this._bindParameters(statement, params);

    let index = this._statementCounter++;

    let deferred = Promise.defer();
    let userCancelled = false;
    let errors = [];
    let rows = [];

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
      handleResult: function (resultSet) {
        // .cancel() may not be immediate and handleResult() could be called
        // after a .cancel().
        for (let row = resultSet.getNextRow(); row && !userCancelled; row = resultSet.getNextRow()) {
          if (!onRow) {
            rows.push(row);
            continue;
          }

          try {
            onRow(row);
          } catch (e if e instanceof StopIteration) {
            userCancelled = true;
            pending.cancel();
            break;
          } catch (ex) {
            self._log.warn("Exception when calling onRow callback: " +
                           CommonUtils.exceptionStr(ex));
          }
        }
      },

      handleError: function (error) {
        self._log.info("Error when executing SQL (" + error.result + "): " +
                       error.message);
        errors.push(error);
      },

      handleCompletion: function (reason) {
        self._log.debug("Stmt #" + index + " finished.");
        self._pendingStatements.delete(index);

        switch (reason) {
          case Ci.mozIStorageStatementCallback.REASON_FINISHED:
            // If there is an onRow handler, we always resolve to null.
            let result = onRow ? null : rows;
            deferred.resolve(result);
            break;

          case Ci.mozIStorageStatementCallback.REASON_CANCELLED:
            // It is not an error if the user explicitly requested cancel via
            // the onRow handler.
            if (userCancelled) {
              let result = onRow ? null : rows;
              deferred.resolve(result);
            } else {
              deferred.reject(new Error("Statement was cancelled."));
            }

            break;

          case Ci.mozIStorageStatementCallback.REASON_ERROR:
            let error = new Error("Error(s) encountered during statement execution.");
            error.errors = errors;
            deferred.reject(error);
            break;

          default:
            deferred.reject(new Error("Unknown completion reason code: " +
                                      reason));
            break;
        }
      },
    });

    this._pendingStatements.set(index, pending);
    return deferred.promise;
  },

  _ensureOpen: function () {
    if (!this._open) {
      throw new Error("Connection is not open.");
    }
  },

  _clearIdleShrinkTimer: function () {
    if (!this._idleShrinkTimer) {
      return;
    }

    this._idleShrinkTimer.cancel();
  },

  _startIdleShrinkTimer: function () {
    if (!this._idleShrinkTimer) {
      return;
    }

    this._idleShrinkTimer.initWithCallback(this.shrinkMemory.bind(this),
                                           this._idleShrinkMS,
                                           this._idleShrinkTimer.TYPE_ONE_SHOT);
  },
});

this.Sqlite = {
  openConnection: openConnection,
  cloneStorageConnection: cloneStorageConnection,
  /**
   * Shutdown barrier client. May be used by clients to perform last-minute
   * cleanup prior to the shutdown of this module.
   *
   * See the documentation of AsyncShutdown.Barrier.prototype.client.
   */
  get shutdown() {
    return Barriers.shutdown.client;
  }
};
