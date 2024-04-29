/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(unknown_lints)]
#![warn(rust_2018_idioms)]

//! A crate with various sql/sqlcipher helpers.

mod conn_ext;
pub mod debug_tools;
mod each_chunk;
mod lazy;
mod maybe_cached;
pub mod open_database;
mod repeat;

pub use conn_ext::*;
pub use each_chunk::*;
pub use lazy::*;
pub use maybe_cached::*;
pub use repeat::*;

/// In PRAGMA foo='bar', `'bar'` must be a constant string (it cannot be a
/// bound parameter), so we need to escape manually. According to
/// <https://www.sqlite.org/faq.html>, the only character that must be escaped is
/// the single quote, which is escaped by placing two single quotes in a row.
pub fn escape_string_for_pragma(s: &str) -> String {
    s.replace('\'', "''")
}

#[cfg(test)]
mod test {
    use super::*;
    #[test]
    fn test_escape_string_for_pragma() {
        assert_eq!(escape_string_for_pragma("foobar"), "foobar");
        assert_eq!(escape_string_for_pragma("'foo'bar'"), "''foo''bar''");
        assert_eq!(escape_string_for_pragma("''"), "''''");
    }
}
