/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use chrono::Local;

/// The template parameter for a timestamp in a "raw" sponsored suggestion URL.
const TIMESTAMP_TEMPLATE: &str = "%YYYYMMDDHH%";

/// The length, in bytes, of a timestamp in a "cooked" sponsored suggestion URL.
///
/// Cooked timestamps don't include the leading or trailing `%`, so this is
/// 2 bytes shorter than [`TIMESTAMP_TEMPLATE`].
const TIMESTAMP_LENGTH: usize = 10;

/// A suggestion from the database to show in the address bar.
#[derive(Clone, Debug, PartialEq)]
pub enum Suggestion {
    Amp {
        title: String,
        url: String,
        raw_url: String,
        icon: Option<Vec<u8>>,
        full_keyword: String,
        block_id: i64,
        advertiser: String,
        iab_category: String,
        impression_url: String,
        click_url: String,
        raw_click_url: String,
    },
    Pocket {
        title: String,
        url: String,
        score: f64,
        is_top_pick: bool,
    },
    Wikipedia {
        title: String,
        url: String,
        icon: Option<Vec<u8>>,
        full_keyword: String,
    },
    Amo {
        title: String,
        url: String,
        icon_url: String,
        description: String,
        rating: Option<String>,
        number_of_ratings: i64,
        guid: String,
        score: f64,
    },
}

/// Replaces all template parameters in a "raw" sponsored suggestion URL,
/// producing a "cooked" URL with real values.
pub(crate) fn cook_raw_suggestion_url(raw_url: &str) -> String {
    let timestamp = Local::now().format("%Y%m%d%H").to_string();
    debug_assert!(timestamp.len() == TIMESTAMP_LENGTH);
    // "Raw" sponsored suggestion URLs must not contain more than one timestamp
    // template parameter, so we replace just the first occurrence.
    raw_url.replacen(TIMESTAMP_TEMPLATE, &timestamp, 1)
}

/// Determines whether a "raw" sponsored suggestion URL is equivalent to a
/// "cooked" URL. The two URLs are equivalent if they are identical except for
/// their replaced template parameters, which can be different.
pub fn raw_suggestion_url_matches(raw_url: &str, cooked_url: &str) -> bool {
    let Some((raw_url_prefix, raw_url_suffix)) = raw_url.split_once(TIMESTAMP_TEMPLATE) else {
        return raw_url == cooked_url;
    };
    let (Some(cooked_url_prefix), Some(cooked_url_suffix)) = (
        cooked_url.get(..raw_url_prefix.len()),
        cooked_url.get(raw_url_prefix.len() + TIMESTAMP_LENGTH..),
    ) else {
        return false;
    };
    if raw_url_prefix != cooked_url_prefix || raw_url_suffix != cooked_url_suffix {
        return false;
    }
    let maybe_timestamp =
        &cooked_url[raw_url_prefix.len()..raw_url_prefix.len() + TIMESTAMP_LENGTH];
    maybe_timestamp.bytes().all(|b| b.is_ascii_digit())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn cook_url_with_template_parameters() {
        let raw_url_with_one_timestamp = "https://example.com?a=%YYYYMMDDHH%";
        let cooked_url_with_one_timestamp = cook_raw_suggestion_url(raw_url_with_one_timestamp);
        assert_eq!(
            cooked_url_with_one_timestamp.len(),
            raw_url_with_one_timestamp.len() - 2
        );
        assert_ne!(raw_url_with_one_timestamp, cooked_url_with_one_timestamp);

        let raw_url_with_trailing_segment = "https://example.com?a=%YYYYMMDDHH%&b=c";
        let cooked_url_with_trailing_segment =
            cook_raw_suggestion_url(raw_url_with_trailing_segment);
        assert_eq!(
            cooked_url_with_trailing_segment.len(),
            raw_url_with_trailing_segment.len() - 2
        );
        assert_ne!(
            raw_url_with_trailing_segment,
            cooked_url_with_trailing_segment
        );
    }

    #[test]
    fn cook_url_without_template_parameters() {
        let raw_url_without_timestamp = "https://example.com?b=c";
        let cooked_url_without_timestamp = cook_raw_suggestion_url(raw_url_without_timestamp);
        assert_eq!(raw_url_without_timestamp, cooked_url_without_timestamp);
    }

    #[test]
    fn url_with_template_parameters_matches() {
        let raw_url_with_one_timestamp = "https://example.com?a=%YYYYMMDDHH%";
        let raw_url_with_trailing_segment = "https://example.com?a=%YYYYMMDDHH%&b=c";

        // Equivalent, except for their replaced template parameters.
        assert!(raw_suggestion_url_matches(
            raw_url_with_one_timestamp,
            "https://example.com?a=0000000000"
        ));
        assert!(raw_suggestion_url_matches(
            raw_url_with_trailing_segment,
            "https://example.com?a=1111111111&b=c"
        ));

        // Different lengths.
        assert!(!raw_suggestion_url_matches(
            raw_url_with_one_timestamp,
            "https://example.com?a=1234567890&c=d"
        ));
        assert!(!raw_suggestion_url_matches(
            raw_url_with_one_timestamp,
            "https://example.com?a=123456789"
        ));
        assert!(!raw_suggestion_url_matches(
            raw_url_with_trailing_segment,
            "https://example.com?a=0987654321"
        ));
        assert!(!raw_suggestion_url_matches(
            raw_url_with_trailing_segment,
            "https://example.com?a=0987654321&b=c&d=e"
        ));

        // Different query parameter names.
        assert!(!raw_suggestion_url_matches(
            raw_url_with_one_timestamp,         // `a`.
            "https://example.com?b=4444444444"  // `b`.
        ));
        assert!(!raw_suggestion_url_matches(
            raw_url_with_trailing_segment,          // `a&b`.
            "https://example.com?a=5555555555&c=c"  // `a&c`.
        ));

        // Not a timestamp.
        assert!(!raw_suggestion_url_matches(
            raw_url_with_one_timestamp,
            "https://example.com?a=bcdefghijk"
        ));
        assert!(!raw_suggestion_url_matches(
            raw_url_with_trailing_segment,
            "https://example.com?a=bcdefghijk&b=c"
        ));
    }

    #[test]
    fn url_without_template_parameters_matches() {
        let raw_url_without_timestamp = "https://example.com?b=c";

        assert!(raw_suggestion_url_matches(
            raw_url_without_timestamp,
            "https://example.com?b=c"
        ));
        assert!(!raw_suggestion_url_matches(
            raw_url_without_timestamp,
            "http://example.com"
        ));
        assert!(!raw_suggestion_url_matches(
            raw_url_without_timestamp, // `a`.
            "http://example.com?a=c"   // `b`.
        ));
        assert!(!raw_suggestion_url_matches(
            raw_url_without_timestamp,
            "https://example.com?b=c&d=e"
        ));
    }
}
