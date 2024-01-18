/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use rusqlite::types::ToSqlOutput;
use rusqlite::{named_params, Result as RusqliteResult, ToSql};
use sql_support::ConnExt;
use url::form_urlencoded;

use crate::{
    db::SuggestDao,
    provider::SuggestionProvider,
    rs::{DownloadedYelpSuggestion, SuggestRecordId},
    suggestion::Suggestion,
    Result, SuggestionQuery,
};

#[derive(Clone, Copy, Debug, Eq, PartialEq, Hash)]
#[repr(u8)]
enum Modifier {
    PreModifier = 0,
    PostModifier = 1,
}

impl ToSql for Modifier {
    fn to_sql(&self) -> RusqliteResult<ToSqlOutput<'_>> {
        Ok(ToSqlOutput::from(*self as u8))
    }
}

/// This module assumes like following query.
/// "Pre-modifier? Subject Post-modifier? (Location-modifier | Location-sign Location?)?"
/// For example, the query below is valid.
/// "Best(Pre-modifier) Ramen(Subject) Delivery(Post-modifier) In(Location-sign) Tokyo(Location)"
/// Also, as everything except Subject is optional, "Ramen" will be also valid query.
/// However, "Best Best Ramen" and "Ramen Best" is out of the above appearance order rule,
/// parsing will be failed. Also, every words except Location needs to be registered in DB.
/// Please refer to the query test in store.rs for all of combination.
/// Currently, the maximum query length is determined while refering to having word lengths in DB
/// and location names.
/// max subject: 50 + pre-modifier: 10 + post-modifier: 10 + location-sign: 7 + location: 50 = 127 = 150.
const MAX_QUERY_LENGTH: usize = 150;

/// The max number of words consisting the modifier. To improve the SQL performance by matching with
/// "keyword=:modifier" (please see is_modifier()), define this how many words we should check.
const MAX_MODIFIER_WORDS_NUMBER: usize = 2;

impl<'a> SuggestDao<'a> {
    /// Inserts the suggestions for Yelp attachment into the database.
    pub fn insert_yelp_suggestions(
        &mut self,
        record_id: &SuggestRecordId,
        suggestion: &DownloadedYelpSuggestion,
    ) -> Result<()> {
        for keyword in &suggestion.subjects {
            self.scope.err_if_interrupted()?;
            self.conn.execute_cached(
                "INSERT INTO yelp_subjects(record_id, keyword) VALUES(:record_id, :keyword)",
                named_params! {
                    ":record_id": record_id.as_str(),
                    ":keyword": keyword,
                },
            )?;
        }

        for keyword in &suggestion.pre_modifiers {
            self.scope.err_if_interrupted()?;
            self.conn.execute_cached(
                "INSERT INTO yelp_modifiers(record_id, type, keyword) VALUES(:record_id, :type, :keyword)",
                named_params! {
                    ":record_id": record_id.as_str(),
                    ":type": Modifier::PreModifier,
                    ":keyword": keyword,
                },
            )?;
        }

        for keyword in &suggestion.post_modifiers {
            self.scope.err_if_interrupted()?;
            self.conn.execute_cached(
                "INSERT INTO yelp_modifiers(record_id, type, keyword) VALUES(:record_id, :type, :keyword)",
                named_params! {
                    ":record_id": record_id.as_str(),
                    ":type": Modifier::PostModifier,
                    ":keyword": keyword,
                },
            )?;
        }

        for sign in &suggestion.location_signs {
            self.scope.err_if_interrupted()?;
            self.conn.execute_cached(
                "INSERT INTO yelp_location_signs(record_id, keyword, need_location) VALUES(:record_id, :keyword, :need_location)",
                named_params! {
                    ":record_id": record_id.as_str(),
                    ":keyword": sign.keyword,
                    ":need_location": sign.need_location,
                },
            )?;
        }

        Ok(())
    }

    /// Fetch Yelp suggestion from given user's query.
    pub fn fetch_yelp_suggestion(&self, query: &SuggestionQuery) -> Result<Option<Suggestion>> {
        if !query.providers.contains(&SuggestionProvider::Yelp) {
            return Ok(None);
        }

        if query.keyword.len() > MAX_QUERY_LENGTH {
            return Ok(None);
        }

        let query_string = &query.keyword.trim();
        if !query_string.contains(' ') {
            if !self.is_subject(query_string)? {
                return Ok(None);
            }

            let builder = SuggestionBuilder {
                query,
                subject: query_string,
                pre_modifier: None,
                post_modifier: None,
                location_sign: None,
                location: None,
                need_location: false,
            };
            return Ok(Some(builder.into()));
        }

        // Find the location sign and the location.
        let (query_without_location, location_sign, location, need_location) =
            self.find_location(query_string)?;

        if let (Some(_), false) = (&location, need_location) {
            // The location sign does not need the specific location, but user is setting something.
            return Ok(None);
        }

        if query_without_location.is_empty() {
            // No remained query.
            return Ok(None);
        }

        // Find the modifiers.
        let (subject_candidate, pre_modifier, post_modifier) =
            self.find_modifiers(&query_without_location)?;

        if !self.is_subject(&subject_candidate)? {
            return Ok(None);
        }

        let builder = SuggestionBuilder {
            query,
            subject: &subject_candidate,
            pre_modifier,
            post_modifier,
            location_sign,
            location,
            need_location,
        };
        Ok(Some(builder.into()))
    }

    /// Find the location information from the given query string.
    /// It returns the location tuple as follows:
    /// (
    ///   String: Query string that is removed found location information.
    ///   Option<String>: Location sign found in yelp_location_signs table. If not found, returns None.
    ///   Option<String>: Specific location name after location sign. If not found, returns None.
    ///   bool: Reflects need_location field in the table.
    /// )
    fn find_location(&self, query: &str) -> Result<(String, Option<String>, Option<String>, bool)> {
        let query_with_spaces = format!(" {} ", query);
        let mut results: Vec<(usize, usize, i8)> = self.conn.query_rows_and_then_cached(
            "
        SELECT
          INSTR(:query, ' ' || keyword || ' ') AS sign_index,
          LENGTH(keyword) AS sign_length,
          need_location
        FROM yelp_location_signs
        WHERE
          sign_index > 0
        ORDER BY
          sign_length DESC
        LIMIT 1
        ",
            named_params! {
                ":query": &query_with_spaces.to_lowercase(),
            },
            |row| -> Result<_> {
                Ok((
                    row.get::<_, usize>("sign_index")?,
                    row.get::<_, usize>("sign_length")?,
                    row.get::<_, i8>("need_location")?,
                ))
            },
        )?;

        let (sign_index, sign_length, need_location) = if let Some(res) = results.pop() {
            res
        } else {
            return Ok((query.trim().to_string(), None, None, false));
        };

        let pre_location = query_with_spaces
            .get(..sign_index)
            .map(str::trim)
            .map(str::to_string)
            .unwrap_or_default();
        let location_sign = query_with_spaces
            .get(sign_index..sign_index + sign_length)
            .map(str::trim)
            .filter(|s| !s.is_empty())
            .map(str::to_string);
        let location = query_with_spaces
            .get(sign_index + sign_length..)
            .map(str::trim)
            .filter(|s| !s.is_empty())
            .map(str::to_string);

        Ok((pre_location, location_sign, location, need_location == 1))
    }

    /// Find the pre/post modifier from the given query string.
    /// It returns the modifiers tuple as follows:
    /// (
    ///   String: Query string that is removed found the modifiers.
    ///   Option<String>: Pre-modifier found in the yelp_modifiers table. If not found, returns None.
    ///   Option<String>: Post-modifier found in the yelp_modifiers table. If not found, returns None.
    /// )
    fn find_modifiers(&self, query: &str) -> Result<(String, Option<String>, Option<String>)> {
        if !query.contains(' ') {
            return Ok((query.to_string(), None, None));
        }

        let words: Vec<_> = query.split_whitespace().collect();

        let mut pre_modifier = None;
        for n in (1..=MAX_MODIFIER_WORDS_NUMBER).rev() {
            let mut candidate_chunks = words.chunks(n);
            let candidate = candidate_chunks.next().unwrap_or(&[""]).join(" ");
            if self.is_modifier(&candidate, Modifier::PreModifier)? {
                pre_modifier = Some(candidate);
                break;
            }
        }

        let mut post_modifier = None;
        for n in (1..=MAX_MODIFIER_WORDS_NUMBER).rev() {
            let mut candidate_chunks = words.rchunks(n);
            let candidate = candidate_chunks.next().unwrap_or(&[""]).join(" ");
            if self.is_modifier(&candidate, Modifier::PostModifier)? {
                post_modifier = Some(candidate);
                break;
            }
        }

        let mut subject_candidate = query;
        if let Some(ref modifier) = pre_modifier {
            subject_candidate = &subject_candidate[modifier.len()..];
        }
        if let Some(ref modifier) = post_modifier {
            subject_candidate = &subject_candidate[..subject_candidate.len() - modifier.len()];
        }

        Ok((
            subject_candidate.trim().to_string(),
            pre_modifier,
            post_modifier,
        ))
    }

    fn is_modifier(&self, word: &str, modifier_type: Modifier) -> Result<bool> {
        let result = self.conn.query_row_and_then_cachable(
            "
        SELECT EXISTS (
            SELECT 1 FROM yelp_modifiers WHERE type = :type AND keyword = :word LIMIT 1
        )
        ",
            named_params! {
                ":type": modifier_type,
                ":word": word.to_lowercase(),
            },
            |row| row.get::<_, bool>(0),
            true,
        )?;

        Ok(result)
    }

    fn is_subject(&self, word: &str) -> Result<bool> {
        if word.is_empty() {
            return Ok(false);
        }

        let result = self.conn.query_row_and_then_cachable(
            "
        SELECT EXISTS (
            SELECT 1 FROM yelp_subjects WHERE keyword = :word LIMIT 1
        )
        ",
            named_params! {
                ":word": word.to_lowercase(),
            },
            |row| row.get::<_, bool>(0),
            true,
        )?;

        Ok(result)
    }
}

struct SuggestionBuilder<'a> {
    query: &'a SuggestionQuery,
    subject: &'a str,
    pre_modifier: Option<String>,
    post_modifier: Option<String>,
    location_sign: Option<String>,
    location: Option<String>,
    need_location: bool,
}

impl<'a> From<SuggestionBuilder<'a>> for Suggestion {
    fn from(builder: SuggestionBuilder<'a>) -> Suggestion {
        // This location sign such the 'near by' needs to add as a description parameter.
        let location_modifier = if !builder.need_location {
            builder.location_sign
        } else {
            None
        };
        let description = [
            builder.pre_modifier,
            Some(builder.subject.to_string()),
            builder.post_modifier,
            location_modifier,
        ]
        .iter()
        .flatten()
        .map(|s| s.as_str())
        .collect::<Vec<_>>()
        .join(" ");

        // https://www.yelp.com/search?find_desc={description}&find_loc={location}
        let mut url = String::from("https://www.yelp.com/search?");
        let mut parameters = form_urlencoded::Serializer::new(String::new());
        parameters.append_pair("find_desc", &description);
        if let (Some(location), true) = (&builder.location, builder.need_location) {
            parameters.append_pair("find_loc", location);
        }
        url.push_str(&parameters.finish());

        Suggestion::Yelp {
            url,
            // Use user’s query as title as it is.
            title: builder.query.keyword.clone(),
        }
    }
}
