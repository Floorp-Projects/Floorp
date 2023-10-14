/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use rusqlite::types::{FromSql, FromSqlError, FromSqlResult, ToSqlOutput, ValueRef};
use rusqlite::{Result as RusqliteResult, ToSql};

/// Classification of Pocket confidence keywords, where High Confidence
/// require an exact match to keyword prefix and suffix.
/// While Low Confidence, requires a match on prefix and be a
/// substring for the suffix.
#[derive(Clone, Copy, Debug, Eq, PartialEq, Hash)]
#[repr(u8)]
pub enum KeywordConfidence {
    Low = 0,
    High = 1,
}

impl FromSql for KeywordConfidence {
    fn column_result(value: ValueRef<'_>) -> FromSqlResult<Self> {
        let v = value.as_i64()?;
        u8::try_from(v)
            .ok()
            .and_then(KeywordConfidence::from_u8)
            .ok_or_else(|| FromSqlError::OutOfRange(v))
    }
}

impl KeywordConfidence {
    #[inline]
    pub(crate) fn from_u8(v: u8) -> Option<Self> {
        match v {
            0 => Some(KeywordConfidence::Low),
            1 => Some(KeywordConfidence::High),
            _ => None,
        }
    }
}

impl ToSql for KeywordConfidence {
    fn to_sql(&self) -> RusqliteResult<ToSqlOutput<'_>> {
        Ok(ToSqlOutput::from(*self as u8))
    }
}

/// Split the keyword by the first whitespace into the prefix and the suffix.
/// Return an empty string as the suffix if there is no whitespace.
///
/// # Examples
///
/// ```
/// # use suggest::pocket::split_keyword;
/// assert_eq!(split_keyword("foo"), ("foo", ""));
/// assert_eq!(split_keyword("foo bar baz"), ("foo", "bar baz"));
/// ```
pub fn split_keyword(keyword: &str) -> (&str, &str) {
    keyword.split_once(' ').unwrap_or((keyword, ""))
}
