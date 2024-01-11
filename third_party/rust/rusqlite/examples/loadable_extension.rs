//! Adaptation of https://sqlite.org/loadext.html#programming_loadable_extensions
use std::os::raw::{c_char, c_int};

use rusqlite::ffi;
use rusqlite::functions::FunctionFlags;
use rusqlite::types::{ToSqlOutput, Value};
use rusqlite::{to_sqlite_error, Connection, Result};

/// # build
/// ```sh
/// cargo build --example loadable_extension --features "loadable_extension modern_sqlite functions vtab trace"
/// ```
/// # test
/// ```sh
/// sqlite> .log on
/// sqlite> .load target/debug/examples/libloadable_extension.so
/// (28) Rusqlite extension initialized
/// sqlite> SELECT rusqlite_test_function();
/// Rusqlite extension loaded correctly!
/// ```
#[allow(clippy::not_unsafe_ptr_arg_deref)]
#[no_mangle]
pub extern "C" fn sqlite3_extension_init(
    db: *mut ffi::sqlite3,
    pz_err_msg: *mut *mut c_char,
    p_api: *mut ffi::sqlite3_api_routines,
) -> c_int {
    if p_api.is_null() {
        return ffi::SQLITE_ERROR;
    } else if let Err(err) = extension_init(db, p_api) {
        return unsafe { to_sqlite_error(&err, pz_err_msg) };
    }
    ffi::SQLITE_OK
}

fn extension_init(db: *mut ffi::sqlite3, p_api: *mut ffi::sqlite3_api_routines) -> Result<()> {
    let db = unsafe { Connection::extension_init2(db, p_api)? };
    db.create_scalar_function(
        "rusqlite_test_function",
        0,
        FunctionFlags::SQLITE_DETERMINISTIC,
        |_ctx| {
            Ok(ToSqlOutput::Owned(Value::Text(
                "Rusqlite extension loaded correctly!".to_string(),
            )))
        },
    )?;
    rusqlite::trace::log(ffi::SQLITE_WARNING, "Rusqlite extension initialized");
    Ok(())
}
