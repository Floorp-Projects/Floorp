/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This crate implements a prefs file parser.
//!
//! Pref files have the following grammar.
//!
//! <pref-file>   = <pref>*
//! <pref>        = <pref-spec> "(" <pref-name> "," <pref-value> ")" ";"
//! <pref-spec>   = "user_pref" | "pref" | "sticky_pref"
//! <pref-name>   = <string-literal>
//! <pref-value>  = <string-literal> | "true" | "false" | <int-value>
//! <int-value>   = <sign>? <int-literal>
//! <sign>        = "+" | "-"
//! <int-literal> = [0-9]+ (and cannot be followed by [A-Za-z_])
//! <string-literal> =
//!   A single or double-quoted string, with the following escape sequences
//!   allowed: \", \', \\, \n, \r, \xNN, \uNNNN, where \xNN gives a raw byte
//!   value that is copied directly into an 8-bit string value, and \uNNNN
//!   gives a UTF-16 code unit that is converted to UTF-8 before being copied
//!   into an 8-bit string value. \x00 and \u0000 are disallowed because they
//!   would cause C++ code handling such strings to misbehave.
//!
//! Comments can take three forms:
//! - # Python-style comments
//! - // C++ style comments
//! - /* C style comments (non-nested) */
//!
//! Non-end-of-line whitespace chars are \t, \v, \f, and space.
//!
//! End-of-line sequences can take three forms, each of which is considered as a
//! single EoL:
//! - \n
//! - \r (without subsequent \n)
//! - \r\n
//!
//! The valid range for <int-value> is -2,147,483,648..2,147,483,647. Values
//! outside that range will result in a parse error.
//!
//! A '\0' char is interpreted as the end of the file. The use of this character
//! in a prefs file is not recommended. Within string literals \x00 or \u0000
//! can be used instead.

// This parser uses several important optimizations.
//
// - Because "'\0' means EOF" is part of the grammar (see above) we can match
//   EOF as a normal char/token, which means we can avoid a typical "do we
//   still have chars remaining?" test in get_char(), which gives a speedup
//   because get_char() is a very hot function. (Actually, Rust would
//   bounds-check this function anyway, so we have get_char_unchecked() which
//   is used for the two hottest call sites.)
//
//   This also means EOF is representable by a u8. If EOF was represented by an
//   out-of-band value such as -1 or 256, we'd have to return a larger type
//   such as u16 or i16 from get_char().
//
// - When starting a new token, it uses a lookup table with the first char,
//   which quickly identifies what kind of token it will be. Furthermore, if
//   that token is an unambiguous single-char token (e.g. '(', ')', '+', ',',
//   '-', ';'), the parser will return the appropriate token kind value at
//   minimal cost because the single-char tokens have a uniform representation.
//
// - It has a lookup table that identifies chars in string literals that need
//   special handling. This means non-special chars (the common case) can be
//   handled with a single test, rather than testing for the multiple special
//   cases.
//
// - It pre-scans string literals for special chars. If none are present, it
//   bulk copies the string literal into a Vec, which is faster than doing a
//   char-by-char copy.
//
// - It reuses Vecs to avoid creating a new one for each string literal.

use std::os::raw::{c_char, c_uchar};

//---------------------------------------------------------------------------
// The public interface
//---------------------------------------------------------------------------

/// Keep this in sync with PrefType in Preferences.cpp.
#[derive(Clone, Copy, Debug)]
#[repr(u8)]
pub enum PrefType {
    None,
    String,
    Int,
    Bool,
}

/// Keep this in sync with PrefValueKind in Preferences.h.
#[derive(Clone, Copy, Debug)]
#[repr(u8)]
pub enum PrefValueKind {
    Default,
    User
}

/// Keep this in sync with PrefValue in Preferences.cpp.
#[repr(C)]
pub union PrefValue {
    string_val: *const c_char,
    int_val: i32,
    bool_val: bool,
}

/// Keep this in sync with PrefsParserPrefFn in Preferences.cpp.
type PrefFn = unsafe extern "C" fn(pref_name: *const c_char, pref_type: PrefType,
                                   pref_value_kind: PrefValueKind, pref_value: PrefValue,
                                   is_sticky: bool);

/// Keep this in sync with PrefsParserErrorFn in Preferences.cpp.
type ErrorFn = unsafe extern "C" fn(msg: *const c_char);

/// Parse the contents of a prefs file.
///
/// `buf` is a null-terminated string. `len` is its length, excluding the
/// null terminator.
///
/// Keep this in sync with the prefs_parser_parse() declaration in
/// Preferences.cpp.
#[no_mangle]
pub extern "C" fn prefs_parser_parse(path: *const c_char, buf: *const c_char, len: usize,
                                     pref_fn: PrefFn, error_fn: ErrorFn) -> bool {
    let path = unsafe { std::ffi::CStr::from_ptr(path).to_string_lossy().into_owned() };

    // Make sure `buf` ends in a '\0', and include that in the length, because
    // it represents EOF.
    let buf = unsafe { std::slice::from_raw_parts(buf as *const c_uchar, len + 1) };
    assert!(buf.last() == Some(&EOF));

    let mut parser = Parser::new(&path, &buf, pref_fn, error_fn);
    parser.parse()
}

//---------------------------------------------------------------------------
// The implementation
//---------------------------------------------------------------------------

#[derive(Clone, Copy, Debug, PartialEq)]
enum Token {
    // Unambiguous single-char tokens.
    SingleChar(u8),

    // Keywords
    Pref,       // pref
    StickyPref, // sticky_pref
    UserPref,   // user_pref
    True,       // true
    False,      // false

    // String literal, e.g. '"string"'. The value is stored elsewhere.
    String,

    // Unsigned integer literal, e.g. '123'. Although libpref uses i32 values,
    // any '-' and '+' before an integer literal are treated as separate
    // tokens, so these token values are always positive. Furthermore, we
    // tokenize int literals as u32 so that 2147483648 (which doesn't fit into
    // an i32) can be subsequently negated to -2147483648 (which does fit into
    // an i32) if a '-' token precedes it.
    Int(u32),

    // Malformed token.
    Error(&'static str),
}

// We categorize every char by what action should be taken when it appears at
// the start of a new token.
#[derive(Clone, Copy, PartialEq)]
enum CharKind {
    // These are ordered by frequency. See the comment in GetToken().
    SingleChar, // Unambiguous single-char tokens: [()+,-]
    SpaceNL,    // [\t\v\f \n]
    Keyword,    // [A-Za-z_]
    Quote,      // ["']
    Slash,      // /
    Digit,      // [0-9]
    Hash,       // #
    CR,         // \r
    Other       // Everything else; invalid except within strings and comments.
}

const C_SINGL: CharKind = CharKind::SingleChar;
const C_SPCNL: CharKind = CharKind::SpaceNL;
const C_KEYWD: CharKind = CharKind::Keyword;
const C_QUOTE: CharKind = CharKind::Quote;
const C_SLASH: CharKind = CharKind::Slash;
const C_DIGIT: CharKind = CharKind::Digit;
const C_HASH : CharKind = CharKind::Hash;
const C_CR   : CharKind = CharKind::CR;
const C______: CharKind = CharKind::Other;

const CHAR_KINDS: [CharKind; 256] = [
/*         0        1        2        3        4        5        6        7        8        9    */
/*   0+ */ C_SINGL, C______, C______, C______, C______, C______, C______, C______, C______, C_SPCNL,
/*  10+ */ C_SPCNL, C_SPCNL, C_SPCNL, C_CR   , C______, C______, C______, C______, C______, C______,
/*  20+ */ C______, C______, C______, C______, C______, C______, C______, C______, C______, C______,
/*  30+ */ C______, C______, C_SPCNL, C______, C_QUOTE, C_HASH , C______, C______, C______, C_QUOTE,
/*  40+ */ C_SINGL, C_SINGL, C______, C_SINGL, C_SINGL, C_SINGL, C______, C_SLASH, C_DIGIT, C_DIGIT,
/*  50+ */ C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C______, C_SINGL,
/*  60+ */ C______, C______, C______, C______, C______, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD,
/*  70+ */ C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD,
/*  80+ */ C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD,
/*  90+ */ C_KEYWD, C______, C______, C______, C______, C_KEYWD, C______, C_KEYWD, C_KEYWD, C_KEYWD,
/* 100+ */ C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD,
/* 110+ */ C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD, C_KEYWD,
/* 120+ */ C_KEYWD, C_KEYWD, C_KEYWD, C______, C______, C______, C______, C______, C______, C______,
/* 130+ */ C______, C______, C______, C______, C______, C______, C______, C______, C______, C______,
/* 140+ */ C______, C______, C______, C______, C______, C______, C______, C______, C______, C______,
/* 150+ */ C______, C______, C______, C______, C______, C______, C______, C______, C______, C______,
/* 160+ */ C______, C______, C______, C______, C______, C______, C______, C______, C______, C______,
/* 170+ */ C______, C______, C______, C______, C______, C______, C______, C______, C______, C______,
/* 180+ */ C______, C______, C______, C______, C______, C______, C______, C______, C______, C______,
/* 190+ */ C______, C______, C______, C______, C______, C______, C______, C______, C______, C______,
/* 200+ */ C______, C______, C______, C______, C______, C______, C______, C______, C______, C______,
/* 210+ */ C______, C______, C______, C______, C______, C______, C______, C______, C______, C______,
/* 220+ */ C______, C______, C______, C______, C______, C______, C______, C______, C______, C______,
/* 230+ */ C______, C______, C______, C______, C______, C______, C______, C______, C______, C______,
/* 240+ */ C______, C______, C______, C______, C______, C______, C______, C______, C______, C______,
/* 250+ */ C______, C______, C______, C______, C______, C______
];

const _______: bool = false;
const SPECIAL_STRING_CHARS: [bool; 256] = [
/*         0        1        2        3        4        5        6        7        8        9    */
/*   0+ */    true, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/*  10+ */    true, _______, _______,    true, _______, _______, _______, _______, _______, _______,
/*  20+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/*  30+ */ _______, _______, _______, _______,    true, _______, _______, _______, _______,    true,
/*  40+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/*  50+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/*  60+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/*  70+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/*  80+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/*  90+ */ _______, _______,    true, _______, _______, _______, _______, _______, _______, _______,
/* 100+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/* 110+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/* 120+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/* 130+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/* 140+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/* 150+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/* 160+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/* 170+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/* 180+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/* 190+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/* 200+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/* 210+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/* 220+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/* 230+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/* 240+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/* 250+ */ _______, _______, _______, _______, _______, _______
];

struct KeywordInfo {
  string: &'static [u8],
  token: Token,
}

const KEYWORD_INFOS: &[KeywordInfo; 5] = &[
  // These are ordered by frequency.
  KeywordInfo { string: b"pref",        token: Token::Pref },
  KeywordInfo { string: b"true",        token: Token::True },
  KeywordInfo { string: b"false",       token: Token::False },
  KeywordInfo { string: b"user_pref",   token: Token::UserPref },
  KeywordInfo { string: b"sticky_pref", token: Token::StickyPref },
];

struct Parser<'t> {
    path: &'t str,      // Path to the file being parsed. Used in error messages.
    buf: &'t [u8],      // Text being parsed.
    i: usize,           // Index of next char to be read.
    line_num: u32,      // Current line number within the text.
    pref_fn: PrefFn,    // Callback for processing each pref.
    error_fn: ErrorFn,  // Callback for parse errors.
}

// As described above, we use 0 to represent EOF.
const EOF: u8 = b'\0';

impl<'t> Parser<'t> {
    fn new(path: &'t str, buf: &'t [u8], pref_fn: PrefFn, error_fn: ErrorFn) -> Parser<'t> {
        // Make sure these tables take up 1 byte per entry.
        assert!(std::mem::size_of_val(&CHAR_KINDS) == 256);
        assert!(std::mem::size_of_val(&SPECIAL_STRING_CHARS) == 256);

        Parser {
            path: path,
            buf: buf,
            i: 0,
            line_num: 1,
            pref_fn: pref_fn,
            error_fn: error_fn,
        }
    }

    fn parse(&mut self) -> bool {
        // These are reused, because allocating a new Vec for every string is slow.
        let mut name_str  = Vec::with_capacity(128); // For pref names.
        let mut value_str = Vec::with_capacity(512); // For string pref values.
        let mut none_str  = Vec::with_capacity(0);   // For tokens that shouldn't be strings.

        loop {
            // Note: if you add error recovery here, be aware that the
            // erroneous char may have been the text-ending EOF, in which case
            // self.i will point one past the end of the text. You should check
            // for that possibility before getting more chars.

            // EOF?
            let token = self.get_token(&mut none_str);
            if token == Token::SingleChar(EOF) {
                break;
            }

            // <pref-spec>
            let (pref_value_kind, is_sticky) = match token {
                Token::Pref => {
                    (PrefValueKind::Default, false)
                }
                Token::StickyPref => {
                    (PrefValueKind::Default, true)
                }
                Token::UserPref => {
                    (PrefValueKind::User, false)
                }
                _ => return self.error(token,
                                       "expected pref specifier at start of pref definition")
            };

            // "("
            let token = self.get_token(&mut none_str);
            if token != Token::SingleChar(b'(') {
                return self.error(token, "expected '(' after pref specifier");
            }

            // <pref-name>
            let token = self.get_token(&mut name_str);
            let pref_name = if token == Token::String {
                &name_str
            } else {
                return self.error(token, "expected pref name after '('");
            };

            // ","
            let token = self.get_token(&mut none_str);
            if token != Token::SingleChar(b',') {
                return self.error(token, "expected ',' after pref name");
            }

            // <pref-value>
            let token = self.get_token(&mut value_str);
            let (pref_type, pref_value) = match token {
                Token::True => {
                    (PrefType::Bool, PrefValue { bool_val: true })
                }
                Token::False => {
                    (PrefType::Bool, PrefValue { bool_val: false })
                }
                Token::String => {
                    (PrefType::String,
                     PrefValue { string_val: value_str.as_ptr() as *const c_char })

                }
                Token::Int(u) => {
                    // Accept u <= 2147483647; anything larger will overflow i32.
                    if u <= std::i32::MAX as u32 {
                        (PrefType::Int, PrefValue { int_val: u as i32 })
                    } else {
                        return self.error(Token::Error("integer literal overflowed"), "");
                    }

                }
                Token::SingleChar(b'-') => {
                    let token = self.get_token(&mut none_str);
                    if let Token::Int(u) = token {
                        // Accept u <= 2147483648; anything larger will overflow i32 once negated.
                        if u <= std::i32::MAX as u32 {
                            (PrefType::Int, PrefValue { int_val: -(u as i32) })
                        } else if u == std::i32::MAX as u32 + 1 {
                            (PrefType::Int, PrefValue { int_val: std::i32::MIN })
                        } else {
                            return self.error(Token::Error("integer literal overflowed"), "");
                        }
                    } else {
                        return self.error(token, "expected integer literal after '-'");
                    }

                }
                Token::SingleChar(b'+') => {
                    let token = self.get_token(&mut none_str);
                    if let Token::Int(u) = token {
                        // Accept u <= 2147483647; anything larger will overflow i32.
                        if u <= std::i32::MAX as u32 {
                            (PrefType::Int, PrefValue { int_val: u as i32 })
                        } else {
                            return self.error(Token::Error("integer literal overflowed"), "");
                        }
                    } else {
                        return self.error(token, "expected integer literal after '+'");
                    }

                }
                _ => return self.error(token, "expected pref value after ','")
            };

            // ")"
            let token = self.get_token(&mut none_str);
            if token != Token::SingleChar(b')') {
                return self.error(token, "expected ')' after pref value");
            }

            // ";"
            let token = self.get_token(&mut none_str);
            if token != Token::SingleChar(b';') {
                return self.error(token, "expected ';' after ')'");
            }

            unsafe { (self.pref_fn)(pref_name.as_ptr() as *const c_char, pref_type, pref_value_kind,
                                    pref_value, is_sticky) };
        }

        true
    }

    fn error(&self, token: Token, msg: &str) -> bool {
        // If `token` is a Token::Error, it's a lexing error and the error
        // message is within `token`. Otherwise, it's a parsing error and the
        // error message is in `msg`.
        let msg = if let Token::Error(token_msg) = token {
            token_msg
        } else {
            msg
        };
        let msg = format!("{}:{}: prefs parse error: {}", self.path, self.line_num, msg);
        let msg = std::ffi::CString::new(msg).unwrap();
        unsafe { (self.error_fn)(msg.as_ptr() as *const c_char) };

        false
    }

    #[inline(always)]
    fn get_char(&mut self) -> u8 {
        let c = self.buf[self.i];
        self.i += 1;
        c
    }

    // This function skips the bounds check. Using it at the hottest two call
    // sites gives a ~15% parsing speed boost.
    #[inline(always)]
    unsafe fn get_char_unchecked(&mut self) -> u8 {
        let c = *self.buf.get_unchecked(self.i);
        self.i += 1;
        c
    }

    #[inline(always)]
    fn unget_char(&mut self) {
        debug_assert!(self.i > 0);
        self.i -= 1;
    }

    #[inline(always)]
    fn match_char(&mut self, c: u8) -> bool {
        if self.buf[self.i] == c {
            self.i += 1;
            return true;
        }
        false
    }

    #[inline(always)]
    fn match_single_line_comment(&mut self) {
        loop {
            // To reach here, the previous char must have been '/', and
            // assertions elsewhere ensure that there must be at least one
            // subsequent char (the '\0' for EOF).
            let c = unsafe { self.get_char_unchecked() };

            // All the special chars have value <= b'\r'.
            if c > b'\r' {
                continue;
            }
            match c {
                b'\n' => {
                    self.line_num += 1;
                    break;
                }
                b'\r' => {
                    self.line_num += 1;
                    self.match_char(b'\n');
                    break;
                }
                EOF => {
                    // We must unget the EOF otherwise we'll read past it the
                    // next time around the main loop in get_token(), violating
                    // self.buf's bounds.
                    self.unget_char();
                    break;
                }
                _ => continue
            }
        }
    }

    // Returns false if we hit EOF without closing the comment.
    fn match_multi_line_comment(&mut self) -> bool
    {
        loop {
            match self.get_char() {
                b'*' => {
                    if self.match_char(b'/') {
                        return true;
                    }
                }
                b'\n' => {
                    self.line_num += 1;
                }
                b'\r' => {
                    self.line_num += 1;
                    self.match_char(b'\n');
                }
                EOF => {
                    return false
                }
                _ => continue
            }
        }
    }

    fn match_hex_digits(&mut self, ndigits: i32) -> Option<u16> {
        debug_assert!(ndigits == 2 || ndigits == 4);
        let mut value: u16 = 0;
        for _ in 0..ndigits {
            value = value << 4;
            match self.get_char() {
                c @ b'0'... b'9' => value += (c - b'0') as u16,
                c @ b'A'...b'F' => value += (c - b'A') as u16 + 10,
                c @ b'a'...b'f' => value += (c - b'a') as u16 + 10,
                _ => return None
            }
        }
        Some(value)
    }

    #[inline(always)]
    fn char_kind(c: u8) -> CharKind {
        // Use get_unchecked() because a u8 index cannot exceed this table's
        // bounds.
        unsafe { *CHAR_KINDS.get_unchecked(c as usize) }
    }

    #[inline(always)]
    fn is_special_string_char(c: u8) -> bool {
        // Use get_unchecked() because a u8 index cannot exceed this table's
        // bounds.
        unsafe { *SPECIAL_STRING_CHARS.get_unchecked(c as usize) }
    }

    // If the obtained Token has a value, it is put within the Token, unless
    // it's a string, in which case it's put in `str_buf`. This avoids
    // allocating a new Vec for every string, which is slow.
    fn get_token(&mut self, str_buf: &mut Vec<u8>) -> Token {
        loop {
            // Note: the following tests are ordered by frequency when parsing
            // greprefs.js:
            // - SingleChar      36.7%
            // - SpaceNL         27.7% (14.9% for spaces, 12.8% for NL)
            // - Keyword         13.4%
            // - Quote           11.4%
            // - Slash            8.1%
            // - Digit            2.7%
            // - Hash, CR, Other  0.0%

            let c = self.get_char();
            match Parser::char_kind(c) {
                CharKind::SingleChar => {
                    return Token::SingleChar(c);
                }
                CharKind::SpaceNL => {
                    // It's slightly faster to combine the handling of the
                    // space chars with NL than to handle them separately; we
                    // have an extra test for this case, but one fewer test for
                    // all the subsequent CharKinds.
                    if c == b'\n' {
                        self.line_num += 1;
                    }
                    continue;
                }
                CharKind::Keyword => {
                    let start = self.i - 1;
                    loop {
                        let c = self.get_char();
                        if Parser::char_kind(c) != CharKind::Keyword {
                            self.unget_char();
                            break;
                        }
                    }
                    for info in KEYWORD_INFOS.iter() {
                        if &self.buf[start..self.i] == info.string {
                            return info.token;
                        }
                    }
                    return Token::Error("unknown keyword");
                }
                CharKind::Quote => {
                    return self.get_string_token(c, str_buf);
                }
                CharKind::Slash => {
                    match self.get_char() {
                        b'/' => {
                            self.match_single_line_comment();
                        }
                        b'*' => {
                            if !self.match_multi_line_comment() {
                                return Token::Error("unterminated /* comment");
                            }
                        }
                        _ => return Token::Error("expected '/' or '*' after '/'")
                    }
                    continue;
                }
                CharKind::Digit => {
                    let mut value = (c - b'0') as u32;
                    loop {
                        let c = self.get_char();
                        match Parser::char_kind(c) {
                            CharKind::Digit => {
                                fn add_digit(v: u32, c: u8) -> Option<u32> {
                                    v.checked_mul(10)?.checked_add((c - b'0') as u32)
                                }
                                if let Some(v) = add_digit(value, c) {
                                    value = v;
                                } else {
                                    return Token::Error("integer literal overflowed");
                                }
                            }
                            CharKind::Keyword => {
                                // Reject things like "123foo".
                                return Token::Error(
                                    "unexpected character in integer literal");
                            }
                            _ => {
                                self.unget_char();
                                break;
                            }
                        }
                    }
                    return Token::Int(value);
                }
                CharKind::Hash => {
                    self.match_single_line_comment();
                    continue;
                }
                CharKind::CR => {
                    self.match_char(b'\n');
                    self.line_num += 1;
                    continue;
                }
                _ => return Token::Error("unexpected character")
            }
        }
    }

    // Always inline this because it has a single call site.
    #[inline(always)]
    fn get_string_token(&mut self, quote_char: u8, str_buf: &mut Vec<u8>) -> Token {
        // First scan through the string to see if it contains any chars that
        // need special handling.
        let start = self.i;
        let has_special_chars = loop {
            // To reach here, the previous char must have been a quote
            // (quote_char), and assertions elsewhere ensure that there must be
            // at least one subsequent char (the '\0' for EOF).
            let c = unsafe { self.get_char_unchecked() };
            if Parser::is_special_string_char(c) {
                break c != quote_char;
            }
        };

        // Clear str_buf's contents without changing its capacity.
        str_buf.clear();

        // If there are no special chars (the common case), we can bulk copy it
        // to str_buf. This is a lot faster than the char-by-char loop below.
        if !has_special_chars {
          str_buf.extend(&self.buf[start..self.i - 1]);
          str_buf.push(b'\0');
          return Token::String;
        }

        // There were special chars. Re-scan the string, filling in str_buf one
        // char at a time.
        self.i = start;
        loop {
            let c = self.get_char();
            let c2 = if !Parser::is_special_string_char(c) {
                c

            } else if c == quote_char {
                break;

            } else if c == b'\\' {
                match self.get_char() {
                    b'\"' => b'\"',
                    b'\'' => b'\'',
                    b'\\' => b'\\',
                    b'n'  => b'\n',
                    b'r'  => b'\r',
                    b'x'  => {
                        if let Some(value) = self.match_hex_digits(2) {
                            debug_assert!(value <= 0xff);
                            if value != 0 {
                                value as u8
                            } else {
                                return Token::Error("\\x00 is not allowed");
                            }
                        } else {
                            return Token::Error("malformed \\x escape sequence");
                        }
                    }
                    b'u' => {
                        if let Some(value) = self.match_hex_digits(4) {
                            let mut utf16 = vec![value];
                            if 0xd800 == (0xfc00 & value) {
                                // High surrogate value. Look for the low surrogate value.
                                if self.match_char(b'\\') && self.match_char(b'u') {
                                    if let Some(lo) = self.match_hex_digits(4) {
                                        if 0xdc00 == (0xfc00 & lo) {
                                            // Found a valid low surrogate.
                                            utf16.push(lo);
                                        } else {
                                            return Token::Error(
                                                "invalid low surrogate value after high surrogate");
                                        }
                                    }
                                }
                                if utf16.len() != 2 {
                                    return Token::Error(
                                        "expected low surrogate after high surrogate");
                                }
                            } else if value == 0 {
                                return Token::Error("\\u0000 is not allowed");
                            }

                            // Insert the UTF-16 sequence as UTF-8.
                            let utf8 = String::from_utf16(&utf16).unwrap();
                            str_buf.extend(utf8.as_bytes());
                        } else {
                            return Token::Error("malformed \\u escape sequence");
                        }
                        continue; // We don't want to str_buf.push(c2) below.
                    }
                    _ => return Token::Error("unexpected escape sequence character after '\\'")
                }

            } else if c == b'\n' {
                self.line_num += 1;
                c

            } else if c == b'\r' {
                self.line_num += 1;
                if self.match_char(b'\n') {
                    str_buf.push(b'\r');
                    b'\n'
                } else {
                    c
                }

            } else if c == EOF {
                return Token::Error("unterminated string literal");

            } else {
                // This case is only hit for the non-closing quote char.
                debug_assert!((c == b'\'' || c == b'\"') && c != quote_char);
                c
            };
            str_buf.push(c2);
        }
        str_buf.push(b'\0');
        return Token::String;
    }
}
