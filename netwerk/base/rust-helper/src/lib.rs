/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate nserror;
use self::nserror::*;

extern crate nsstring;
use self::nsstring::{nsACString, nsCString};

extern crate thin_vec;
use self::thin_vec::ThinVec;

use std::fs::File;
use std::io::{self, BufRead};
use std::net::Ipv4Addr;

/// HTTP leading whitespace, defined in netwerk/protocol/http/nsHttp.h
static HTTP_LWS: &'static [u8] = &[' ' as u8, '\t' as u8];

/// Trim leading whitespace, trailing whitespace, and quality-value
/// from a token.
fn trim_token(token: &[u8]) -> &[u8] {
    // Trim left whitespace
    let ltrim = token
        .iter()
        .take_while(|c| HTTP_LWS.iter().any(|ws| &ws == c))
        .count();

    // Trim right whitespace
    // remove "; q=..." if present
    let rtrim = token[ltrim..]
        .iter()
        .take_while(|c| **c != (';' as u8) && HTTP_LWS.iter().all(|ws| ws != *c))
        .count();

    &token[ltrim..ltrim + rtrim]
}

#[no_mangle]
/// Allocates an nsACString that contains a ISO 639 language list
/// notated with HTTP "q" values for output with an HTTP Accept-Language
/// header. Previous q values will be stripped because the order of
/// the langs implies the q value. The q values are calculated by dividing
/// 1.0 amongst the number of languages present.
///
/// Ex: passing: "en, ja"
///     returns: "en,ja;q=0.5"
///
///     passing: "en, ja, fr_CA"
///     returns: "en,ja;q=0.7,fr_CA;q=0.3"
pub extern "C" fn rust_prepare_accept_languages<'a, 'b>(
    i_accept_languages: &'a nsACString,
    o_accept_languages: &'b mut nsACString,
) -> nsresult {
    if i_accept_languages.is_empty() {
        return NS_OK;
    }

    let make_tokens = || {
        i_accept_languages
            .split(|c| *c == (',' as u8))
            .map(|token| trim_token(token))
            .filter(|token| token.len() != 0)
    };

    let n = make_tokens().count();

    for (count_n, i_token) in make_tokens().enumerate() {
        // delimiter if not first item
        if count_n != 0 {
            o_accept_languages.append(",");
        }

        let token_pos = o_accept_languages.len();
        o_accept_languages.append(&i_token as &[u8]);

        {
            let o_token = o_accept_languages.to_mut();
            canonicalize_language_tag(&mut o_token[token_pos..]);
        }

        // Divide the quality-values evenly among the languages.
        let q = 1.0 - count_n as f32 / n as f32;

        let u: u32 = ((q + 0.005) * 100.0) as u32;
        // Only display q-value if less than 1.00.
        if u < 100 {
            // With a small number of languages, one decimal place is
            // enough to prevent duplicate q-values.
            // Also, trailing zeroes do not add any information, so
            // they can be removed.
            if n < 10 || u % 10 == 0 {
                let u = (u + 5) / 10;
                o_accept_languages.append(&format!(";q=0.{}", u));
            } else {
                // Values below 10 require zero padding.
                o_accept_languages.append(&format!(";q=0.{:02}", u));
            }
        }
    }

    NS_OK
}

/// Defines a consistent capitalization for a given language string.
///
/// # Arguments
/// * `token` - a narrow char slice describing a language.
///
/// Valid language tags are of the form
/// "*", "fr", "en-US", "es-419", "az-Arab", "x-pig-latin", "man-Nkoo-GN"
///
/// Language tags are defined in the
/// [rfc5646](https://tools.ietf.org/html/rfc5646) spec. According to
/// the spec:
///
/// > At all times, language tags and their subtags, including private
/// > use and extensions, are to be treated as case insensitive: there
/// > exist conventions for the capitalization of some of the subtags,
/// > but these MUST NOT be taken to carry meaning.
///
/// So why is this code even here? See bug 1108183, I guess.
fn canonicalize_language_tag(token: &mut [u8]) {
    for c in token.iter_mut() {
        *c = c.to_ascii_lowercase();
    }

    let sub_tags = token.split_mut(|c| *c == ('-' as u8));
    for (i, sub_tag) in sub_tags.enumerate() {
        if i == 0 {
            // ISO 639-1 language code, like the "en" in "en-US"
            continue;
        }

        match sub_tag.len() {
            // Singleton tag, like "x" or "i". These signify a
            // non-standard language, so we stop capitalizing after
            // these.
            1 => break,
            // ISO 3166-1 Country code, like "US"
            2 => {
                sub_tag[0] = sub_tag[0].to_ascii_uppercase();
                sub_tag[1] = sub_tag[1].to_ascii_uppercase();
            }
            // ISO 15924 script code, like "Nkoo"
            4 => {
                sub_tag[0] = sub_tag[0].to_ascii_uppercase();
            }
            _ => {}
        };
    }
}

#[no_mangle]
pub extern "C" fn rust_net_is_valid_ipv4_addr<'a>(addr: &'a nsACString) -> bool {
    is_valid_ipv4_addr(addr)
}

#[inline]
fn try_apply_digit(current_octet: u8, digit_to_apply: u8) -> Option<u8> {
    current_octet.checked_mul(10)?.checked_add(digit_to_apply)
}

pub fn is_valid_ipv4_addr<'a>(addr: &'a [u8]) -> bool {
    let mut current_octet: Option<u8> = None;
    let mut dots: u8 = 0;
    for c in addr {
        let c = *c as char;
        match c {
            '.' => {
                match current_octet {
                    None => {
                        // starting an octet with a . is not allowed
                        return false;
                    }
                    Some(_) => {
                        dots = dots + 1;
                        current_octet = None;
                    }
                }
            }
            // The character is not a digit
            no_digit if no_digit.to_digit(10).is_none() => {
                return false;
            }
            digit => {
                match current_octet {
                    None => {
                        // Unwrap is sound because it has been checked in the previous arm
                        current_octet = Some(digit.to_digit(10).unwrap() as u8);
                    }
                    Some(octet) => {
                        if let Some(0) = current_octet {
                            // Leading 0 is not allowed
                            return false;
                        }
                        if let Some(applied) =
                            try_apply_digit(octet, digit.to_digit(10).unwrap() as u8)
                        {
                            current_octet = Some(applied);
                        } else {
                            // Multiplication or Addition overflowed
                            return false;
                        }
                    }
                }
            }
        }
    }
    dots == 3 && current_octet.is_some()
}

#[no_mangle]
pub extern "C" fn rust_net_is_valid_ipv6_addr<'a>(addr: &'a nsACString) -> bool {
    is_valid_ipv6_addr(addr)
}

#[inline(always)]
fn fast_is_hex_digit(c: u8) -> bool {
    match c {
        b'0'..=b'9' => true,
        b'a'..=b'f' => true,
        b'A'..=b'F' => true,
        _ => false,
    }
}

pub fn is_valid_ipv6_addr<'a>(addr: &'a [u8]) -> bool {
    let mut double_colon = false;
    let mut colon_before = false;
    let mut digits: u8 = 0;
    let mut blocks: u8 = 0;

    // The smallest ipv6 is unspecified (::)
    // The IP starts with a single colon
    if addr.len() < 2 || addr[0] == b':' && addr[1] != b':' {
        return false;
    }
    //Enumerate with an u8 for cache locality
    for (i, c) in (0u8..).zip(addr) {
        match c {
            maybe_digit if fast_is_hex_digit(*maybe_digit) => {
                // Too many digits in the block
                if digits == 4 {
                    return false;
                }
                colon_before = false;
                digits += 1;
            }
            b':' => {
                // Too many columns
                if double_colon && colon_before || blocks == 8 {
                    return false;
                }
                if !colon_before {
                    if digits != 0 {
                        blocks += 1;
                    }
                    digits = 0;
                    colon_before = true;
                } else if !double_colon {
                    double_colon = true;
                }
            }
            b'.' => {
                // IPv4 from the last block
                if is_valid_ipv4_addr(&addr[(i - digits) as usize..]) {
                    return double_colon && blocks < 6 || !double_colon && blocks == 6;
                }
                return false;
            }
            _ => {
                // Invalid character
                return false;
            }
        }
    }
    if colon_before && !double_colon {
        // The IP ends with a single colon
        return false;
    }
    if digits != 0 {
        blocks += 1;
    }

    double_colon && blocks < 8 || !double_colon && blocks == 8
}

#[no_mangle]
pub extern "C" fn rust_net_is_valid_scheme_char(a_char: u8) -> bool {
    is_valid_scheme_char(a_char)
}

#[no_mangle]
pub extern "C" fn rust_net_is_valid_scheme<'a>(scheme: &'a nsACString) -> bool {
    if scheme.is_empty() {
        return false;
    }

    // first char must be alpha
    if !scheme[0].is_ascii_alphabetic() {
        return false;
    }

    scheme[1..]
        .iter()
        .all(|a_char| is_valid_scheme_char(*a_char))
}

fn is_valid_scheme_char(a_char: u8) -> bool {
    a_char.is_ascii_alphanumeric() || a_char == b'+' || a_char == b'.' || a_char == b'-'
}

pub type ParsingCallback = extern "C" fn(&ThinVec<nsCString>) -> bool;

#[no_mangle]
pub extern "C" fn rust_parse_etc_hosts<'a>(path: &'a nsACString, callback: ParsingCallback) {
    let file = match File::open(&*path.to_utf8()) {
        Ok(file) => io::BufReader::new(file),
        Err(..) => return,
    };

    let mut array = ThinVec::new();
    for line in file.lines() {
        let line = match line {
            Ok(l) => l,
            Err(..) => continue,
        };

        let mut iter = line.split('#').next().unwrap().split_whitespace();
        iter.next(); // skip the IP

        array.extend(
            iter.filter(|host| {
                // Make sure it's a valid domain
                let invalid = [
                    '\0', '\t', '\n', '\r', ' ', '#', '%', '/', ':', '?', '@', '[', '\\', ']',
                ];
                host.parse::<Ipv4Addr>().is_err() && !host.contains(&invalid[..])
            })
            .map(nsCString::from),
        );

        // /etc/hosts files can be huge. To make sure we don't block shutdown
        // for every 100 domains that we parse we call the callback passing the
        // domains and see if we should keep parsing.
        if array.len() > 100 {
            let keep_going = callback(&array);
            array.clear();
            if !keep_going {
                break;
            }
        }
    }

    if !array.is_empty() {
        callback(&array);
    }
}
