use std::ascii::AsciiExt;

extern crate nserror;
use self::nserror::*;

extern crate nsstring;
use self::nsstring::nsACString;

/// HTTP leading whitespace, defined in netwerk/protocol/http/nsHttp.h
static HTTP_LWS: &'static [u8] = &[' ' as u8, '\t' as u8];

/// Trim leading whitespace, trailing whitespace, and quality-value
/// from a token.
fn trim_token(token: &[u8]) -> &[u8] {
    // Trim left whitespace
    let ltrim = token.iter()
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
#[allow(non_snake_case)]
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
pub extern "C" fn rust_prepare_accept_languages<'a, 'b>(i_accept_languages: &'a nsACString,
                                                        o_accept_languages: &'b mut nsACString)
                                                        -> nsresult {
    if i_accept_languages.is_empty() {
        return NS_OK;
    }

    let make_tokens = || {
        i_accept_languages.split(|c| *c == (',' as u8))
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
        *c = AsciiExt::to_ascii_lowercase(c);
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
                sub_tag[0] = AsciiExt::to_ascii_uppercase(&sub_tag[0]);
                sub_tag[1] = AsciiExt::to_ascii_uppercase(&sub_tag[1]);
            },
            // ISO 15924 script code, like "Nkoo"
            4  => {
                sub_tag[0] = AsciiExt::to_ascii_uppercase(&sub_tag[0]);
            },
            _ => {},
        };
    }
}
