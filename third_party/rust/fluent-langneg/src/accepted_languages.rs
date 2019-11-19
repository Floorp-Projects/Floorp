//! This function parses Accept-Language string into a list of language tags that
//! can be later passed to language negotiation functions.
//!
//! # Example:
//!
//! ```
//! use fluent_langneg::negotiate_languages;
//! use fluent_langneg::NegotiationStrategy;
//! use fluent_langneg::parse_accepted_languages;
//! use fluent_langneg::convert_vec_str_to_langids_lossy;
//! use unic_langid::LanguageIdentifier;
//!
//! let requested = parse_accepted_languages("de-AT;0.9,de-DE;0.8,de;0.7;en-US;0.5");
//! let available = convert_vec_str_to_langids_lossy(&["fr", "pl", "de", "en-US"]);
//! let default: LanguageIdentifier = "en-US".parse().expect("Failed to parse a langid.");
//!
//! let supported = negotiate_languages(
//!   &requested,
//!   &available,
//!   Some(&default),
//!   NegotiationStrategy::Filtering
//! );
//!
//! let expected = convert_vec_str_to_langids_lossy(&["de", "en-US"]);
//! assert_eq!(supported,
//!            expected.iter().map(|t| t.as_ref()).collect::<Vec<&LanguageIdentifier>>());
//! ```
//!
//! This function ignores the weights associated with the locales, since Fluent Locale
//! language negotiation only uses the order of locales, not the weights.
//!

use unic_langid::LanguageIdentifier;

pub fn parse(s: &str) -> Vec<LanguageIdentifier> {
    s.split(',')
        .map(|t| t.trim().split(';').nth(0).unwrap())
        .filter(|t| !t.is_empty())
        .filter_map(|t| t.parse().ok())
        .collect()
}
