/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/// Given a list of keywords for a suggestion, returns a phrase that best
/// completes the user's query. This function uses two heuristics to pick the
/// best match:
///
/// 1. Find the first keyword in the list that has at least one more word than
///    the query, then trim the keyword up to the end of that word.
/// 2. If there isn't a keyword with more words, pick the keyword that forms the
///    longest suffix of the query. This might be the query itself.
pub fn full_keyword(query: &str, keywords: &[impl AsRef<str>]) -> String {
    let query_words_len = query.split_whitespace().count();
    let min_phrase_words_len = if query.ends_with(char::is_whitespace) {
        // If the query ends with a space, find a keyword with at least one more
        // word, so that the completed phrase can show a word after the space.
        query_words_len + 1
    } else {
        query_words_len
    };
    keywords
        .iter()
        .map(AsRef::as_ref)
        .filter(|phrase| phrase.starts_with(query))
        .map(|phrase| phrase.split_whitespace().collect::<Vec<_>>())
        .find(|phrase_words| phrase_words.len() > min_phrase_words_len)
        .map(|phrase_words| phrase_words[..min_phrase_words_len].join(" "))
        .unwrap_or_else(|| {
            keywords
                .iter()
                .map(AsRef::as_ref)
                .filter(|phrase| phrase.starts_with(query) && query.len() < phrase.len())
                .max_by_key(|phrase| phrase.trim().len())
                .unwrap_or(query)
                .to_owned()
        })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn keywords_with_more_words() {
        assert_eq!(
            full_keyword(
                "moz",
                &[
                    "moz",
                    "mozi",
                    "mozil",
                    "mozill",
                    "mozilla",
                    "mozilla firefox"
                ]
            ),
            "mozilla".to_owned(),
        );
        assert_eq!(
            full_keyword(
                "mozilla",
                &[
                    "moz",
                    "mozi",
                    "mozil",
                    "mozill",
                    "mozilla",
                    "mozilla firefox"
                ]
            ),
            "mozilla".to_owned(),
        );
    }

    #[test]
    fn keywords_with_longer_phrase() {
        assert_eq!(
            full_keyword("moz", &["moz", "mozi", "mozil", "mozill", "mozilla"]),
            "mozilla".to_owned()
        );
        assert_eq!(
            full_keyword(
                "mozilla f",
                &["moz", "mozi", "mozil", "mozill", "mozilla firefox"]
            ),
            "mozilla firefox".to_owned()
        );
    }

    #[test]
    fn query_ends_with_space() {
        assert_eq!(
            full_keyword(
                "mozilla ",
                &["moz", "mozi", "mozil", "mozill", "mozilla firefox"]
            ),
            "mozilla firefox".to_owned()
        );
    }
}
