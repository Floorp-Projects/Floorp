/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use rusqlite::{
    types::{FromSql, FromSqlError, FromSqlResult, ToSql, ToSqlOutput, ValueRef},
    Result as RusqliteResult,
};

/// A provider is a source of search suggestions.
#[derive(Clone, Copy, Debug, Eq, PartialEq, Hash)]
#[repr(u8)]
pub enum SuggestionProvider {
    Amp = 1,
    Wikipedia = 2,
    Amo = 3,
    Pocket = 4,
    Yelp = 5,
}

impl FromSql for SuggestionProvider {
    fn column_result(value: ValueRef<'_>) -> FromSqlResult<Self> {
        let v = value.as_i64()?;
        u8::try_from(v)
            .ok()
            .and_then(SuggestionProvider::from_u8)
            .ok_or_else(|| FromSqlError::OutOfRange(v))
    }
}

impl SuggestionProvider {
    #[inline]
    pub(crate) fn from_u8(v: u8) -> Option<Self> {
        match v {
            1 => Some(SuggestionProvider::Amp),
            2 => Some(SuggestionProvider::Wikipedia),
            3 => Some(SuggestionProvider::Amo),
            4 => Some(SuggestionProvider::Pocket),
            5 => Some(SuggestionProvider::Yelp),
            _ => None,
        }
    }
}

impl ToSql for SuggestionProvider {
    fn to_sql(&self) -> RusqliteResult<ToSqlOutput<'_>> {
        Ok(ToSqlOutput::from(*self as u8))
    }
}
