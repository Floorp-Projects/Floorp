pub mod errors;

use std::iter::Peekable;

pub use self::errors::ParserError;
use crate::subtags;
use crate::LanguageIdentifier;

pub fn parse_language_identifier_from_iter<'a>(
    iter: &mut Peekable<impl Iterator<Item = &'a [u8]>>,
    allow_extension: bool,
) -> Result<LanguageIdentifier, ParserError> {
    let mut position = 0;

    let mut language = None;
    let mut script = None;
    let mut region = None;
    let mut variants = vec![];

    let mut st_peek = iter.peek();

    while let Some(subtag) = st_peek {
        if position == 0 {
            // Language
            language = subtags::parse_language_subtag(subtag)?;
            position = 1;
        } else if position == 1 {
            if let Ok(s) = subtags::parse_script_subtag(subtag) {
                script = Some(s);
                position = 2;
            } else if let Ok(s) = subtags::parse_region_subtag(subtag) {
                region = Some(s);
                position = 3;
            } else if let Ok(v) = subtags::parse_variant_subtag(subtag) {
                variants.push(v);
                position = 3;
            } else {
                break;
            }
        } else if position == 2 {
            if let Ok(s) = subtags::parse_region_subtag(subtag) {
                region = Some(s);
                position = 3;
            } else if let Ok(v) = subtags::parse_variant_subtag(subtag) {
                variants.push(v);
                position = 3;
            } else {
                break;
            }
        } else {
            // Variants
            if let Ok(v) = subtags::parse_variant_subtag(subtag) {
                variants.push(v);
            } else {
                break;
            }
        }
        iter.next();
        st_peek = iter.peek();
    }

    if !allow_extension && iter.peek().is_some() {
        return Err(ParserError::InvalidSubtag);
    }

    let variants = if variants.is_empty() {
        None
    } else {
        variants.sort_unstable();
        variants.dedup();
        Some(variants.into_boxed_slice())
    };

    Ok(LanguageIdentifier {
        language,
        script,
        region,
        variants,
    })
}

pub fn parse_language_identifier(t: &[u8]) -> Result<LanguageIdentifier, ParserError> {
    let mut iter = t.split(|c| *c == b'-' || *c == b'_').peekable();
    parse_language_identifier_from_iter(&mut iter, false)
}
