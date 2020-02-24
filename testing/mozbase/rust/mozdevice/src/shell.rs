// Copyright (c) 2017 Jimmy Cuadra
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

use regex::Regex;

/// Escapes a string so it will be interpreted as a single word by the UNIX Bourne shell.
///
/// If the input string is empty, this function returns an empty quoted string.
pub fn escape(input: &str) -> String {
    // Stolen from https://docs.rs/shellwords/1.0.0/src/shellwords/lib.rs.html#24-37.
    let escape_pattern: Regex = Regex::new(r"([^A-Za-z0-9_\-.,:/@\n])").unwrap();
    let line_feed: Regex = Regex::new(r"\n").unwrap();

    if input.is_empty() {
        return "''".to_owned();
    }

    let output = &escape_pattern.replace_all(input, "\\$1");

    line_feed.replace_all(output, "'\n'").to_string()
}

#[cfg(test)]
mod tests {
    use super::escape;

    #[test]
    fn empty_escape() {
        assert_eq!(escape(""), "''");
    }

    #[test]
    fn full_escape() {
        assert_eq!(escape("foo '\"' bar"), "foo\\ \\'\\\"\\'\\ bar");
    }

    #[test]
    fn escape_multibyte() {
        assert_eq!(escape("あい"), "\\あ\\い");
    }
}
