//! Ensure we reject connections when SQLite is in single-threaded mode, as it
//! would violate safety if multiple Rust threads tried to use connections.

#[cfg(not(feature = "loadable_extension"))]
#[test]
fn test_error_when_singlethread_mode() {
    use rusqlite::ffi;
    use rusqlite::Connection;
    // put SQLite into single-threaded mode
    unsafe {
        // Note: macOS system SQLite seems to return an error if you attempt to
        // reconfigure to single-threaded mode.
        if ffi::sqlite3_config(ffi::SQLITE_CONFIG_SINGLETHREAD) != ffi::SQLITE_OK {
            return;
        }
        assert_eq!(ffi::sqlite3_initialize(), ffi::SQLITE_OK);
    }
    let res = Connection::open_in_memory();
    res.unwrap_err();
}
