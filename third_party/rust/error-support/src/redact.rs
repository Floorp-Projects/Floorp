/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Functions to redact strings to remove PII before logging them

/// Redact a URL.
///
/// It's tricky to redact an URL without revealing PII.  We check for various known bad URL forms
/// and report them, otherwise we just log "<URL>".
pub fn redact_url(url: &str) -> String {
    if url.is_empty() {
        return "<URL (empty)>".to_string();
    }
    match url.find(':') {
        None => "<URL (no scheme)>".to_string(),
        Some(n) => {
            let mut chars = url[0..n].chars();
            match chars.next() {
                // No characters in the scheme
                None => return "<URL (empty scheme)>".to_string(),
                Some(c) => {
                    // First character must be alphabetic
                    if !c.is_ascii_alphabetic() {
                        return "<URL (invalid scheme)>".to_string();
                    }
                }
            }
            for c in chars {
                // Subsequent characters must be in the set ( alpha | digit | "+" | "-" | "." )
                if !(c.is_ascii_alphanumeric() || c == '+' || c == '-' || c == '.') {
                    return "<URL (invalid scheme)>".to_string();
                }
            }
            "<URL>".to_string()
        }
    }
}

/// Redact compact jwe string (Five base64 segments, separated by `.` chars)
pub fn redact_compact_jwe(url: &str) -> String {
    url.replace(|ch| ch != '.', "x")
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_redact_url() {
        assert_eq!(redact_url("http://some.website.com/index.html"), "<URL>");
        assert_eq!(redact_url("about:config"), "<URL>");
        assert_eq!(redact_url(""), "<URL (empty)>");
        assert_eq!(redact_url("://some.website.com/"), "<URL (empty scheme)>");
        assert_eq!(redact_url("some.website.com/"), "<URL (no scheme)>");
        assert_eq!(redact_url("some.website.com/"), "<URL (no scheme)>");
        assert_eq!(
            redact_url("abc%@=://some.website.com/"),
            "<URL (invalid scheme)>"
        );
        assert_eq!(
            redact_url("0https://some.website.com/"),
            "<URL (invalid scheme)>"
        );
        assert_eq!(
            redact_url("a+weird-but.lega1-SCHEME://some.website.com/"),
            "<URL>"
        );
    }

    #[test]
    fn test_redact_compact_jwe() {
        assert_eq!(redact_compact_jwe("abc.1234.x3243"), "xxx.xxxx.xxxxx")
    }
}
