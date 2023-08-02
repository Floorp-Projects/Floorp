// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Unicode Extensions provide information about user preferences in a given locale.
//!
//! The main struct for this extension is [`Unicode`] which contains [`Keywords`] and
//! [`Attributes`].
//!
//!
//! # Examples
//!
//! ```
//! use icu::locid::Locale;
//! use icu::locid::{
//!     extensions::unicode::Unicode,
//!     extensions_unicode_attribute as attribute,
//!     extensions_unicode_key as key, extensions_unicode_value as value,
//! };
//!
//! let loc: Locale = "en-US-u-foobar-hc-h12".parse().expect("Parsing failed.");
//!
//! assert_eq!(
//!     loc.extensions.unicode.keywords.get(&key!("hc")),
//!     Some(&value!("h12"))
//! );
//! assert!(loc
//!     .extensions
//!     .unicode
//!     .attributes
//!     .contains(&attribute!("foobar")));
//! ```
mod attribute;
mod attributes;
mod key;
mod keywords;
mod value;

use alloc::vec;
pub use attribute::Attribute;
pub use attributes::Attributes;
pub use key::Key;
pub use keywords::Keywords;
pub use value::Value;

use crate::parser::ParserError;
use crate::parser::SubtagIterator;
use litemap::LiteMap;

/// Unicode Extensions provide information about user preferences in a given locale.
///
/// A list of [`Unicode BCP47 U Extensions`] as defined in [`Unicode Locale
/// Identifier`] specification.
///
/// Unicode extensions provide subtags that specify language and/or locale-based behavior
/// or refinements to language tags, according to work done by the Unicode Consortium.
/// (See [`RFC 6067`] for details).
///
/// [`Unicode BCP47 U Extensions`]: https://unicode.org/reports/tr35/#u_Extension
/// [`RFC 6067`]: https://www.ietf.org/rfc/rfc6067.txt
/// [`Unicode Locale Identifier`]: https://unicode.org/reports/tr35/#Unicode_locale_identifier
///
/// # Examples
///
/// ```
/// use icu::locid::Locale;
/// use icu::locid::{
///     extensions_unicode_key as key, extensions_unicode_value as value,
/// };
///
/// let loc: Locale =
///     "de-u-hc-h12-ca-buddhist".parse().expect("Parsing failed.");
///
/// assert_eq!(
///     loc.extensions.unicode.keywords.get(&key!("ca")),
///     Some(&value!("buddhist"))
/// );
/// ```
#[derive(Clone, PartialEq, Eq, Debug, Default, Hash, PartialOrd, Ord)]
#[allow(clippy::exhaustive_structs)] // spec-backed stable datastructure
pub struct Unicode {
    /// The key-value pairs present in this locale extension, with each extension key subtag
    /// associated to its provided value subtag.
    pub keywords: Keywords,
    /// A canonically ordered sequence of single standalone subtags for this locale extension.
    pub attributes: Attributes,
}

impl Unicode {
    /// Returns a new empty map of Unicode extensions. Same as [`default()`](Default::default()), but is `const`.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::locid::extensions::unicode::Unicode;
    ///
    /// assert_eq!(Unicode::new(), Unicode::default());
    /// ```
    #[inline]
    pub const fn new() -> Self {
        Self {
            keywords: Keywords::new(),
            attributes: Attributes::new(),
        }
    }

    /// Returns [`true`] if there list of keywords and attributes is empty.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::locid::Locale;
    ///
    /// let loc: Locale = "en-US-u-foo".parse().expect("Parsing failed.");
    ///
    /// assert!(!loc.extensions.unicode.is_empty());
    /// ```
    pub fn is_empty(&self) -> bool {
        self.keywords.is_empty() && self.attributes.is_empty()
    }

    /// Clears all Unicode extension keywords and attributes, effectively removing
    /// the Unicode extension.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::locid::Locale;
    ///
    /// let mut loc: Locale =
    ///     "und-t-mul-u-hello-ca-buddhist-hc-h12".parse().unwrap();
    /// loc.extensions.unicode.clear();
    /// assert_eq!(loc, "und-t-mul".parse().unwrap());
    /// ```
    pub fn clear(&mut self) {
        self.keywords.clear();
        self.attributes.clear();
    }

    pub(crate) fn try_from_iter(iter: &mut SubtagIterator) -> Result<Self, ParserError> {
        let mut attributes = vec![];
        let mut keywords = LiteMap::new();

        let mut current_keyword = None;
        let mut current_type = vec![];

        while let Some(subtag) = iter.peek() {
            if let Ok(attr) = Attribute::try_from_bytes(subtag) {
                if let Err(idx) = attributes.binary_search(&attr) {
                    attributes.insert(idx, attr);
                }
            } else {
                break;
            }
            iter.next();
        }

        while let Some(subtag) = iter.peek() {
            let slen = subtag.len();
            if slen == 2 {
                if let Some(kw) = current_keyword.take() {
                    keywords.try_insert(kw, Value::from_vec_unchecked(current_type));
                    current_type = vec![];
                }
                current_keyword = Some(Key::try_from_bytes(subtag)?);
            } else if current_keyword.is_some() {
                match Value::parse_subtag(subtag) {
                    Ok(Some(t)) => current_type.push(t),
                    Ok(None) => {}
                    Err(_) => break,
                }
            } else {
                break;
            }
            iter.next();
        }

        if let Some(kw) = current_keyword.take() {
            keywords.try_insert(kw, Value::from_vec_unchecked(current_type));
        }

        // Ensure we've defined at least one attribute or keyword
        if attributes.is_empty() && keywords.is_empty() {
            return Err(ParserError::InvalidExtension);
        }

        Ok(Self {
            keywords: keywords.into(),
            attributes: Attributes::from_vec_unchecked(attributes),
        })
    }

    pub(crate) fn for_each_subtag_str<E, F>(&self, f: &mut F) -> Result<(), E>
    where
        F: FnMut(&str) -> Result<(), E>,
    {
        if self.is_empty() {
            return Ok(());
        }
        f("u")?;
        self.attributes.for_each_subtag_str(f)?;
        self.keywords.for_each_subtag_str(f)?;
        Ok(())
    }
}

writeable::impl_display_with_writeable!(Unicode);

impl writeable::Writeable for Unicode {
    fn write_to<W: core::fmt::Write + ?Sized>(&self, sink: &mut W) -> core::fmt::Result {
        if self.is_empty() {
            return Ok(());
        }
        sink.write_str("u")?;
        if !self.attributes.is_empty() {
            sink.write_char('-')?;
            writeable::Writeable::write_to(&self.attributes, sink)?;
        }
        if !self.keywords.is_empty() {
            sink.write_char('-')?;
            writeable::Writeable::write_to(&self.keywords, sink)?;
        }
        Ok(())
    }

    fn writeable_length_hint(&self) -> writeable::LengthHint {
        if self.is_empty() {
            return writeable::LengthHint::exact(0);
        }
        let mut result = writeable::LengthHint::exact(1);
        if !self.attributes.is_empty() {
            result += writeable::Writeable::writeable_length_hint(&self.attributes) + 1;
        }
        if !self.keywords.is_empty() {
            result += writeable::Writeable::writeable_length_hint(&self.keywords) + 1;
        }
        result
    }
}
