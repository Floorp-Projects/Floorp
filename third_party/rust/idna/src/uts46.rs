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
use std::cmp::Ordering::{Equal, Greater, Less};
use unicode_bidi::{bidi_class, BidiClass};
use unicode_normalization::char::is_combining_mark;
use unicode_normalization::UnicodeNormalization;

include!("uts46_mapping_table.rs");

const PUNYCODE_PREFIX: &'static str = "xn--";

#[derive(Debug)]
struct StringTableSlice {
    // Store these as separate fields so the structure will have an
    // alignment of 1 and thus pack better into the Mapping enum, below.
    byte_start_lo: u8,
    byte_start_hi: u8,
    byte_len: u8,
}

fn decode_slice(slice: &StringTableSlice) -> &'static str {
    let lo = slice.byte_start_lo as usize;
    let hi = slice.byte_start_hi as usize;
    let start = (hi << 8) | lo;
    let len = slice.byte_len as usize;
    &STRING_TABLE[start..(start + len)]
}

#[repr(u8)]
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
}

fn find_char(codepoint: char) -> &'static Mapping {
    let r = TABLE.binary_search_by(|ref range| {
        if codepoint > range.to {
            Less
        } else if codepoint < range.from {
            Greater
        } else {
            Equal
        }
    });
    r.ok()
        .map(|i| {
            const SINGLE_MARKER: u16 = 1 << 15;

            let x = INDEX_TABLE[i];
            let single = (x & SINGLE_MARKER) != 0;
            let offset = !SINGLE_MARKER & x;

            if single {
                &MAPPING_TABLE[offset as usize]
            } else {
                &MAPPING_TABLE[(offset + (codepoint as u16 - TABLE[i].from as u16)) as usize]
            }
        })
        .unwrap()
}

fn map_char(codepoint: char, config: Config, output: &mut String, errors: &mut Vec<Error>) {
    match *find_char(codepoint) {
        Mapping::Valid => output.push(codepoint),
        Mapping::Ignored => {}
        Mapping::Mapped(ref slice) => output.push_str(decode_slice(slice)),
        Mapping::Deviation(ref slice) => {
            if config.transitional_processing {
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
            if config.use_std3_ascii_rules {
                errors.push(Error::DissallowedByStd3AsciiRules);
            }
            output.push(codepoint)
        }
        Mapping::DisallowedStd3Mapped(ref slice) => {
            if config.use_std3_ascii_rules {
                errors.push(Error::DissallowedMappedInStd3);
            }
            output.push_str(decode_slice(slice))
        }
    }
}

// http://tools.ietf.org/html/rfc5893#section-2
fn passes_bidi(label: &str, is_bidi_domain: bool) -> bool {
    // Rule 0: Bidi Rules apply to Bidi Domain Names: a name with at least one RTL label.  A label
    // is RTL if it contains at least one character of bidi class R, AL or AN.
    if !is_bidi_domain {
        return true;
    }

    let mut chars = label.chars();
    let first_char_class = match chars.next() {
        Some(c) => bidi_class(c),
        None => return true, // empty string
    };

    match first_char_class {
        // LTR label
        BidiClass::L => {
            // Rule 5
            loop {
                match chars.next() {
                    Some(c) => {
                        if !matches!(
                            bidi_class(c),
                            BidiClass::L
                                | BidiClass::EN
                                | BidiClass::ES
                                | BidiClass::CS
                                | BidiClass::ET
                                | BidiClass::ON
                                | BidiClass::BN
                                | BidiClass::NSM
                        ) {
                            return false;
                        }
                    }
                    None => {
                        break;
                    }
                }
            }

            // Rule 6
            // must end in L or EN followed by 0 or more NSM
            let mut rev_chars = label.chars().rev();
            let mut last_non_nsm = rev_chars.next();
            loop {
                match last_non_nsm {
                    Some(c) if bidi_class(c) == BidiClass::NSM => {
                        last_non_nsm = rev_chars.next();
                        continue;
                    }
                    _ => {
                        break;
                    }
                }
            }
            match last_non_nsm {
                Some(c) if bidi_class(c) == BidiClass::L || bidi_class(c) == BidiClass::EN => {}
                Some(_) => {
                    return false;
                }
                _ => {}
            }
        }

        // RTL label
        BidiClass::R | BidiClass::AL => {
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

                        if !matches!(
                            char_class,
                            BidiClass::R
                                | BidiClass::AL
                                | BidiClass::AN
                                | BidiClass::EN
                                | BidiClass::ES
                                | BidiClass::CS
                                | BidiClass::ET
                                | BidiClass::ON
                                | BidiClass::BN
                                | BidiClass::NSM
                        ) {
                            return false;
                        }
                    }
                    None => {
                        break;
                    }
                }
            }
            // Rule 3
            let mut rev_chars = label.chars().rev();
            let mut last = rev_chars.next();
            loop {
                // must end in L or EN followed by 0 or more NSM
                match last {
                    Some(c) if bidi_class(c) == BidiClass::NSM => {
                        last = rev_chars.next();
                        continue;
                    }
                    _ => {
                        break;
                    }
                }
            }
            match last {
                Some(c)
                    if matches!(
                        bidi_class(c),
                        BidiClass::R | BidiClass::AL | BidiClass::EN | BidiClass::AN
                    ) => {}
                _ => {
                    return false;
                }
            }

            // Rule 4
            if found_an && found_en {
                return false;
            }
        }

        // Rule 1: Should start with L or R/AL
        _ => {
            return false;
        }
    }

    return true;
}

/// http://www.unicode.org/reports/tr46/#Validity_Criteria
fn validate_full(label: &str, is_bidi_domain: bool, config: Config, errors: &mut Vec<Error>) {
    // V1: Must be in NFC form.
    if label.nfc().ne(label.chars()) {
        errors.push(Error::ValidityCriteria);
    } else {
        validate(label, is_bidi_domain, config, errors);
    }
}

fn validate(label: &str, is_bidi_domain: bool, config: Config, errors: &mut Vec<Error>) {
    let first_char = label.chars().next();
    if first_char == None {
        // Empty string, pass
    }
    // V2: No U+002D HYPHEN-MINUS in both third and fourth positions.
    //
    // NOTE: Spec says that the label must not contain a HYPHEN-MINUS character in both the
    // third and fourth positions. But nobody follows this criteria. See the spec issue below:
    // https://github.com/whatwg/url/issues/53

    // V3: neither begin nor end with a U+002D HYPHEN-MINUS
    else if config.check_hyphens && (label.starts_with("-") || label.ends_with("-")) {
        errors.push(Error::ValidityCriteria);
    }
    // V4: not contain a U+002E FULL STOP
    //
    // Here, label can't contain '.' since the input is from .split('.')

    // V5: not begin with a GC=Mark
    else if is_combining_mark(first_char.unwrap()) {
        errors.push(Error::ValidityCriteria);
    }
    // V6: Check against Mapping Table
    else if label.chars().any(|c| match *find_char(c) {
        Mapping::Valid => false,
        Mapping::Deviation(_) => config.transitional_processing,
        Mapping::DisallowedStd3Valid => config.use_std3_ascii_rules,
        _ => true,
    }) {
        errors.push(Error::ValidityCriteria);
    }
    // V7: ContextJ rules
    //
    // TODO: Implement rules and add *CheckJoiners* flag.

    // V8: Bidi rules
    //
    // TODO: Add *CheckBidi* flag
    else if !passes_bidi(label, is_bidi_domain) {
        errors.push(Error::ValidityCriteria);
    }
}

/// http://www.unicode.org/reports/tr46/#Processing
fn processing(domain: &str, config: Config, errors: &mut Vec<Error>) -> String {
    let mut mapped = String::with_capacity(domain.len());
    for c in domain.chars() {
        map_char(c, config, &mut mapped, errors)
    }
    let mut normalized = String::with_capacity(mapped.len());
    normalized.extend(mapped.nfc());

    // Find out if it's a Bidi Domain Name
    //
    // First, check for literal bidi chars
    let mut is_bidi_domain = domain
        .chars()
        .any(|c| matches!(bidi_class(c), BidiClass::R | BidiClass::AL | BidiClass::AN));
    if !is_bidi_domain {
        // Then check for punycode-encoded bidi chars
        for label in normalized.split('.') {
            if label.starts_with(PUNYCODE_PREFIX) {
                match punycode::decode_to_string(&label[PUNYCODE_PREFIX.len()..]) {
                    Some(decoded_label) => {
                        if decoded_label.chars().any(|c| {
                            matches!(bidi_class(c), BidiClass::R | BidiClass::AL | BidiClass::AN)
                        }) {
                            is_bidi_domain = true;
                        }
                    }
                    None => {
                        is_bidi_domain = true;
                    }
                }
            }
        }
    }

    let mut validated = String::new();
    let mut first = true;
    for label in normalized.split('.') {
        if !first {
            validated.push('.');
        }
        first = false;
        if label.starts_with(PUNYCODE_PREFIX) {
            match punycode::decode_to_string(&label[PUNYCODE_PREFIX.len()..]) {
                Some(decoded_label) => {
                    let config = config.transitional_processing(false);
                    validate_full(&decoded_label, is_bidi_domain, config, errors);
                    validated.push_str(&decoded_label)
                }
                None => errors.push(Error::PunycodeError),
            }
        } else {
            // `normalized` is already `NFC` so we can skip that check
            validate(label, is_bidi_domain, config, errors);
            validated.push_str(label)
        }
    }
    validated
}

#[derive(Clone, Copy)]
pub struct Config {
    use_std3_ascii_rules: bool,
    transitional_processing: bool,
    verify_dns_length: bool,
    check_hyphens: bool,
}

/// The defaults are that of https://url.spec.whatwg.org/#idna
impl Default for Config {
    fn default() -> Self {
        Config {
            use_std3_ascii_rules: false,
            transitional_processing: false,
            check_hyphens: false,
            // check_bidi: true,
            // check_joiners: true,

            // Only use for to_ascii, not to_unicode
            verify_dns_length: false,
        }
    }
}

impl Config {
    #[inline]
    pub fn use_std3_ascii_rules(mut self, value: bool) -> Self {
        self.use_std3_ascii_rules = value;
        self
    }

    #[inline]
    pub fn transitional_processing(mut self, value: bool) -> Self {
        self.transitional_processing = value;
        self
    }

    #[inline]
    pub fn verify_dns_length(mut self, value: bool) -> Self {
        self.verify_dns_length = value;
        self
    }

    #[inline]
    pub fn check_hyphens(mut self, value: bool) -> Self {
        self.check_hyphens = value;
        self
    }

    /// http://www.unicode.org/reports/tr46/#ToASCII
    pub fn to_ascii(self, domain: &str) -> Result<String, Errors> {
        let mut errors = Vec::new();
        let mut result = String::new();
        let mut first = true;
        for label in processing(domain, self, &mut errors).split('.') {
            if !first {
                result.push('.');
            }
            first = false;
            if label.is_ascii() {
                result.push_str(label);
            } else {
                match punycode::encode_str(label) {
                    Some(x) => {
                        result.push_str(PUNYCODE_PREFIX);
                        result.push_str(&x);
                    }
                    None => errors.push(Error::PunycodeError),
                }
            }
        }

        if self.verify_dns_length {
            let domain = if result.ends_with(".") {
                &result[..result.len() - 1]
            } else {
                &*result
            };
            if domain.len() < 1 || domain.split('.').any(|label| label.len() < 1) {
                errors.push(Error::TooShortForDns)
            }
            if domain.len() > 253 || domain.split('.').any(|label| label.len() > 63) {
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
    pub fn to_unicode(self, domain: &str) -> (String, Result<(), Errors>) {
        let mut errors = Vec::new();
        let domain = processing(domain, self, &mut errors);
        let errors = if errors.is_empty() {
            Ok(())
        } else {
            Err(Errors(errors))
        };
        (domain, errors)
    }
}

#[derive(PartialEq, Eq, Clone, Copy, Debug)]
enum Error {
    PunycodeError,
    ValidityCriteria,
    DissallowedByStd3AsciiRules,
    DissallowedMappedInStd3,
    DissallowedCharacter,
    TooLongForDns,
    TooShortForDns,
}

/// Errors recorded during UTS #46 processing.
///
/// This is opaque for now, only indicating the presence of at least one error.
/// More details may be exposed in the future.
#[derive(Debug)]
pub struct Errors(Vec<Error>);
