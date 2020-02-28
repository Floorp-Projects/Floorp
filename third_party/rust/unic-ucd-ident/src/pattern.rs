// Copyright 2017-2019 The UNIC Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

char_property! {
    /// A character that should be treated as a syntax character in patterns.
    pub struct PatternSyntax(bool) {
        abbr => "Pat_Syn";
        long => "Pattern_Syntax";
        human => "Pattern Syntax";

        data_table_path => "../tables/pattern_syntax.rsv";
    }

    /// Is this a character that should be treated as syntax in patterns?
    pub fn is_pattern_syntax(char) -> bool;
}

char_property! {
    /// A character that should be treated as a whitespace in patterns.
    pub struct PatternWhitespace(bool) {
        abbr => "Pat_WS";
        long => "Pattern_White_Space";
        human => "Pattern Whitespace";

        data_table_path => "../tables/pattern_white_space.rsv";
    }

    /// Is this a character that should be treated as whitespace in patterns?
    pub fn is_pattern_whitespace(char) -> bool;
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_is_pattern_syntax() {
        use super::is_pattern_syntax;

        // ASCII
        assert_eq!(is_pattern_syntax('\u{0000}'), false);
        assert_eq!(is_pattern_syntax('\u{0020}'), false);
        assert_eq!(is_pattern_syntax('\u{0021}'), true);

        assert_eq!(is_pattern_syntax('\u{0027}'), true);
        assert_eq!(is_pattern_syntax('\u{0028}'), true);
        assert_eq!(is_pattern_syntax('\u{0029}'), true);
        assert_eq!(is_pattern_syntax('\u{002a}'), true);

        assert_eq!(is_pattern_syntax('\u{0030}'), false);
        assert_eq!(is_pattern_syntax('\u{0039}'), false);
        assert_eq!(is_pattern_syntax('\u{003a}'), true);
        assert_eq!(is_pattern_syntax('\u{003b}'), true);
        assert_eq!(is_pattern_syntax('\u{003c}'), true);
        assert_eq!(is_pattern_syntax('\u{003d}'), true);

        assert_eq!(is_pattern_syntax('\u{004a}'), false);
        assert_eq!(is_pattern_syntax('\u{004b}'), false);
        assert_eq!(is_pattern_syntax('\u{004c}'), false);
        assert_eq!(is_pattern_syntax('\u{004d}'), false);
        assert_eq!(is_pattern_syntax('\u{004e}'), false);

        assert_eq!(is_pattern_syntax('\u{006a}'), false);
        assert_eq!(is_pattern_syntax('\u{006b}'), false);
        assert_eq!(is_pattern_syntax('\u{006c}'), false);
        assert_eq!(is_pattern_syntax('\u{006d}'), false);
        assert_eq!(is_pattern_syntax('\u{006e}'), false);

        assert_eq!(is_pattern_syntax('\u{007a}'), false);
        assert_eq!(is_pattern_syntax('\u{007b}'), true);
        assert_eq!(is_pattern_syntax('\u{007c}'), true);
        assert_eq!(is_pattern_syntax('\u{007d}'), true);
        assert_eq!(is_pattern_syntax('\u{007e}'), true);

        assert_eq!(is_pattern_syntax('\u{00c0}'), false);
        assert_eq!(is_pattern_syntax('\u{00c1}'), false);
        assert_eq!(is_pattern_syntax('\u{00c2}'), false);
        assert_eq!(is_pattern_syntax('\u{00c3}'), false);
        assert_eq!(is_pattern_syntax('\u{00c4}'), false);

        // Other BMP
        assert_eq!(is_pattern_syntax('\u{061b}'), false);
        assert_eq!(is_pattern_syntax('\u{061c}'), false);
        assert_eq!(is_pattern_syntax('\u{061d}'), false);

        assert_eq!(is_pattern_syntax('\u{200d}'), false);
        assert_eq!(is_pattern_syntax('\u{200e}'), false);
        assert_eq!(is_pattern_syntax('\u{200f}'), false);
        assert_eq!(is_pattern_syntax('\u{2010}'), true);

        assert_eq!(is_pattern_syntax('\u{2029}'), false);
        assert_eq!(is_pattern_syntax('\u{202a}'), false);
        assert_eq!(is_pattern_syntax('\u{202e}'), false);
        assert_eq!(is_pattern_syntax('\u{202f}'), false);

        // Other Planes
        assert_eq!(is_pattern_syntax('\u{10000}'), false);
        assert_eq!(is_pattern_syntax('\u{10001}'), false);

        assert_eq!(is_pattern_syntax('\u{20000}'), false);
        assert_eq!(is_pattern_syntax('\u{30000}'), false);
        assert_eq!(is_pattern_syntax('\u{40000}'), false);
        assert_eq!(is_pattern_syntax('\u{50000}'), false);
        assert_eq!(is_pattern_syntax('\u{60000}'), false);
        assert_eq!(is_pattern_syntax('\u{70000}'), false);
        assert_eq!(is_pattern_syntax('\u{80000}'), false);
        assert_eq!(is_pattern_syntax('\u{90000}'), false);
        assert_eq!(is_pattern_syntax('\u{a0000}'), false);
        assert_eq!(is_pattern_syntax('\u{b0000}'), false);
        assert_eq!(is_pattern_syntax('\u{c0000}'), false);
        assert_eq!(is_pattern_syntax('\u{d0000}'), false);
        assert_eq!(is_pattern_syntax('\u{e0000}'), false);

        assert_eq!(is_pattern_syntax('\u{efffe}'), false);
        assert_eq!(is_pattern_syntax('\u{effff}'), false);

        // Priavte-Use Area
        assert_eq!(is_pattern_syntax('\u{f0000}'), false);
        assert_eq!(is_pattern_syntax('\u{f0001}'), false);
        assert_eq!(is_pattern_syntax('\u{ffffe}'), false);
        assert_eq!(is_pattern_syntax('\u{fffff}'), false);
        assert_eq!(is_pattern_syntax('\u{100000}'), false);
        assert_eq!(is_pattern_syntax('\u{100001}'), false);
        assert_eq!(is_pattern_syntax('\u{10fffe}'), false);
        assert_eq!(is_pattern_syntax('\u{10ffff}'), false);
    }

    #[test]
    fn test_is_pattern_whitespace() {
        use super::is_pattern_whitespace;

        // ASCII
        assert_eq!(is_pattern_whitespace('\u{0000}'), false);
        assert_eq!(is_pattern_whitespace('\u{0020}'), true);
        assert_eq!(is_pattern_whitespace('\u{0021}'), false);

        assert_eq!(is_pattern_whitespace('\u{0027}'), false);
        assert_eq!(is_pattern_whitespace('\u{0028}'), false);
        assert_eq!(is_pattern_whitespace('\u{0029}'), false);
        assert_eq!(is_pattern_whitespace('\u{002a}'), false);

        assert_eq!(is_pattern_whitespace('\u{0030}'), false);
        assert_eq!(is_pattern_whitespace('\u{0039}'), false);
        assert_eq!(is_pattern_whitespace('\u{003a}'), false);
        assert_eq!(is_pattern_whitespace('\u{003b}'), false);
        assert_eq!(is_pattern_whitespace('\u{003c}'), false);
        assert_eq!(is_pattern_whitespace('\u{003d}'), false);

        assert_eq!(is_pattern_whitespace('\u{004a}'), false);
        assert_eq!(is_pattern_whitespace('\u{004b}'), false);
        assert_eq!(is_pattern_whitespace('\u{004c}'), false);
        assert_eq!(is_pattern_whitespace('\u{004d}'), false);
        assert_eq!(is_pattern_whitespace('\u{004e}'), false);

        assert_eq!(is_pattern_whitespace('\u{006a}'), false);
        assert_eq!(is_pattern_whitespace('\u{006b}'), false);
        assert_eq!(is_pattern_whitespace('\u{006c}'), false);
        assert_eq!(is_pattern_whitespace('\u{006d}'), false);
        assert_eq!(is_pattern_whitespace('\u{006e}'), false);

        assert_eq!(is_pattern_whitespace('\u{007a}'), false);
        assert_eq!(is_pattern_whitespace('\u{007b}'), false);
        assert_eq!(is_pattern_whitespace('\u{007c}'), false);
        assert_eq!(is_pattern_whitespace('\u{007d}'), false);
        assert_eq!(is_pattern_whitespace('\u{007e}'), false);

        assert_eq!(is_pattern_whitespace('\u{00c0}'), false);
        assert_eq!(is_pattern_whitespace('\u{00c1}'), false);
        assert_eq!(is_pattern_whitespace('\u{00c2}'), false);
        assert_eq!(is_pattern_whitespace('\u{00c3}'), false);
        assert_eq!(is_pattern_whitespace('\u{00c4}'), false);

        // Other BMP
        assert_eq!(is_pattern_whitespace('\u{061b}'), false);
        assert_eq!(is_pattern_whitespace('\u{061c}'), false);
        assert_eq!(is_pattern_whitespace('\u{061d}'), false);

        assert_eq!(is_pattern_whitespace('\u{200d}'), false);
        assert_eq!(is_pattern_whitespace('\u{200e}'), true);
        assert_eq!(is_pattern_whitespace('\u{200f}'), true);
        assert_eq!(is_pattern_whitespace('\u{2010}'), false);

        assert_eq!(is_pattern_whitespace('\u{2029}'), true);
        assert_eq!(is_pattern_whitespace('\u{202a}'), false);
        assert_eq!(is_pattern_whitespace('\u{202e}'), false);
        assert_eq!(is_pattern_whitespace('\u{202f}'), false);

        // Other Planes
        assert_eq!(is_pattern_whitespace('\u{10000}'), false);
        assert_eq!(is_pattern_whitespace('\u{10001}'), false);

        assert_eq!(is_pattern_whitespace('\u{20000}'), false);
        assert_eq!(is_pattern_whitespace('\u{30000}'), false);
        assert_eq!(is_pattern_whitespace('\u{40000}'), false);
        assert_eq!(is_pattern_whitespace('\u{50000}'), false);
        assert_eq!(is_pattern_whitespace('\u{60000}'), false);
        assert_eq!(is_pattern_whitespace('\u{70000}'), false);
        assert_eq!(is_pattern_whitespace('\u{80000}'), false);
        assert_eq!(is_pattern_whitespace('\u{90000}'), false);
        assert_eq!(is_pattern_whitespace('\u{a0000}'), false);
        assert_eq!(is_pattern_whitespace('\u{b0000}'), false);
        assert_eq!(is_pattern_whitespace('\u{c0000}'), false);
        assert_eq!(is_pattern_whitespace('\u{d0000}'), false);
        assert_eq!(is_pattern_whitespace('\u{e0000}'), false);

        assert_eq!(is_pattern_whitespace('\u{efffe}'), false);
        assert_eq!(is_pattern_whitespace('\u{effff}'), false);

        // Priavte-Use Area
        assert_eq!(is_pattern_whitespace('\u{f0000}'), false);
        assert_eq!(is_pattern_whitespace('\u{f0001}'), false);
        assert_eq!(is_pattern_whitespace('\u{ffffe}'), false);
        assert_eq!(is_pattern_whitespace('\u{fffff}'), false);
        assert_eq!(is_pattern_whitespace('\u{100000}'), false);
        assert_eq!(is_pattern_whitespace('\u{100001}'), false);
        assert_eq!(is_pattern_whitespace('\u{10fffe}'), false);
        assert_eq!(is_pattern_whitespace('\u{10ffff}'), false);
    }
}
