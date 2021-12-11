// Copyright Mozilla Foundation
//
// Licensed under the Apache License (Version 2.0), or the MIT license,
// (the "Licenses") at your option. You may not use this file except in
// compliance with one of the Licenses. You may obtain copies of the
// Licenses at:
//
//    http://www.apache.org/licenses/LICENSE-2.0
//    http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Licenses is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the Licenses for the specific language governing permissions and
// limitations under the Licenses.

//! `xmldecl::parse()` extracts an encoding from an ASCII-based bogo-XML
//! declaration in `text/html` in a WebKit-compatible way.

extern crate encoding_rs;

fn position(needle: u8, haystack: &[u8]) -> Option<usize> {
    haystack.iter().position(|&x| x == needle)
}

// The standard library lacks subslice search.
// Since our needle, "encoding" is short, 'g' occurs in it only once,
// and the other letters we expect to skip over are "version", let's
// search for 'g' and verify match.
fn skip_encoding(hay: &[u8]) -> Option<&[u8]> {
    let mut haystack = hay;
    loop {
        if let Some(g) = position(b'g', haystack) {
            let (head, tail) = haystack.split_at(g + 1);
            if let Some(_) = head.strip_suffix(b"encoding") {
                return Some(tail);
            }
            haystack = tail;
        } else {
            return None;
        }
    }
}

/// Extracts an encoding from an ASCII-based bogo-XML declaration.
/// `bytes` must the prefix of a `text/html` resource. If it is longer
/// than 1024 bytes, only the first 1024 bytes are examined.
///
/// The intended use is that when the `meta` prescan fails, the HTML
/// parser will have buffered the first 1024 bytes at which point they
/// should be passed to this function.
pub fn parse(bytes: &[u8]) -> Option<&'static encoding_rs::Encoding> {
    let up_to_kilobyte = if bytes.len() > 1024 {
        &bytes[..1024]
    } else {
        bytes
    };
    if let Some(after_xml) = up_to_kilobyte.strip_prefix(b"<?xml") {
        if let Some(gt) = position(b'>', after_xml) {
            let until_gt = &after_xml[..gt];
            if let Some(tail) = skip_encoding(until_gt) {
                let mut pos = 0;
                loop {
                    if pos >= tail.len() {
                        return None;
                    }
                    let c = tail[pos];
                    pos += 1;
                    if c == b'=' {
                        break;
                    }
                    if c <= b' ' {
                        continue;
                    }
                    return None;
                }
                // pos is now the index of the byte after =
                let is_single_quoted;
                let label_start;
                loop {
                    if pos >= tail.len() {
                        return None;
                    }
                    let c = tail[pos];
                    pos += 1;
                    if c == b'"' {
                        is_single_quoted = false;
                        label_start = pos;
                        break;
                    }
                    if c == b'\'' {
                        is_single_quoted = true;
                        label_start = pos;
                        break;
                    }
                    if c <= b' ' {
                        continue;
                    }
                    return None;
                }
                loop {
                    if pos >= tail.len() {
                        return None;
                    }
                    let c = tail[pos];
                    if c <= b' ' {
                        return None;
                    }
                    if (c == b'"' && !is_single_quoted) || (c == b'\'' && is_single_quoted) {
                        let encoding = encoding_rs::Encoding::for_label(&tail[label_start..pos]);
                        if encoding == Some(encoding_rs::UTF_16LE)
                            || encoding == Some(encoding_rs::UTF_16BE)
                        {
                            return Some(encoding_rs::UTF_8);
                        }
                        return encoding;
                    }
                    pos += 1;
                }
            }
        }
    }
    None
}

// Any copyright to the test code below this comment is dedicated to the
// Public Domain. http://creativecommons.org/publicdomain/zero/1.0/

#[cfg(test)]
mod tests {
    use super::parse;
    #[test]
    fn baseline() {
        assert_eq!(
            parse(b"<?xml version=\"1.0\" encoding=\"windows-1251\"?>AAAA"),
            Some(encoding_rs::WINDOWS_1251)
        );
    }
    #[test]
    fn meta_encoding_before_charset() {
        assert_eq!(parse(b"<?xml version=\"1.0\" <meta encoding=\"windows-1251\" charset=\"windows-1253\"?>AAAA"), Some(encoding_rs::WINDOWS_1251));
    }
    #[test]
    fn lt() {
        assert_eq!(
            parse(b"<?xml<encoding=\"windows-1251\"?>AAAA"),
            Some(encoding_rs::WINDOWS_1251)
        );
    }
    #[test]
    fn unmatched_quotes() {
        assert_eq!(
            parse(b"<?xml version=\"1.0\" encoding=\"windows-1251'?>AAAA"),
            None
        );
    }
    #[test]
    fn no_version() {
        assert_eq!(
            parse(b"<?xml encoding=\"windows-1251\"?>AAAA"),
            Some(encoding_rs::WINDOWS_1251)
        );
    }
    #[test]
    fn no_quotes_space() {
        assert_eq!(
            parse(b"<?xml version=\"1.0\" encoding=windows-1251 ?>AAAA"),
            None
        );
    }
    #[test]
    fn no_quotes() {
        assert_eq!(
            parse(b"<?xml version=\"1.0\" encoding=windows-1251?>AAAA"),
            None
        );
    }
    #[test]
    fn no_space_no_version_line_breaks_trailing_body() {
        assert_eq!(
            parse(b"<?xmlencoding  \n = \n   'windows-1251'<body>AAAA"),
            Some(encoding_rs::WINDOWS_1251)
        );
    }
    #[test]
    fn space_before_label() {
        assert_eq!(
            parse(b"<?xml version=\"1.0\" encoding=\" windows-1251\"?>AAAA"),
            None
        );
    }
    #[test]
    fn space_after_label() {
        assert_eq!(
            parse(b"<?xml version=\"1.0\" encoding=\"windows-1251 \"?>AAAA"),
            None
        );
    }
    #[test]
    fn one_around_label() {
        assert_eq!(
            parse(b"<?xml version=\"1.0\" encoding=\"\x01windows-1251\x01\"?>AAAA"),
            None
        );
    }
    #[test]
    fn one_around_equals() {
        assert_eq!(
            parse(b"<?xml version=\"1.0\" encoding\x01=\x01\"windows-1251\"?>AAAA"),
            Some(encoding_rs::WINDOWS_1251)
        );
    }
    #[test]
    fn no_version_no_space_trailing_lt_without_question_mark() {
        assert_eq!(
            parse(b"<?xmlencoding=\"windows-1251\"<>AAAA"),
            Some(encoding_rs::WINDOWS_1251)
        );
    }
    #[test]
    fn no_version_no_space_spaces_around_equals_single_quotes_trailing_body() {
        assert_eq!(
            parse(b"<?xmlencoding   =   'windows-1251'<body>AAAA"),
            Some(encoding_rs::WINDOWS_1251)
        );
    }
    #[test]
    fn no_version_no_space_single_quotes_trailing_body() {
        assert_eq!(
            parse(b"<?xmlencoding='windows-1251'<body>AAAA"),
            Some(encoding_rs::WINDOWS_1251)
        );
    }
    #[test]
    fn no_version_no_space_double_quotes_trailing_body() {
        assert_eq!(
            parse(b"<?xmlencoding=\"windows-1251\"<body>AAAA"),
            Some(encoding_rs::WINDOWS_1251)
        );
    }
    #[test]
    fn no_version_no_space_no_trailing_question_mark() {
        assert_eq!(
            parse(b"<?xmlencoding=\"windows-1251\">AAAA"),
            Some(encoding_rs::WINDOWS_1251)
        );
    }
    #[test]
    fn no_version_no_space() {
        assert_eq!(
            parse(b"<?xmlencoding=\"windows-1251\"?>AAAA"),
            Some(encoding_rs::WINDOWS_1251)
        );
    }
    #[test]
    fn upper_case_xml() {
        assert_eq!(
            parse(b"<?XML version=\"1.0\" encoding=\"windows-1251\"?>AAAA"),
            None
        );
    }
    #[test]
    fn meta_charset_before_encoding() {
        assert_eq!(parse(b"<?xml version=\"1.0\" <meta charset=\"windows-1253\" encoding=\"windows-1251\"?>AAAA"), Some(encoding_rs::WINDOWS_1251));
    }
    #[test]
    fn lt_between_xml_and_encoding() {
        assert_eq!(
            parse(b"<?xml<encoding=\"windows-1251\"?>AAAA"),
            Some(encoding_rs::WINDOWS_1251)
        );
    }
    #[test]
    fn letter_between_xml_and_encoding() {
        assert_eq!(
            parse(b"<?xmlaencoding=\"windows-1251\"?>AAAA"),
            Some(encoding_rs::WINDOWS_1251)
        );
    }
    #[test]
    fn gt_between_xml_and_encoding() {
        assert_eq!(parse(b"<?xml>encoding=\"windows-1251\"?>"), None);
    }
    #[test]
    fn non_primary_label() {
        assert_eq!(
            parse(b"<?xml version=\"1.0\" encoding=\"cp1251\"?>AAAA"),
            Some(encoding_rs::WINDOWS_1251)
        );
    }
    #[test]
    fn upper_case_label() {
        assert_eq!(
            parse(b"<?xml version=\"1.0\" encoding=\"WINDOWS-1251\"?>AAAA"),
            Some(encoding_rs::WINDOWS_1251)
        );
    }
    #[test]
    fn upper_case_version() {
        assert_eq!(
            parse(b"<?xml VERSION=\"1.0\" encoding=\"windows-1251\"?>AAAA"),
            Some(encoding_rs::WINDOWS_1251)
        );
    }
    #[test]
    fn upper_case_encoding() {
        assert_eq!(
            parse(b"<?xml version=\"1.0\" ENCODING=\"windows-1251\"?>AAAA"),
            None
        );
    }
    #[test]
    fn space_before() {
        assert_eq!(
            parse(b" <?xml version=\"1.0\" encoding=\"windows-1251\"?>AAAA"),
            None
        );
    }
    #[test]
    fn encoding_equals_encoding() {
        assert_eq!(
            parse(b"<?xml version=\"1.0\" encoding=encoding=\"windows-1251\"?>AAAA"),
            None
        );
    }
    #[test]
    fn encodingencoding() {
        assert_eq!(
            parse(b"<?xml version=\"1.0\" encodingencoding=\"windows-1251\"?>AAAA"),
            None
        );
    }
    #[test]
    fn utf16() {
        assert_eq!(
            parse(b"<?xml version=\"1.0\" encoding=\"UTF-16\"?>AAAA"),
            Some(encoding_rs::UTF_8)
        );
    }
    #[test]
    fn utf16le() {
        assert_eq!(
            parse(b"<?xml version=\"1.0\" encoding=\"UTF-16LE\"?>AAAA"),
            Some(encoding_rs::UTF_8)
        );
    }
    #[test]
    fn utf16be() {
        assert_eq!(
            parse(b"<?xml version=\"1.0\" encoding=\"UTF-16BE\"?>AAAA"),
            Some(encoding_rs::UTF_8)
        );
    }
    #[test]
    fn bytes_1024() {
        let mut v = Vec::new();
        v.extend_from_slice(b"<?xml version=\"1.0\" encoding=\"windows-1251\"");
        while v.len() < 1022 {
            v.push(b' ');
        }
        v.extend_from_slice(b"?>AAAA");
        assert_eq!(v.len(), 1028);
        assert_eq!(parse(&v), Some(encoding_rs::WINDOWS_1251));
    }
    #[test]
    fn bytes_1025() {
        let mut v = Vec::new();
        v.extend_from_slice(b"<?xml version=\"1.0\" encoding=\"windows-1251\"");
        while v.len() < 1023 {
            v.push(b' ');
        }
        v.extend_from_slice(b"?>AAAA");
        assert_eq!(v.len(), 1029);
        assert_eq!(parse(&v), None);
    }
}
