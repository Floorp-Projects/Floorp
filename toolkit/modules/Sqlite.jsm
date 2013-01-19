/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "Sqlite",
];

const {interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/commonjs/promise/core.js");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-common/log4moz.js");

XPCOMUtils.defineLazyModuleGetter(this, "CommonUtils",
                                  "resource://services-common/utils.js");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");


// Counts the number of created connections per database basename(). This is
// used for logging to distinguish connection instances.
let connectionCounters = {};


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
  let log = Log4Moz.repository.getLogger("Sqlite.ConnectionOpener");

  if (!options.path) {
    throw new Error("path not specified in connection options.");
  }

  // Retains absolute paths and normalizes relative as relative to profile.
  let path = OS.Path.join(OS.Constants.Path.profileDir, options.path);

  let sharedMemoryCache = "sharedMemoryCache" in options ?
                            options.sharedMemoryCache : true;

  let file = FileUtils.File(path);
  let openDatabaseFn = sharedMemoryCache ?
                         Services.storage.openDatabase :
                         Services.storage.openUnsharedDatabase;

  let basename = OS.Path.basename(path);

  if (!connectionCounters[basename]) {
    connectionCounters[basename] = 1;
  }

  let number = connectionCounters[basename]++;
  let identifier = basename + "#" + number;

  log.info("Opening database: " + path + " (" + identifier + ")");
  try {
    let connection = openDatabaseFn(file);

    if (!connection.connectionReady) {
      log.warn("Connection is not ready.");
      return Promise.reject(new Error("Connection is not ready."));
    }

    return Promise.resolve(new OpenedConnection(connection, basename, number));
  } catch (ex) {
    log.warn("Could not open database: " + CommonUtils.exceptionStr(ex));
    return Promise.reject(ex);
  }
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
 */
function OpenedConnection(connection, basename, number) {
  let log = Log4Moz.repository.getLogger("Sqlite.Connection." + basename);

  // getLogger() returns a shared object. We can't modify the functions on this
  // object since they would have effect on all instances and last write would
  // win. So, we create a "proxy" object with our custom functions. Everything
  // else is proxied back to the shared logger instance via prototype
  // inheritance.
  let logProxy = {__proto__: log};

  // Automatically prefix all log messages with the identifier.
  for (let level in Log4Moz.Level) {
    if (level == "Desc") {
      continue;
    }

    let lc = level.toLowerCase();
    logProxy[lc] = function (msg) {
      return log[lc].call(log, "Conn #" + number + ": " + msg);
    };
  }

  this._log = logProxy;

  this._log.info("Opened");

  this._connection = connection;
  this._open = true;

  this._cachedStatements = new Map();
  this._anonymousStatements = new Map();
  this._anonymousCounter = 0;
  this._inProgressStatements = new Map();
  this._inProgressCounter = 0;

  this._inProgressTransaction = null;
}

OpenedConnection.prototype = Object.freeze({
  TRANSACTION_DEFERRED: "DEFERRED",
  TRANSACTION_IMMEDIATE: "IMMEDIATE",
  TRANSACTION_EXCLUSIVE: "EXCLUSIVE",

  TRANSACTION_TYPES: ["DEFERRED", "IMMEDIATE", "EXCLUSIVE"],

  get connectionReady() {
    return this._open && this._connection.connectionReady;
  },

  /**
   * The row ID from the last INSERT operation.
   *
   * Because all statements are executed asynchronously, this could
   * return unexpected results if multiple statements are performed in
   * parallel. It is the caller's responsibility to schedule
   * appropriately.
   *
   * It is recommended to only use this within transactions (which are
   * handled as sequential statements via Tasks).
   */
  get lastInsertRowID() {
    this._ensureOpen();
    return this._connection.lastInsertRowID;
  },

  /**
   * The number of rows that were changed, inserted, or deleted by the
   * last operation.
   *
   * The same caveats regarding asynchronous execution for
   * `lastInsertRowID` also apply here.
   */
  get affectedRows() {
    this._ensureOpen();
    return this._connection.affectedRows;
  },

  /**
   * The integer schema version of the database.
   *
   * This is 0 if not schema version has been set.
   */
  get schemaVersion() {
    this._ensureOpen();
    return this._connection.schemaVersion;
  },

  set schemaVersion(value) {
    this._ensureOpen();
    this._connection.schemaVersion = value;
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
   *
   * IMPROVEMENT: Resolve the promise to a closed connection which can be
   * reopened.
   *
   * @return Promise<>
   */
  close: function () {
    if (!this._connection) {
      return Promise.resolve();
    }

    this._log.debug("Request to close connection.");

    let deferred = Promise.defer();

    // We need to take extra care with transactions during shutdown.
    //
    // If we don't have a transaction in progress, we can proceed with shutdown
    // immediately.
    if (!this._inProgressTransaction) {
      this._finalize(deferred);
      return deferred.promise;
    }

    // Else if we do have a transaction in progress, we forcefully roll it
    // back. This is an async task, so we wait on it to finish before
    // performing finalization.
    this._log.warn("Transaction in progress at time of close. Rolling back.");

    let onRollback = this._finalize.bind(this, deferred);

    this.execute("ROLLBACK TRANSACTION").then(onRollback, onRollback);
    this._inProgressTransaction.reject(new Error("Connection being closed."));
    this._inProgressTransaction = null;

    return deferred.promise;
  },

  _finalize: function (deferred) {
    this._log.debug("Finalizing connection.");
    // Cancel any in-progress statements.
    for (let [k, statement] of this._inProgressStatements) {
      statement.cancel();
    }
    this._inProgressStatements.clear();

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

    return this._executeStatement(sql, statement, params, onRow);
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

    let deferred = Promise.defer();

    this._executeStatement(sql, statement, params, onRow).then(
      function onResult(rows) {
        this._anonymousStatements.delete(index);
        statement.finalize();
        deferred.resolve(rows);
      }.bind(this),

      function onError(error) {
        this._anonymousStatements.delete(index);
        statement.finalize();
        deferred.reject(error);
      }.bind(this)
    );

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
   * Whether a table exists in the database.
   *
   * IMPROVEMENT: Look for temporary tables.
   *
   * @param name
   *        (string) Name of the table.
   *
   * @return Promise<bool>
   */
  tableExists: function (name) {
    return this.execute(
      "SELECT name FROM sqlite_master WHERE type='table' AND name=?",
      [name])
      .then(function onResult(rows) {
        return Promise.resolve(rows.length > 0);
      }
    );
  },

  /**
   * Whether a named index exists.
   *
   * IMPROVEMENT: Look for indexes in temporary tables.
   *
   * @param name
   *        (string) Name of the index.
   *
   * @return Promise<bool>
   */
  indexExists: function (name) {
    return this.execute(
      "SELECT name FROM sqlite_master WHERE type='index' AND name=?",
      [name])
      .then(function onResult(rows) {
        return Promise.resolve(rows.length > 0);
      }
    );
  },

  _executeStatement: function (sql, statement, params, onRow) {
    if (statement.state != statement.MOZ_STORAGE_STATEMENT_READY) {
      throw new Error("Statement is not ready for execution.");
    }

    if (onRow && typeof(onRow) != "function") {
      throw new Error("onRow must be a function. Got: " + onRow);
    }

    if (Array.isArray(params)) {
      for (let i = 0; i < params.length; i++) {
        statement.bindByIndex(i, params[i]);
      }
    } else if (params && typeof(params) == "object") {
      for (let k in params) {
        statement.bindByName(k, params[k]);
      }
    } else if (params) {
      throw new Error("Invalid type for bound parameters. Expected Array or " +
                      "object. Got: " + params);
    }

    let index = this._inProgressCounter++;

    let deferred = Promise.defer();
    let userCancelled = false;
    let errors = [];
    let rows = [];

    // Don't incur overhead for serializing params unless the messages go
    // somewhere.
    if (this._log.level <= Log4Moz.Level.Trace) {
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
        self._log.debug("Stmt #" + index + " finished");
        self._inProgressStatements.delete(index);

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

    this._inProgressStatements.set(index, pending);

    return deferred.promise;
  },

  _ensureOpen: function () {
    if (!this._open) {
      throw new Error("Connection is not open.");
    }
  },
});

this.Sqlite = {
  openConnection: openConnection,
};
