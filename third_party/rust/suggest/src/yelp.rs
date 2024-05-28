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
    Pre = 0,
    Post = 1,
    Yelp = 2,
}

impl ToSql for Modifier {
    fn to_sql(&self) -> RusqliteResult<ToSqlOutput<'_>> {
        Ok(ToSqlOutput::from(*self as u8))
    }
}

/// This module assumes like following query.
/// "Yelp-modifier? Pre-modifier? Subject Post-modifier? (Location-modifier | Location-sign Location?)? Yelp-modifier?"
/// For example, the query below is valid.
/// "Yelp (Yelp-modifier) Best(Pre-modifier) Ramen(Subject) Delivery(Post-modifier) In(Location-sign) Tokyo(Location)"
/// Also, as everything except Subject is optional, "Ramen" will be also valid query.
/// However, "Best Best Ramen" and "Ramen Best" is out of the above appearance order rule,
/// parsing will be failed. Also, every words except Location needs to be registered in DB.
/// Please refer to the query test in store.rs for all of combination.
/// Currently, the maximum query length is determined while referring to having word lengths in DB
/// and location names.
/// max subject: 50 + pre-modifier: 10 + post-modifier: 10 + location-sign: 7 + location: 50 = 127 = 150.
const MAX_QUERY_LENGTH: usize = 150;

/// The max number of words consisting the modifier. To improve the SQL performance by matching with
/// "keyword=:modifier" (please see is_modifier()), define this how many words we should check.
const MAX_MODIFIER_WORDS_NUMBER: usize = 2;

/// At least this many characters must be typed for a subject to be matched.
const SUBJECT_PREFIX_MATCH_THRESHOLD: usize = 2;

impl<'a> SuggestDao<'a> {
    /// Inserts the suggestions for Yelp attachment into the database.
    pub(crate) fn insert_yelp_suggestions(
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
                    ":type": Modifier::Pre,
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
                    ":type": Modifier::Post,
                    ":keyword": keyword,
                },
            )?;
        }

        for keyword in &suggestion.yelp_modifiers {
            self.scope.err_if_interrupted()?;
            self.conn.execute_cached(
                "INSERT INTO yelp_modifiers(record_id, type, keyword) VALUES(:record_id, :type, :keyword)",
                named_params! {
                    ":record_id": record_id.as_str(),
                    ":type": Modifier::Yelp,
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

        self.scope.err_if_interrupted()?;
        self.conn.execute_cached(
            "INSERT INTO yelp_custom_details(record_id, icon_id, score) VALUES(:record_id, :icon_id, :score)",
            named_params! {
                ":record_id": record_id.as_str(),
                ":icon_id": suggestion.icon_id,
                ":score": suggestion.score,
            },
        )?;

        Ok(())
    }

    /// Fetch Yelp suggestion from given user's query.
    pub(crate) fn fetch_yelp_suggestions(
        &self,
        query: &SuggestionQuery,
    ) -> Result<Vec<Suggestion>> {
        if !query.providers.contains(&SuggestionProvider::Yelp) {
            return Ok(vec![]);
        }

        if query.keyword.len() > MAX_QUERY_LENGTH {
            return Ok(vec![]);
        }

        let query_string = &query.keyword.trim();
        if !query_string.contains(' ') {
            let Some((subject, subject_exact_match)) = self.find_subject(query_string)? else {
                return Ok(vec![]);
            };
            let (icon, icon_mimetype, score) = self.fetch_custom_details()?;
            let builder = SuggestionBuilder {
                subject: &subject,
                subject_exact_match,
                pre_modifier: None,
                post_modifier: None,
                location_sign: None,
                location: None,
                need_location: false,
                icon,
                icon_mimetype,
                score,
            };
            return Ok(vec![builder.into()]);
        }

        // Find the yelp keyword modifier and remove them from the query.
        let (query_without_yelp_modifiers, _, _) =
            self.find_modifiers(query_string, Modifier::Yelp, Modifier::Yelp)?;

        // Find the location sign and the location.
        let (query_without_location, location_sign, location, need_location) =
            self.find_location(&query_without_yelp_modifiers)?;

        if let (Some(_), false) = (&location, need_location) {
            // The location sign does not need the specific location, but user is setting something.
            return Ok(vec![]);
        }

        if query_without_location.is_empty() {
            // No remained query.
            return Ok(vec![]);
        }

        // Find the modifiers.
        let (subject_candidate, pre_modifier, post_modifier) =
            self.find_modifiers(&query_without_location, Modifier::Pre, Modifier::Post)?;

        let Some((subject, subject_exact_match)) = self.find_subject(&subject_candidate)? else {
            return Ok(vec![]);
        };

        let (icon, icon_mimetype, score) = self.fetch_custom_details()?;
        let builder = SuggestionBuilder {
            subject: &subject,
            subject_exact_match,
            pre_modifier,
            post_modifier,
            location_sign,
            location,
            need_location,
            icon,
            icon_mimetype,
            score,
        };
        Ok(vec![builder.into()])
    }

    /// Fetch the custom details for Yelp suggestions.
    /// It returns the location tuple as follows:
    /// (
    ///   Option<Vec<u8>>: Icon data. If not found, returns None.
    ///   Option<String>: Mimetype of the icon data. If not found, returns None.
    ///   f64: Reflects score field in the yelp_custom_details table.
    /// )
    ///
    /// Note that there should be only one record in `yelp_custom_details`
    /// as all the Yelp assets are stored in the attachment of a single record
    /// on Remote Settings. The following query will perform a table scan against
    /// `yelp_custom_details` followed by an index search against `icons`,
    /// which should be fine since there is only one record in the first table.
    fn fetch_custom_details(&self) -> Result<(Option<Vec<u8>>, Option<String>, f64)> {
        let result = self.conn.query_row_and_then_cachable(
            r#"
            SELECT
              i.data, i.mimetype, y.score
            FROM
              yelp_custom_details y
            LEFT JOIN
              icons i
              ON y.icon_id = i.id
            LIMIT
              1
            "#,
            (),
            |row| -> Result<_> {
                Ok((
                    row.get::<_, Option<Vec<u8>>>(0)?,
                    row.get::<_, Option<String>>(1)?,
                    row.get::<_, f64>(2)?,
                ))
            },
            true,
        )?;

        Ok(result)
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
    fn find_modifiers(
        &self,
        query: &str,
        pre_modifier_type: Modifier,
        post_modifier_type: Modifier,
    ) -> Result<(String, Option<String>, Option<String>)> {
        if !query.contains(' ') {
            return Ok((query.to_string(), None, None));
        }

        let words: Vec<_> = query.split_whitespace().collect();

        let mut pre_modifier = None;
        for n in (1..=MAX_MODIFIER_WORDS_NUMBER).rev() {
            let mut candidate_chunks = words.chunks(n);
            let candidate = candidate_chunks.next().unwrap_or(&[""]).join(" ");
            if self.is_modifier(&candidate, pre_modifier_type)? {
                pre_modifier = Some(candidate);
                break;
            }
        }

        let mut post_modifier = None;
        for n in (1..=MAX_MODIFIER_WORDS_NUMBER).rev() {
            let mut candidate_chunks = words.rchunks(n);
            let candidate = candidate_chunks.next().unwrap_or(&[""]).join(" ");
            if self.is_modifier(&candidate, post_modifier_type)? {
                post_modifier = Some(candidate);
                break;
            }
        }

        let mut without_modifiers = query;
        if let Some(ref modifier) = pre_modifier {
            without_modifiers = &without_modifiers[modifier.len()..];
        }
        if let Some(ref modifier) = post_modifier {
            without_modifiers = &without_modifiers[..without_modifiers.len() - modifier.len()];
        }

        Ok((
            without_modifiers.trim().to_string(),
            pre_modifier,
            post_modifier,
        ))
    }

    /// Find the subject from the given string.
    /// It returns the Option. If it is not none, it contains the tuple as follows:
    /// (
    ///   String: Subject.
    ///   bool: Whether the subject matched exactly with the parameter.
    /// )
    fn find_subject(&self, candidate: &str) -> Result<Option<(String, bool)>> {
        if candidate.is_empty() {
            return Ok(None);
        }

        // If the length of subject candidate is less than
        // SUBJECT_PREFIX_MATCH_THRESHOLD, should exact match.
        if candidate.len() < SUBJECT_PREFIX_MATCH_THRESHOLD {
            return Ok(if self.is_subject(candidate)? {
                Some((candidate.to_string(), true))
            } else {
                None
            });
        }

        // Otherwise, apply prefix-match.
        Ok(
            match self.conn.query_row_and_then_cachable(
                "SELECT keyword
                 FROM yelp_subjects
                 WHERE keyword BETWEEN :candidate AND :candidate || x'FFFF'
                 ORDER BY LENGTH(keyword) ASC, keyword ASC
                 LIMIT 1",
                named_params! {
                    ":candidate": candidate.to_lowercase(),
                },
                |row| row.get::<_, String>(0),
                true,
            ) {
                Ok(keyword) => {
                    debug_assert!(candidate.len() <= keyword.len());
                    Some((
                        format!("{}{}", candidate, &keyword[candidate.len()..]),
                        candidate.len() == keyword.len(),
                    ))
                }
                Err(_) => None,
            },
        )
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
    subject: &'a str,
    subject_exact_match: bool,
    pre_modifier: Option<String>,
    post_modifier: Option<String>,
    location_sign: Option<String>,
    location: Option<String>,
    need_location: bool,
    icon: Option<Vec<u8>>,
    icon_mimetype: Option<String>,
    score: f64,
}

impl<'a> From<SuggestionBuilder<'a>> for Suggestion {
    fn from(builder: SuggestionBuilder<'a>) -> Suggestion {
        // This location sign such the 'near by' needs to add as a description parameter.
        let location_modifier = if !builder.need_location {
            builder.location_sign.as_deref()
        } else {
            None
        };
        let description = [
            builder.pre_modifier.as_deref(),
            Some(builder.subject),
            builder.post_modifier.as_deref(),
            location_modifier,
        ]
        .iter()
        .flatten()
        .copied()
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

        let title = [
            builder.pre_modifier.as_deref(),
            Some(builder.subject),
            builder.post_modifier.as_deref(),
            builder.location_sign.as_deref(),
            builder.location.as_deref(),
        ]
        .iter()
        .flatten()
        .copied()
        .collect::<Vec<_>>()
        .join(" ");

        Suggestion::Yelp {
            url,
            title,
            icon: builder.icon,
            icon_mimetype: builder.icon_mimetype,
            score: builder.score,
            has_location_sign: location_modifier.is_none() && builder.location_sign.is_some(),
            subject_exact_match: builder.subject_exact_match,
            location_param: "find_loc".to_string(),
        }
    }
}
