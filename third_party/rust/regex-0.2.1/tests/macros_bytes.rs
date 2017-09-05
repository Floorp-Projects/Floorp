// Macros for use in writing tests generic over &str/&[u8].
macro_rules! text { ($text:expr) => { $text.as_bytes() } }
macro_rules! t { ($re:expr) => { text!($re) } }
macro_rules! match_text { ($text:expr) => { $text.as_bytes() } }

macro_rules! bytes { ($text:expr) => { $text } }
macro_rules! b { ($text:expr) => { bytes!($text) } }

// macro_rules! u { ($re:expr) => { concat!("(?u)", $re) } }

macro_rules! no_expand {
    ($text:expr) => {{
        use regex::bytes::NoExpand;
        NoExpand(text!($text))
    }}
}

macro_rules! show {
    ($text:expr) => {{
        use std::ascii::escape_default;
        let mut s = vec![];
        for &b in bytes!($text) {
            s.extend(escape_default(b));
        }
        String::from_utf8(s).unwrap()
    }}
}

macro_rules! expand {
    ($name:ident, $re:expr, $text:expr, $expand:expr, $expected:expr) => {
        #[test]
        fn $name() {
            let re = regex!($re);
            let cap = re.captures(t!($text)).unwrap();

            let mut got = vec![];
            cap.expand(t!($expand), &mut got);
            assert_eq!(show!(t!($expected)), show!(&*got));
        }
    }
}
