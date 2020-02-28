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
    /// A character that can start an identifier, stable under NFKC.
    pub struct XidStart(bool) {
        abbr => "XIDS";
        long => "XID_Start";
        human => "XID Start";

        data_table_path => "../tables/xid_start.rsv";
    }

    /// Is this a NFKC-safe identifier starting character?
    pub fn is_xid_start(char) -> bool;
}

char_property! {
    /// A character that can continue an identifier, stable under NFKC.
    pub struct XidContinue(bool) {
        abbr => "XIDC";
        long => "XID_Continue";
        human => "XID Continue";

        data_table_path => "../tables/xid_continue.rsv";
    }

    /// Is this a NFKC-safe identifier continuing character?
    pub fn is_xid_continue(char) -> bool;
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_is_xid_start() {
        use super::is_xid_start;

        // ASCII
        assert_eq!(is_xid_start('\u{0000}'), false);
        assert_eq!(is_xid_start('\u{0020}'), false);
        assert_eq!(is_xid_start('\u{0021}'), false);

        assert_eq!(is_xid_start('\u{0027}'), false);
        assert_eq!(is_xid_start('\u{0028}'), false);
        assert_eq!(is_xid_start('\u{0029}'), false);
        assert_eq!(is_xid_start('\u{002a}'), false);

        assert_eq!(is_xid_start('\u{0030}'), false);
        assert_eq!(is_xid_start('\u{0039}'), false);
        assert_eq!(is_xid_start('\u{003a}'), false);
        assert_eq!(is_xid_start('\u{003b}'), false);
        assert_eq!(is_xid_start('\u{003c}'), false);
        assert_eq!(is_xid_start('\u{003d}'), false);

        assert_eq!(is_xid_start('\u{004a}'), true);
        assert_eq!(is_xid_start('\u{004b}'), true);
        assert_eq!(is_xid_start('\u{004c}'), true);
        assert_eq!(is_xid_start('\u{004d}'), true);
        assert_eq!(is_xid_start('\u{004e}'), true);

        assert_eq!(is_xid_start('\u{006a}'), true);
        assert_eq!(is_xid_start('\u{006b}'), true);
        assert_eq!(is_xid_start('\u{006c}'), true);
        assert_eq!(is_xid_start('\u{006d}'), true);
        assert_eq!(is_xid_start('\u{006e}'), true);

        assert_eq!(is_xid_start('\u{007a}'), true);
        assert_eq!(is_xid_start('\u{007b}'), false);
        assert_eq!(is_xid_start('\u{007c}'), false);
        assert_eq!(is_xid_start('\u{007d}'), false);
        assert_eq!(is_xid_start('\u{007e}'), false);

        assert_eq!(is_xid_start('\u{00c0}'), true);
        assert_eq!(is_xid_start('\u{00c1}'), true);
        assert_eq!(is_xid_start('\u{00c2}'), true);
        assert_eq!(is_xid_start('\u{00c3}'), true);
        assert_eq!(is_xid_start('\u{00c4}'), true);

        // Other BMP
        assert_eq!(is_xid_start('\u{061b}'), false);
        assert_eq!(is_xid_start('\u{061c}'), false);
        assert_eq!(is_xid_start('\u{061d}'), false);

        assert_eq!(is_xid_start('\u{200d}'), false);
        assert_eq!(is_xid_start('\u{200e}'), false);
        assert_eq!(is_xid_start('\u{200f}'), false);
        assert_eq!(is_xid_start('\u{2010}'), false);

        assert_eq!(is_xid_start('\u{2029}'), false);
        assert_eq!(is_xid_start('\u{202a}'), false);
        assert_eq!(is_xid_start('\u{202e}'), false);
        assert_eq!(is_xid_start('\u{202f}'), false);

        // Other Planes
        assert_eq!(is_xid_start('\u{10000}'), true);
        assert_eq!(is_xid_start('\u{10001}'), true);

        assert_eq!(is_xid_start('\u{20000}'), true);
        assert_eq!(is_xid_start('\u{30000}'), false);
        assert_eq!(is_xid_start('\u{40000}'), false);
        assert_eq!(is_xid_start('\u{50000}'), false);
        assert_eq!(is_xid_start('\u{60000}'), false);
        assert_eq!(is_xid_start('\u{70000}'), false);
        assert_eq!(is_xid_start('\u{80000}'), false);
        assert_eq!(is_xid_start('\u{90000}'), false);
        assert_eq!(is_xid_start('\u{a0000}'), false);
        assert_eq!(is_xid_start('\u{b0000}'), false);
        assert_eq!(is_xid_start('\u{c0000}'), false);
        assert_eq!(is_xid_start('\u{d0000}'), false);
        assert_eq!(is_xid_start('\u{e0000}'), false);

        assert_eq!(is_xid_start('\u{efffe}'), false);
        assert_eq!(is_xid_start('\u{effff}'), false);

        // Priavte-Use Area
        assert_eq!(is_xid_start('\u{f0000}'), false);
        assert_eq!(is_xid_start('\u{f0001}'), false);
        assert_eq!(is_xid_start('\u{ffffe}'), false);
        assert_eq!(is_xid_start('\u{fffff}'), false);
        assert_eq!(is_xid_start('\u{100000}'), false);
        assert_eq!(is_xid_start('\u{100001}'), false);
        assert_eq!(is_xid_start('\u{10fffe}'), false);
        assert_eq!(is_xid_start('\u{10ffff}'), false);
    }

    #[test]
    fn test_is_xid_continue() {
        use super::is_xid_continue;

        // ASCII
        assert_eq!(is_xid_continue('\u{0000}'), false);
        assert_eq!(is_xid_continue('\u{0020}'), false);
        assert_eq!(is_xid_continue('\u{0021}'), false);

        assert_eq!(is_xid_continue('\u{0027}'), false);
        assert_eq!(is_xid_continue('\u{0028}'), false);
        assert_eq!(is_xid_continue('\u{0029}'), false);
        assert_eq!(is_xid_continue('\u{002a}'), false);

        assert_eq!(is_xid_continue('\u{0030}'), true);
        assert_eq!(is_xid_continue('\u{0039}'), true);
        assert_eq!(is_xid_continue('\u{003a}'), false);
        assert_eq!(is_xid_continue('\u{003b}'), false);
        assert_eq!(is_xid_continue('\u{003c}'), false);
        assert_eq!(is_xid_continue('\u{003d}'), false);

        assert_eq!(is_xid_continue('\u{004a}'), true);
        assert_eq!(is_xid_continue('\u{004b}'), true);
        assert_eq!(is_xid_continue('\u{004c}'), true);
        assert_eq!(is_xid_continue('\u{004d}'), true);
        assert_eq!(is_xid_continue('\u{004e}'), true);

        assert_eq!(is_xid_continue('\u{006a}'), true);
        assert_eq!(is_xid_continue('\u{006b}'), true);
        assert_eq!(is_xid_continue('\u{006c}'), true);
        assert_eq!(is_xid_continue('\u{006d}'), true);
        assert_eq!(is_xid_continue('\u{006e}'), true);

        assert_eq!(is_xid_continue('\u{007a}'), true);
        assert_eq!(is_xid_continue('\u{007b}'), false);
        assert_eq!(is_xid_continue('\u{007c}'), false);
        assert_eq!(is_xid_continue('\u{007d}'), false);
        assert_eq!(is_xid_continue('\u{007e}'), false);

        assert_eq!(is_xid_continue('\u{00c0}'), true);
        assert_eq!(is_xid_continue('\u{00c1}'), true);
        assert_eq!(is_xid_continue('\u{00c2}'), true);
        assert_eq!(is_xid_continue('\u{00c3}'), true);
        assert_eq!(is_xid_continue('\u{00c4}'), true);

        // Other BMP
        assert_eq!(is_xid_continue('\u{061b}'), false);
        assert_eq!(is_xid_continue('\u{061c}'), false);
        assert_eq!(is_xid_continue('\u{061d}'), false);

        assert_eq!(is_xid_continue('\u{200d}'), false);
        assert_eq!(is_xid_continue('\u{200e}'), false);
        assert_eq!(is_xid_continue('\u{200f}'), false);
        assert_eq!(is_xid_continue('\u{2010}'), false);

        assert_eq!(is_xid_continue('\u{2029}'), false);
        assert_eq!(is_xid_continue('\u{202a}'), false);
        assert_eq!(is_xid_continue('\u{202e}'), false);
        assert_eq!(is_xid_continue('\u{202f}'), false);

        // Other Planes
        assert_eq!(is_xid_continue('\u{10000}'), true);
        assert_eq!(is_xid_continue('\u{10001}'), true);

        assert_eq!(is_xid_continue('\u{20000}'), true);
        assert_eq!(is_xid_continue('\u{30000}'), false);
        assert_eq!(is_xid_continue('\u{40000}'), false);
        assert_eq!(is_xid_continue('\u{50000}'), false);
        assert_eq!(is_xid_continue('\u{60000}'), false);
        assert_eq!(is_xid_continue('\u{70000}'), false);
        assert_eq!(is_xid_continue('\u{80000}'), false);
        assert_eq!(is_xid_continue('\u{90000}'), false);
        assert_eq!(is_xid_continue('\u{a0000}'), false);
        assert_eq!(is_xid_continue('\u{b0000}'), false);
        assert_eq!(is_xid_continue('\u{c0000}'), false);
        assert_eq!(is_xid_continue('\u{d0000}'), false);
        assert_eq!(is_xid_continue('\u{e0000}'), false);

        assert_eq!(is_xid_continue('\u{efffe}'), false);
        assert_eq!(is_xid_continue('\u{effff}'), false);

        // Priavte-Use Area
        assert_eq!(is_xid_continue('\u{f0000}'), false);
        assert_eq!(is_xid_continue('\u{f0001}'), false);
        assert_eq!(is_xid_continue('\u{ffffe}'), false);
        assert_eq!(is_xid_continue('\u{fffff}'), false);
        assert_eq!(is_xid_continue('\u{100000}'), false);
        assert_eq!(is_xid_continue('\u{100001}'), false);
        assert_eq!(is_xid_continue('\u{10fffe}'), false);
        assert_eq!(is_xid_continue('\u{10ffff}'), false);
    }
}
