/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

importScripts("worker_sqlite_shared.js",
  "resource://gre/modules/workers/require.js");

self.onmessage = function onmessage(msg) {
  try {
    run_test();
  } catch (ex) {
    let {message, moduleStack, moduleName, lineNumber} = ex;
    let error = new Error(message, moduleName, lineNumber);
    error.stack = moduleStack;
    dump("Uncaught error: " + error + "\n");
    dump("Full stack: " + moduleStack + "\n");
    throw error;
  }
};

let Sqlite;

let SQLITE_OK;   /* Successful result */
let SQLITE_ROW;  /* sqlite3_step() has another row ready */
let SQLITE_DONE; /* sqlite3_step() has finished executing */

function test_init() {
  do_print("Starting test_init");
  // Sqlite should be loaded.
  Sqlite = require("resource://gre/modules/sqlite/sqlite_internal.js");
  do_check_neq(typeof Sqlite, "undefined");
  do_check_neq(typeof Sqlite.Constants, "undefined");
  SQLITE_OK = Sqlite.Constants.SQLITE_OK;
  SQLITE_ROW = Sqlite.Constants.SQLITE_ROW;
  SQLITE_DONE = Sqlite.Constants.SQLITE_DONE;
}

/**
 * Clean up the database.
 * @param  {sqlite3_ptr} db A pointer to the database.
 */
function cleanupDB(db) {
  withQuery(db, "DROP TABLE IF EXISTS TEST;", SQLITE_DONE);
}

/**
 * Open and close sqlite3 database.
 * @param  {String}   open            A name of the sqlite3 open function to be
 *                                    used.
 * @param  {Array}    openArgs = []   Optional arguments to open function.
 * @param  {Function} callback = null An optional callback to be run after the
 *                                    database is opened but before it is
 *                                    closed.
 */
function withDB(open, openArgs = [], callback = null) {
  let db = Sqlite.Type.sqlite3_ptr.implementation();
  let dbPtr = db.address();

  // Open database.
  let result = Sqlite[open].apply(Sqlite, ["data/test.db", dbPtr].concat(
    openArgs));
  do_check_eq(result, SQLITE_OK);

  // Drop the test table if it already exists.
  cleanupDB(db);

  try {
    if (callback) {
      callback(db);
    }
  } catch (ex) {
    do_check_true(false);
    throw ex;
  } finally {
    // Drop the test table if it still exists.
    cleanupDB(db);
    // Close data base.
    result = Sqlite.close(db);
    do_check_eq(result, SQLITE_OK);
  }
}

/**
 * Execute an SQL query using sqlite3 API.
 * @param  {sqlite3_ptr} db         A pointer to the database.
 * @param  {String}      sql        A SQL query string.
 * @param  {Number}      stepResult Expected result code after evaluating the
 *                                  SQL statement.
 * @param  {Function}    bind       An optional callback with SQL binding steps.
 * @param  {Function}    callback   An optional callback that runs after the SQL
 *                                  query completes.
 */
function withQuery(db, sql, stepResult, bind, callback) {
  // Create an instance of a single SQL statement.
  let sqlStmt = Sqlite.Type.sqlite3_stmt_ptr.implementation();
  let sqlStmtPtr = sqlStmt.address();

  // Unused portion of an SQL query.
  let unused = Sqlite.Type.cstring.implementation();
  let unusedPtr = unused.address();

  // Compile an SQL statement.
  let result = Sqlite.prepare_v2(db, sql, sql.length, sqlStmtPtr, unusedPtr);
  do_check_eq(result, SQLITE_OK);

  try {
    if (bind) {
      bind(sqlStmt);
    }

    // Evaluate an SQL statement.
    result = Sqlite.step(sqlStmt);
    do_check_eq(result, stepResult);

    if (callback) {
      callback(sqlStmt);
    }
  } catch (ex) {
    do_check_true(false);
    throw ex;
  } finally {
    // Destroy a prepared statement object.
    result = Sqlite.finalize(sqlStmt);
    do_check_eq(result, SQLITE_OK);
  }
}

function test_open_close() {
  do_print("Starting test_open_close");
  do_check_eq(typeof Sqlite.open, "function");
  do_check_eq(typeof Sqlite.close, "function");

  withDB("open");
}

function test_open_v2_close() {
  do_print("Starting test_open_v2_close");
  do_check_eq(typeof Sqlite.open_v2, "function");

  withDB("open_v2", [0x02, null]);
}

function createTableOnOpen(db) {
  withQuery(db, "CREATE TABLE TEST(" +
              "ID INT PRIMARY KEY NOT NULL," +
              "FIELD1 INT," +
              "FIELD2 REAL," +
              "FIELD3 TEXT," +
              "FIELD4 TEXT," +
              "FIELD5 BLOB" +
            ");", SQLITE_DONE);
}

function test_create_table() {
  do_print("Starting test_create_table");
  do_check_eq(typeof Sqlite.prepare_v2, "function");
  do_check_eq(typeof Sqlite.step, "function");
  do_check_eq(typeof Sqlite.finalize, "function");

  withDB("open", [], createTableOnOpen);
}

/**
 * Read column values after evaluating the SQL SELECT statement.
 * @param  {sqlite3_stmt_ptr} sqlStmt A pointer to the SQL statement.
 */
function onSqlite3Step(sqlStmt) {
  // Get an int value from a query result from the ID (column 0).
  let field = Sqlite.column_int(sqlStmt, 0);
  do_check_eq(field, 3);

  // Get an int value from a query result from the column 1.
  field = Sqlite.column_int(sqlStmt, 1);
  do_check_eq(field, 2);
  // Get an int64 value from a query result from the column 1.
  field = Sqlite.column_int64(sqlStmt, 1);
  do_check_eq(field, 2);

  // Get a double value from a query result from the column 2.
  field = Sqlite.column_double(sqlStmt, 2);
  do_check_eq(field, 1.2);

  // Get a number of bytes of the value in the column 3.
  let bytes = Sqlite.column_bytes(sqlStmt, 3);
  do_check_eq(bytes, 4);
  // Get a text(cstring) value from a query result from the column 3.
  field = Sqlite.column_text(sqlStmt, 3);
  do_check_eq(field.readString(), "DATA");

  // Get a number of bytes of the UTF-16 value in the column 4.
  bytes = Sqlite.column_bytes16(sqlStmt, 4);
  do_check_eq(bytes, 8);
  // Get a text16(wstring) value from a query result from the column 4.
  field = Sqlite.column_text16(sqlStmt, 4);
  do_check_eq(field.readString(), "TADA");

  // Get a blob value from a query result from the column 5.
  field = Sqlite.column_blob(sqlStmt, 5);
  do_check_eq(ctypes.cast(field,
    Sqlite.Type.cstring.implementation).readString(), "BLOB");
}

function test_insert_select() {
  do_print("Starting test_insert_select");
  do_check_eq(typeof Sqlite.column_int, "function");
  do_check_eq(typeof Sqlite.column_int64, "function");
  do_check_eq(typeof Sqlite.column_double, "function");
  do_check_eq(typeof Sqlite.column_bytes, "function");
  do_check_eq(typeof Sqlite.column_text, "function");
  do_check_eq(typeof Sqlite.column_text16, "function");
  do_check_eq(typeof Sqlite.column_blob, "function");

  function onOpen(db) {
    createTableOnOpen(db);
    withQuery(db,
      "INSERT INTO TEST VALUES (3, 2, 1.2, \"DATA\", \"TADA\", \"BLOB\");",
      SQLITE_DONE);
    withQuery(db, "SELECT * FROM TEST;", SQLITE_ROW, null, onSqlite3Step);
  }

  withDB("open", [], onOpen);
}

function test_insert_bind_select() {
  do_print("Starting test_insert_bind_select");
  do_check_eq(typeof Sqlite.bind_int, "function");
  do_check_eq(typeof Sqlite.bind_int64, "function");
  do_check_eq(typeof Sqlite.bind_double, "function");
  do_check_eq(typeof Sqlite.bind_text, "function");
  do_check_eq(typeof Sqlite.bind_text16, "function");
  do_check_eq(typeof Sqlite.bind_blob, "function");

  function onBind(sqlStmt) {
    // Bind an int value to the ID (column 0).
    let result = Sqlite.bind_int(sqlStmt, 1, 3);
    do_check_eq(result, SQLITE_OK);

    // Bind an int64 value to the FIELD1 (column 1).
    result = Sqlite.bind_int64(sqlStmt, 2, 2);
    do_check_eq(result, SQLITE_OK);

    // Bind a double value to the FIELD2 (column 2).
    result = Sqlite.bind_double(sqlStmt, 3, 1.2);
    do_check_eq(result, SQLITE_OK);

    // Destructor.
    let destructor = Sqlite.Constants.SQLITE_TRANSIENT;
    // Bind a text value to the FIELD3 (column 3).
    result = Sqlite.bind_text(sqlStmt, 4, "DATA", 4, destructor);
    do_check_eq(result, SQLITE_OK);

    // Bind a text16 value to the FIELD4 (column 4).
    result = Sqlite.bind_text16(sqlStmt, 5, "TADA", 8, destructor);
    do_check_eq(result, SQLITE_OK);

    // Bind a blob value to the FIELD5 (column 5).
    result = Sqlite.bind_blob(sqlStmt, 6, ctypes.char.array()("BLOB"), 4,
      destructor);
    do_check_eq(result, SQLITE_OK);
  }

  function onOpen(db) {
    createTableOnOpen(db);
    withQuery(db, "INSERT INTO TEST VALUES (?, ?, ?, ?, ?, ?);", SQLITE_DONE,
      onBind);
    withQuery(db, "SELECT * FROM TEST;", SQLITE_ROW, null, onSqlite3Step);
  }

  withDB("open", [], onOpen);
}

function run_test() {
  test_init();
  test_open_close();
  test_open_v2_close();
  test_create_table();
  test_insert_select();
  test_insert_bind_select();
  do_test_complete();
}
