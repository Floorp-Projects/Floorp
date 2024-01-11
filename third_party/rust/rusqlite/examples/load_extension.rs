//! Ensure loadable_extension.rs works.

use rusqlite::{Connection, Result};

fn main() -> Result<()> {
    let db = Connection::open_in_memory()?;

    unsafe {
        db.load_extension_enable()?;
        #[cfg(not(windows))]
        db.load_extension("target/debug/examples/libloadable_extension", None)?;
        #[cfg(windows)]
        db.load_extension("target/debug/examples/loadable_extension", None)?;
        db.load_extension_disable()?;
    }

    let str = db.query_row("SELECT rusqlite_test_function()", [], |row| {
        row.get::<_, String>(0)
    })?;
    assert_eq!(&str, "Rusqlite extension loaded correctly!");
    Ok(())
}
