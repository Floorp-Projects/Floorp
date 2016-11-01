/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file defines an Sqlite object containing js-ctypes bindings for
 * sqlite3. It should be included from a worker thread using require.
 *
 * It serves the following purposes:
 * - opens libxul;
 * - defines sqlite3 API functions;
 * - defines the necessary sqlite3 types.
 */

"use strict";

importScripts("resource://gre/modules/workers/require.js");

var SharedAll = require(
  "resource://gre/modules/osfile/osfile_shared_allthreads.jsm");

// Open the sqlite3 library.
var path;
if (SharedAll.Constants.Sys.Name === "Android") {
  path = ctypes.libraryName("sqlite3");
} else if (SharedAll.Constants.Win) {
  path = ctypes.libraryName("nss3");
} else {
  path = SharedAll.Constants.Path.libxul;
}

var lib;
try {
  lib = ctypes.open(path);
} catch (ex) {
  throw new Error("Could not open system library: " + ex.message);
}

var declareLazyFFI = SharedAll.declareLazyFFI;

var Type = Object.create(SharedAll.Type);

/**
 * Opaque Structure |sqlite3_ptr|.
 * |sqlite3_ptr| is equivalent to a void*.
 */
Type.sqlite3_ptr = Type.voidptr_t.withName("sqlite3_ptr");

/**
 * |sqlite3_stmt_ptr| an instance of an object representing a single SQL
 * statement.
 * |sqlite3_stmt_ptr| is equivalent to a void*.
 */
Type.sqlite3_stmt_ptr = Type.voidptr_t.withName("sqlite3_stmt_ptr");

/**
 * |sqlite3_destructor_ptr| a constant defining a special destructor behaviour.
 * |sqlite3_destructor_ptr| is equivalent to a void*.
 */
Type.sqlite3_destructor_ptr = Type.voidptr_t.withName(
  "sqlite3_destructor_ptr");

/**
 * A C double.
 */
Type.double = new SharedAll.Type("double", ctypes.double);

/**
 * |sqlite3_int64| typedef for 64-bit integer.
 */
Type.sqlite3_int64 = Type.int64_t.withName("sqlite3_int64");

/**
 * Sqlite3 constants.
 */
var Constants = {};

/**
 * |SQLITE_STATIC| a special value for the destructor that is passed as an
 * argument to routines like bind_blob, bind_text and bind_text16. It means that
 * the content pointer is constant and will never change and does need to be
 * destroyed.
 */
Constants.SQLITE_STATIC = Type.sqlite3_destructor_ptr.implementation(0);

/**
 * |SQLITE_TRANSIENT| a special value for the destructor that is passed as an
 * argument to routines like bind_blob, bind_text and bind_text16. It means that
 * the content will likely change in the near future and that SQLite should make
 * its own private copy of the content before returning.
 */
Constants.SQLITE_TRANSIENT = Type.sqlite3_destructor_ptr.implementation(-1);

/**
 * |SQLITE_OK|
 * Successful result.
 */
Constants.SQLITE_OK = 0;

/**
 * |SQLITE_ROW|
 * sqlite3_step() has another row ready.
 */
Constants.SQLITE_ROW = 100;

/**
 * |SQLITE_DONE|
 * sqlite3_step() has finished executing.
 */
Constants.SQLITE_DONE = 101;

var Sqlite3 = {
  Constants: Constants,
  Type: Type
};

declareLazyFFI(Sqlite3, "open", lib, "sqlite3_open", null,
               /* return*/    Type.int,
               /* path*/      Type.char.in_ptr,
               /* db handle*/ Type.sqlite3_ptr.out_ptr);

declareLazyFFI(Sqlite3, "open_v2", lib, "sqlite3_open_v2", null,
               /* return*/    Type.int,
               /* path*/      Type.char.in_ptr,
               /* db handle*/ Type.sqlite3_ptr.out_ptr,
               /* flags*/     Type.int,
               /* VFS*/       Type.char.in_ptr);

declareLazyFFI(Sqlite3, "close", lib, "sqlite3_close", null,
               /* return*/    Type.int,
               /* db handle*/ Type.sqlite3_ptr);

declareLazyFFI(Sqlite3, "prepare_v2", lib, "sqlite3_prepare_v2", null,
               /* return*/    Type.int,
               /* db handle*/ Type.sqlite3_ptr,
               /* zSql*/      Type.char.in_ptr,
               /* nByte*/     Type.int,
               /* statement*/ Type.sqlite3_stmt_ptr.out_ptr,
               /* unused*/    Type.cstring.out_ptr);

declareLazyFFI(Sqlite3, "step", lib, "sqlite3_step", null,
               /* return*/    Type.int,
               /* statement*/ Type.sqlite3_stmt_ptr);

declareLazyFFI(Sqlite3, "finalize", lib, "sqlite3_finalize", null,
               /* return*/    Type.int,
               /* statement*/ Type.sqlite3_stmt_ptr);

declareLazyFFI(Sqlite3, "reset", lib, "sqlite3_reset", null,
               /* return*/    Type.int,
               /* statement*/ Type.sqlite3_stmt_ptr);

declareLazyFFI(Sqlite3, "column_int", lib, "sqlite3_column_int", null,
               /* return*/    Type.int,
               /* statement*/ Type.sqlite3_stmt_ptr,
               /* col*/       Type.int);

declareLazyFFI(Sqlite3, "column_blob", lib, "sqlite3_column_blob", null,
               /* return*/    Type.voidptr_t,
               /* statement*/ Type.sqlite3_stmt_ptr,
               /* col*/       Type.int);

declareLazyFFI(Sqlite3, "column_bytes", lib, "sqlite3_column_bytes", null,
               /* return*/    Type.int,
               /* statement*/ Type.sqlite3_stmt_ptr,
               /* col*/       Type.int);

declareLazyFFI(Sqlite3, "column_bytes16", lib, "sqlite3_column_bytes16",
                             null,
               /* return*/    Type.int,
               /* statement*/ Type.sqlite3_stmt_ptr,
               /* col*/       Type.int);

declareLazyFFI(Sqlite3, "column_double", lib, "sqlite3_column_double", null,
               /* return*/    Type.double,
               /* statement*/ Type.sqlite3_stmt_ptr,
               /* col*/       Type.int);

declareLazyFFI(Sqlite3, "column_int64", lib, "sqlite3_column_int64", null,
               /* return*/    Type.sqlite3_int64,
               /* statement*/ Type.sqlite3_stmt_ptr,
               /* col*/       Type.int);

declareLazyFFI(Sqlite3, "column_text", lib, "sqlite3_column_text", null,
               /* return*/    Type.cstring,
               /* statement*/ Type.sqlite3_stmt_ptr,
               /* col*/       Type.int);

declareLazyFFI(Sqlite3, "column_text16", lib, "sqlite3_column_text16", null,
               /* return*/    Type.wstring,
               /* statement*/ Type.sqlite3_stmt_ptr,
               /* col*/       Type.int);

declareLazyFFI(Sqlite3, "bind_int", lib, "sqlite3_bind_int", null,
               /* return*/    Type.int,
               /* statement*/ Type.sqlite3_stmt_ptr,
               /* index*/     Type.int,
               /* value*/     Type.int);

declareLazyFFI(Sqlite3, "bind_int64", lib, "sqlite3_bind_int64", null,
               /* return*/    Type.int,
               /* statement*/ Type.sqlite3_stmt_ptr,
               /* index*/     Type.int,
               /* value*/     Type.sqlite3_int64);

declareLazyFFI(Sqlite3, "bind_double", lib, "sqlite3_bind_double", null,
               /* return*/    Type.int,
               /* statement*/ Type.sqlite3_stmt_ptr,
               /* index*/     Type.int,
               /* value*/     Type.double);

declareLazyFFI(Sqlite3, "bind_null", lib, "sqlite3_bind_null", null,
               /* return*/    Type.int,
               /* statement*/ Type.sqlite3_stmt_ptr,
               /* index*/     Type.int);

declareLazyFFI(Sqlite3, "bind_zeroblob", lib, "sqlite3_bind_zeroblob", null,
               /* return*/    Type.int,
               /* statement*/ Type.sqlite3_stmt_ptr,
               /* index*/     Type.int,
               /* nBytes*/    Type.int);

declareLazyFFI(Sqlite3, "bind_text", lib, "sqlite3_bind_text", null,
               /* return*/     Type.int,
               /* statement*/  Type.sqlite3_stmt_ptr,
               /* index*/      Type.int,
               /* value*/      Type.cstring,
               /* nBytes*/     Type.int,
               /* destructor*/ Type.sqlite3_destructor_ptr);

declareLazyFFI(Sqlite3, "bind_text16", lib, "sqlite3_bind_text16", null,
               /* return*/     Type.int,
               /* statement*/  Type.sqlite3_stmt_ptr,
               /* index*/      Type.int,
               /* value*/      Type.wstring,
               /* nBytes*/     Type.int,
               /* destructor*/ Type.sqlite3_destructor_ptr);

declareLazyFFI(Sqlite3, "bind_blob", lib, "sqlite3_bind_blob", null,
               /* return*/     Type.int,
               /* statement*/  Type.sqlite3_stmt_ptr,
               /* index*/      Type.int,
               /* value*/      Type.voidptr_t,
               /* nBytes*/     Type.int,
               /* destructor*/ Type.sqlite3_destructor_ptr);

module.exports = {
  get Constants() {
    return Sqlite3.Constants;
  },
  get Type() {
    return Sqlite3.Type;
  },
  get open() {
    return Sqlite3.open;
  },
  get open_v2() {
    return Sqlite3.open_v2;
  },
  get close() {
    return Sqlite3.close;
  },
  get prepare_v2() {
    return Sqlite3.prepare_v2;
  },
  get step() {
    return Sqlite3.step;
  },
  get finalize() {
    return Sqlite3.finalize;
  },
  get reset() {
    return Sqlite3.reset;
  },
  get column_int() {
    return Sqlite3.column_int;
  },
  get column_blob() {
    return Sqlite3.column_blob;
  },
  get column_bytes() {
    return Sqlite3.column_bytes;
  },
  get column_bytes16() {
    return Sqlite3.column_bytes16;
  },
  get column_double() {
    return Sqlite3.column_double;
  },
  get column_int64() {
    return Sqlite3.column_int64;
  },
  get column_text() {
    return Sqlite3.column_text;
  },
  get column_text16() {
    return Sqlite3.column_text16;
  },
  get bind_int() {
    return Sqlite3.bind_int;
  },
  get bind_int64() {
    return Sqlite3.bind_int64;
  },
  get bind_double() {
    return Sqlite3.bind_double;
  },
  get bind_null() {
    return Sqlite3.bind_null;
  },
  get bind_zeroblob() {
    return Sqlite3.bind_zeroblob;
  },
  get bind_text() {
    return Sqlite3.bind_text;
  },
  get bind_text16() {
    return Sqlite3.bind_text16;
  },
  get bind_blob() {
    return Sqlite3.bind_blob;
  }
};
