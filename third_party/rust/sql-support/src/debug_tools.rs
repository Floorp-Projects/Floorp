/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// This module supplies utilities to help with debugging SQL in these components.
///
/// To take advantage of this module, you must enable the `debug-tools` feature for
/// this crate.
use rusqlite::{functions::Context, types::Value, Connection};

/// Print the entire contents of an arbitrary query. A common usage would be to pass
/// `SELECT * FROM table`
#[cfg(feature = "debug-tools")]
pub fn print_query(conn: &Connection, query: &str) -> rusqlite::Result<()> {
    use prettytable::{Cell, Row};

    let mut stmt = conn.prepare(query)?;
    let mut rows = stmt.query([])?;
    let mut table = prettytable::Table::new();
    let mut titles = Row::empty();
    for col in rows.as_ref().expect("must have statement").columns() {
        titles.add_cell(Cell::new(col.name()));
    }
    table.set_titles(titles);
    while let Some(sql_row) = rows.next()? {
        let mut table_row = Row::empty();
        for i in 0..sql_row.as_ref().column_count() {
            let val = match sql_row.get::<_, Value>(i)? {
                Value::Null => "null".to_string(),
                Value::Integer(i) => i.to_string(),
                Value::Real(f) => f.to_string(),
                Value::Text(s) => s,
                Value::Blob(b) => format!("<blob with {} bytes>", b.len()),
            };
            table_row.add_cell(Cell::new(&val));
        }
        table.add_row(table_row);
    }
    // printstd ends up on stdout - if we just println!() extra buffering seems to happen?
    use std::io::Write;
    std::io::stdout()
        .write_all(format!("query: {query}\n").as_bytes())
        .unwrap();
    table.printstd();
    Ok(())
}

#[cfg(feature = "debug-tools")]
#[inline(never)]
fn dbg(ctx: &Context<'_>) -> rusqlite::Result<Value> {
    let mut s = Value::Null;
    for i in 0..ctx.len() {
        let raw = ctx.get_raw(i);
        let str_repr = match raw {
            rusqlite::types::ValueRef::Text(_) => raw.as_str().unwrap().to_owned(),
            _ => format!("{:?}", raw),
        };
        eprint!("{} ", str_repr);
        s = raw.into();
    }
    eprintln!();
    Ok(s)
}

#[cfg(not(feature = "debug-tools"))]
// It would be very bad if the `dbg()` function only existed when the `debug-tools` feature was
// enabled - you could imagine some crate defining it as a `dev-dependency`, but then shipping
// sql with an embedded `dbg()` call - tests and CI would pass, but it would fail in the real lib.
fn dbg(ctx: &Context<'_>) -> rusqlite::Result<Value> {
    Ok(if ctx.is_empty() {
        Value::Null
    } else {
        ctx.get_raw(ctx.len() - 1).into()
    })
}

/// You can call this function to add all sql functions provided by this module
/// to your connection. The list of supported functions is described below.
///
/// *Note:* you must enable the `debug-tools` feature for these functions to perform
/// as described. If this feature is not enabled, functions of the same name will still
/// exist, but will be no-ops.
///
/// # dbg
/// `dbg()` is a simple sql function that prints all args to stderr and returns the last argument.
/// It's useful for instrumenting existing WHERE statements - eg, changing a statement
/// ```sql
/// WHERE a.bar = foo
/// ```
/// to
/// ```sql
/// WHERE dbg("a bar", a.bar) = foo
/// ```
/// will print the *literal* `a bar` followed by the *value* of `a.bar`, then return `a.bar`,
/// so the semantics of the `WHERE` statement do not change.
/// If you have a series of statements being executed (ie, statements separated by a `;`),
/// adding a new statement like `SELECT dbg("hello");` is also possible.
pub fn define_debug_functions(c: &Connection) -> rusqlite::Result<()> {
    use rusqlite::functions::FunctionFlags;
    c.create_scalar_function("dbg", -1, FunctionFlags::SQLITE_UTF8, dbg)?;
    Ok(())
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::conn_ext::ConnExt;

    #[test]
    fn test_dbg() {
        let conn = Connection::open_with_flags(":memory:", rusqlite::OpenFlags::default()).unwrap();
        define_debug_functions(&conn).unwrap();
        assert_eq!(conn.query_one::<i64>("SELECT dbg('foo', 0);").unwrap(), 0);
        assert_eq!(
            conn.query_one::<String>("SELECT dbg('foo');").unwrap(),
            "foo"
        );
        assert_eq!(
            conn.query_one::<Option<String>>("SELECT dbg();").unwrap(),
            None
        );
    }
}
