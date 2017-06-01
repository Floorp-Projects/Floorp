// Copyright 2013-2014 The rust-url developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! [*Unicode IDNA Compatibility Processing*
//! (Unicode Technical Standard #46)](http://www.unicode.org/reports/tr46/)

use self::Mapping::*;
use punycode;
use std::ascii::AsciiExt;
use unicode_normalization::UnicodeNormalization;
use unicode_normalization::char::is_combining_mark;
use unicode_bidi::{BidiClass, bidi_class};

include!("uts46_mapping_table.rs");

#[derive(Debug)]
struct StringTableSlice {
    byte_start: u16,
    byte_len: u16,
}

fn decode_slice(slice: &StringTableSlice) -> &'static str {
    let start = slice.byte_start as usize;
    let len = slice.byte_len as usize;
    &STRING_TABLE[start..(start + len)]
}

#[repr(u16)]
#[derive(Debug)]
enum Mapping {
    Valid,
    Ignored,
    Mapped(StringTableSlice),
    Deviation(StringTableSlice),
    Disallowed,
    DisallowedStd3Valid,
    DisallowedStd3Mapped(StringTableSlice),
}

struct Range {
    from: char,
    to: char,
    mapping: Mapping,
}

fn find_char(codepoint: char) -> &'static Mapping {
    let mut min = 0;
    let mut max = TABLE.len() - 1;
    while max > min {
        let mid = (min + max) >> 1;
        if codepoint > TABLE[mid].to {
           min = mid;
        } else if codepoint < TABLE[mid].from {
            max = mid;
        } else {
            min = mid;
            max = mid;
        }
    }
    &TABLE[min].mapping
}

fn map_char(codepoint: char, flags: Flags, output: &mut String, errors: &mut Vec<Error>) {
    match *find_char(codepoint) {
        Mapping::Valid => output.push(codepoint),
        Mapping::Ignored => {},
        Mapping::Mapped(ref slice) => output.push_str(decode_slice(slice)),
        Mapping::Deviation(ref slice) => {
            if flags.transitional_processing {
                output.push_str(decode_slice(slice))
            } else {
                output.push(codepoint)
            }
        }
        Mapping::Disallowed => {
            errors.push(Error::DissallowedCharacter);
            output.push(codepoint);
        }
        Mapping::DisallowedStd3Valid => {
            if flags.use_std3_ascii_rules {
                errors.push(Error::DissallowedByStd3AsciiRules);
            }
            output.push(codepoint)
        }
        Mapping::DisallowedStd3Mapped(ref slice) => {
            if flags.use_std3_ascii_rules {
                errors.push(Error::DissallowedMappedInStd3);
            }
            output.push_str(decode_slice(slice))
        }
    }
}

// http://tools.ietf.org/html/rfc5893#section-2
fn passes_bidi(label: &str, transitional_processing: bool) -> bool {
    let mut chars = label.chars();
    let class = match chars.next() {
        Some(c) => bidi_class(c),
        None => return true, // empty string
    };

    if class == BidiClass::L
       || (class == BidiClass::ON && transitional_processing) // starts with \u200D
       || (class == BidiClass::ES && transitional_processing) // hack: 1.35.+33.49
       || class == BidiClass::EN // hack: starts with number 0à.\u05D0
    { // LTR
        // Rule 5
        loop {
            match chars.next() {
                Some(c) => {
                    let c = bidi_class(c);
                    if !matches!(c, BidiClass::L | BidiClass::EN |
                                    BidiClass::ES | BidiClass::CS |
                                    BidiClass::ET | BidiClass::ON |
                                    BidiClass::BN | BidiClass::NSM) {
                        return false;
                    }
                },
                None => { break; },
            }
        }

        // Rule 6
        let mut rev_chars = label.chars().rev();
        let mut last = rev_chars.next();
        loop { // must end in L or EN followed by 0 or more NSM
            match last {
                Some(c) if bidi_class(c) == BidiClass::NSM => {
                    last = rev_chars.next();
                    continue;
                }
                _ => { break; },
            }
        }

        // TODO: does not pass for àˇ.\u05D0
        // match last {
        //     Some(c) if bidi_class(c) == BidiClass::L
        //             || bidi_class(c) == BidiClass::EN => {},
        //     Some(c) => { return false; },
        //     _ => {}
        // }

    } else if class == BidiClass::R || class == BidiClass::AL { // RTL
        let mut found_en = false;
        let mut found_an = false;

        // Rule 2
        loop {
            match chars.next() {
                Some(c) => {
                    let char_class = bidi_class(c);

                    if char_class == BidiClass::EN {
                        found_en = true;
                    }
                    if char_class == BidiClass::AN {
                        found_an = true;
                    }

                    if !matches!(char_class, BidiClass::R | BidiClass::AL |
                                             BidiClass::AN | BidiClass::EN |
                                             BidiClass::ES | BidiClass::CS |
                                             BidiClass::ET | BidiClass::ON |
                                             BidiClass::BN | BidiClass::NSM) {
                        return false;
                    }
                },
                None => { break; },
            }
        }
        // Rule 3
        let mut rev_chars = label.chars().rev();
        let mut last = rev_chars.next();
        loop { // must end in L or EN followed by 0 or more NSM
            match last {
                Some(c) if bidi_class(c) == BidiClass::NSM => {
                    last = rev_chars.next();
                    continue;
                }
                _ => { break; },
            }
        }
        match last {
            Some(c) if matches!(bidi_class(c), BidiClass::R | BidiClass::AL |
                                               BidiClass::EN | BidiClass::AN) => {},
            _ => { return false; }
        }

        // Rule 4
        if found_an && found_en {
            return false;
        }
    } else {
        // Rule 2: Should start with L or R/AL
        return false;
    }

    return true;
}

/// http://www.unicode.org/reports/tr46/#Validity_Criteria
fn validate(label: &str, flags: Flags, errors: &mut Vec<Error>) {
    if label.nfc().ne(label.chars()) {
        errors.push(Error::ValidityCriteria);
    }

    // Can not contain '.' since the input is from .split('.')
    // Spec says that the label must not contain a HYPHEN-MINUS character in both the
    // third and fourth positions. But nobody follows this criteria. See the spec issue below:
    // https://github.com/whatwg/url/issues/53
    if label.starts_with("-")
        || label.ends_with("-")
        || label.chars().next().map_or(false, is_combining_mark)
        || label.chars().any(|c| match *find_char(c) {
            Mapping::Valid => false,
            Mapping::Deviation(_) => flags.transitional_processing,
            Mapping::DisallowedStd3Valid => flags.use_std3_ascii_rules,
            _ => true,
        })
        || !passes_bidi(label, flags.transitional_processing)
    {
        errors.push(Error::ValidityCriteria)
    }
}

/// http://www.unicode.org/reports/tr46/#Processing
fn processing(domain: &str, flags: Flags, errors: &mut Vec<Error>) -> String {
    let mut mapped = String::new();
    for c in domain.chars() {
        map_char(c, flags, &mut mapped, errors)
    }
    let normalized: String = mapped.nfc().collect();
    let mut validated = String::new();
    for label in normalized.split('.') {
        if validated.len() > 0 {
            validated.push('.');
        }
        if label.starts_with("xn--") {
            match punycode::decode_to_string(&label["xn--".len()..]) {
                Some(decoded_label) => {
                    let flags = Flags { transitional_processing: false, ..flags };
                    validate(&decoded_label, flags, errors);
                    validated.push_str(&decoded_label)
                }
                None => errors.push(Error::PunycodeError)
            }
        } else {
            validate(label, flags, errors);
            validated.push_str(label)
        }
    }
    validated
}

#[derive(Copy, Clone)]
pub struct Flags {
   pub use_std3_ascii_rules: bool,
   pub transitional_processing: bool,
   pub verify_dns_length: bool,
}

#[derive(PartialEq, Eq, Clone, Copy, Debug)]
enum Error {
    PunycodeError,
    ValidityCriteria,
    DissallowedByStd3AsciiRules,
    DissallowedMappedInStd3,
    DissallowedCharacter,
    TooLongForDns,
}

/// Errors recorded during UTS #46 processing.
///
/// This is opaque for now, only indicating the presence of at least one error.
/// More details may be exposed in the future.
#[derive(Debug)]
pub struct Errors(Vec<Error>);

/// http://www.unicode.org/reports/tr46/#ToASCII
pub fn to_ascii(domain: &str, flags: Flags) -> Result<String, Errors> {
    let mut errors = Vec::new();
    let mut result = String::new();
    for label in processing(domain, flags, &mut errors).split('.') {
        if result.len() > 0 {
            result.push('.');
        }
        if label.is_ascii() {
            result.push_str(label);
        } else {
            match punycode::encode_str(label) {
                Some(x) => {
                    result.push_str("xn--");
                    result.push_str(&x);
                },
                None => errors.push(Error::PunycodeError)
            }
        }
    }

    if flags.verify_dns_length {
        let domain = if result.ends_with(".") { &result[..result.len()-1]  } else { &*result };
        if domain.len() < 1 || domain.len() > 253 ||
                domain.split('.').any(|label| label.len() < 1 || label.len() > 63) {
            errors.push(Error::TooLongForDns)
        }
    }
    if errors.is_empty() {
        Ok(result)
    } else {
        Err(Errors(errors))
    }
}

/// http://www.unicode.org/reports/tr46/#ToUnicode
///
/// Only `use_std3_ascii_rules` is used in `flags`.
pub fn to_unicode(domain: &str, mut flags: Flags) -> (String, Result<(), Errors>) {
    flags.transitional_processing = false;
    let mut errors = Vec::new();
    let domain = processing(domain, flags, &mut errors);
    let errors = if errors.is_empty() {
        Ok(())
    } else {
        Err(Errors(errors))
    };
    (domain, errors)
}
