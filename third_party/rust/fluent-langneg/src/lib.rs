//! fluent-langneg is an API for operating on locales and language tags.
//! It's part of Project Fluent, a localization framework designed to unleash
//! the expressive power of the natural language.
//!
//! The primary use of fluent-langneg is to parse/modify/serialize language tags
//! and to perform language negotiation.
//!
//! fluent-langneg operates on a subset of [BCP47](http://tools.ietf.org/html/bcp47).
//! It can parse full BCP47 language tags, and will serialize them back,
//! but currently only allows for operations on primary subtags and
//! unicode extension keys.
//!
//! In result fluent-langneg is not suited to replace full implementations of
//! BCP47 like [rust-language-tags](https://github.com/pyfisch/rust-language-tags),
//! but is arguably a better option for use cases involving operations on
//! language tags and for language negotiation.

pub mod accepted_languages;
pub mod negotiate;

pub use accepted_languages::parse as parse_accepted_languages;
pub use negotiate::negotiate_languages;
pub use negotiate::NegotiationStrategy;

use unic_langid::{LanguageIdentifier, LanguageIdentifierError};

pub fn convert_vec_str_to_langids<'a, I, J>(
    input: I,
) -> Result<Vec<LanguageIdentifier>, LanguageIdentifierError>
where
    I: IntoIterator<Item = J>,
    J: AsRef<[u8]> + 'a,
{
    input
        .into_iter()
        .map(|s| LanguageIdentifier::from_bytes(s.as_ref()))
        .collect()
}

pub fn convert_vec_str_to_langids_lossy<'a, I, J>(input: I) -> Vec<LanguageIdentifier>
where
    I: IntoIterator<Item = J>,
    J: AsRef<[u8]> + 'a,
{
    input
        .into_iter()
        .filter_map(|t| LanguageIdentifier::from_bytes(t.as_ref()).ok())
        .collect()
}
