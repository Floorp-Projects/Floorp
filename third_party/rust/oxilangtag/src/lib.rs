#![doc = include_str!("../README.md")]
#![deny(unsafe_code)]

#[cfg(feature = "serde")]
use serde::{Deserialize, Deserializer, Serialize, Serializer};
use std::borrow::{Borrow, Cow};
use std::cmp::Ordering;
use std::error::Error;
use std::fmt;
use std::hash::{Hash, Hasher};
use std::iter::once;
use std::ops::Deref;
use std::str::{FromStr, Split};

/// A [RFC 5646](https://tools.ietf.org/html/rfc5646) language tag.
///
/// ```
/// use oxilangtag::LanguageTag;
///
/// let language_tag = LanguageTag::parse("en-us").unwrap();
/// assert_eq!(language_tag.into_inner(), "en-us")
/// ```
#[derive(Copy, Clone)]
pub struct LanguageTag<T> {
    tag: T,
    positions: TagElementsPositions,
}

impl<T: Deref<Target = str>> LanguageTag<T> {
    /// Parses a language tag acccording to [RFC 5646](https://tools.ietf.org/html/rfc5646).
    /// and checks if the tag is ["well-formed"](https://tools.ietf.org/html/rfc5646#section-2.2.9).
    ///
    /// This operation keeps internally the `tag` parameter and does not allocate on the heap.
    ///
    /// ```
    /// use oxilangtag::LanguageTag;
    ///
    /// let language_tag = LanguageTag::parse("en-us").unwrap();
    /// assert_eq!(language_tag.into_inner(), "en-us")
    /// ```
    pub fn parse(tag: T) -> Result<Self, LanguageTagParseError> {
        let positions = parse_language_tag(&tag, &mut VoidOutputBuffer::default())?;
        Ok(Self { tag, positions })
    }

    /// Returns the underlying language tag representation.
    #[inline]
    pub fn as_str(&self) -> &str {
        &self.tag
    }

    /// Returns the underlying language tag representation.
    #[inline]
    pub fn into_inner(self) -> T {
        self.tag
    }

    /// Returns the [primary language subtag](https://tools.ietf.org/html/rfc5646#section-2.2.1).
    ///
    /// ```
    /// use oxilangtag::LanguageTag;
    ///
    /// let language_tag = LanguageTag::parse("zh-cmn-Hans-CN").unwrap();
    /// assert_eq!(language_tag.primary_language(), "zh");
    /// ```
    #[inline]
    pub fn primary_language(&self) -> &str {
        &self.tag[..self.positions.language_end]
    }

    /// Returns the [extended language subtags](https://tools.ietf.org/html/rfc5646#section-2.2.2).
    ///
    /// Valid language tags have at most one extended language.
    ///
    /// ```
    /// use oxilangtag::LanguageTag;
    ///
    /// let language_tag = LanguageTag::parse("zh-cmn-Hans-CN").unwrap();
    /// assert_eq!(language_tag.extended_language(), Some("cmn"));
    /// ```
    #[inline]
    pub fn extended_language(&self) -> Option<&str> {
        if self.positions.language_end == self.positions.extlang_end {
            None
        } else {
            Some(&self.tag[self.positions.language_end + 1..self.positions.extlang_end])
        }
    }

    /// Iterates on the [extended language subtags](https://tools.ietf.org/html/rfc5646#section-2.2.2).
    ///
    /// Valid language tags have at most one extended language.
    ///
    /// ```
    /// use oxilangtag::LanguageTag;
    ///
    /// let language_tag = LanguageTag::parse("zh-cmn-Hans-CN").unwrap();
    /// assert_eq!(language_tag.extended_language_subtags().collect::<Vec<_>>(), vec!["cmn"]);
    /// ```
    #[inline]
    pub fn extended_language_subtags(&self) -> impl Iterator<Item = &str> {
        self.extended_language().unwrap_or("").split_terminator('-')
    }

    /// Returns the [primary language subtag](https://tools.ietf.org/html/rfc5646#section-2.2.1)
    /// and its [extended language subtags](https://tools.ietf.org/html/rfc5646#section-2.2.2).
    ///
    /// ```
    /// use oxilangtag::LanguageTag;
    ///
    /// let language_tag = LanguageTag::parse("zh-cmn-Hans-CN").unwrap();
    /// assert_eq!(language_tag.full_language(), "zh-cmn");
    /// ```
    #[inline]
    pub fn full_language(&self) -> &str {
        &self.tag[..self.positions.extlang_end]
    }

    /// Returns the [script subtag](https://tools.ietf.org/html/rfc5646#section-2.2.3).
    ///
    /// ```
    /// use oxilangtag::LanguageTag;
    ///
    /// let language_tag = LanguageTag::parse("zh-cmn-Hans-CN").unwrap();
    /// assert_eq!(language_tag.script(), Some("Hans"));
    /// ```
    #[inline]
    pub fn script(&self) -> Option<&str> {
        if self.positions.extlang_end == self.positions.script_end {
            None
        } else {
            Some(&self.tag[self.positions.extlang_end + 1..self.positions.script_end])
        }
    }

    /// Returns the [region subtag](https://tools.ietf.org/html/rfc5646#section-2.2.4).
    ///
    /// ```
    /// use oxilangtag::LanguageTag;
    ///
    /// let language_tag = LanguageTag::parse("zh-cmn-Hans-CN").unwrap();
    /// assert_eq!(language_tag.region(), Some("CN"));
    /// ```
    #[inline]
    pub fn region(&self) -> Option<&str> {
        if self.positions.script_end == self.positions.region_end {
            None
        } else {
            Some(&self.tag[self.positions.script_end + 1..self.positions.region_end])
        }
    }

    /// Returns the [variant subtags](https://tools.ietf.org/html/rfc5646#section-2.2.5).
    ///
    /// ```
    /// use oxilangtag::LanguageTag;
    ///
    /// let language_tag = LanguageTag::parse("zh-Latn-TW-pinyin").unwrap();
    /// assert_eq!(language_tag.variant(), Some("pinyin"));
    /// ```
    #[inline]
    pub fn variant(&self) -> Option<&str> {
        if self.positions.region_end == self.positions.variant_end {
            None
        } else {
            Some(&self.tag[self.positions.region_end + 1..self.positions.variant_end])
        }
    }

    /// Iterates on the [variant subtags](https://tools.ietf.org/html/rfc5646#section-2.2.5).
    ///
    /// ```
    /// use oxilangtag::LanguageTag;
    ///
    /// let language_tag = LanguageTag::parse("zh-Latn-TW-pinyin").unwrap();
    /// assert_eq!(language_tag.variant_subtags().collect::<Vec<_>>(), vec!["pinyin"]);
    /// ```
    #[inline]
    pub fn variant_subtags(&self) -> impl Iterator<Item = &str> {
        self.variant().unwrap_or("").split_terminator('-')
    }

    /// Returns the [extension subtags](https://tools.ietf.org/html/rfc5646#section-2.2.6).
    ///
    /// ```
    /// use oxilangtag::LanguageTag;
    ///
    /// let language_tag = LanguageTag::parse("de-DE-u-co-phonebk").unwrap();
    /// assert_eq!(language_tag.extension(), Some("u-co-phonebk"));
    /// ```
    #[inline]
    pub fn extension(&self) -> Option<&str> {
        if self.positions.variant_end == self.positions.extension_end {
            None
        } else {
            Some(&self.tag[self.positions.variant_end + 1..self.positions.extension_end])
        }
    }

    /// Iterates on the [extension subtags](https://tools.ietf.org/html/rfc5646#section-2.2.6).
    ///
    /// ```
    /// use oxilangtag::LanguageTag;
    ///
    /// let language_tag = LanguageTag::parse("de-DE-u-co-phonebk").unwrap();
    /// assert_eq!(language_tag.extension_subtags().collect::<Vec<_>>(), vec![('u', "co-phonebk")]);
    /// ```
    #[inline]
    pub fn extension_subtags(&self) -> impl Iterator<Item = (char, &str)> {
        match self.extension() {
            Some(parts) => ExtensionsIterator::new(parts),
            None => ExtensionsIterator::new(""),
        }
    }

    /// Returns the [private use subtags](https://tools.ietf.org/html/rfc5646#section-2.2.7).
    ///
    /// ```
    /// use oxilangtag::LanguageTag;
    ///
    /// let language_tag = LanguageTag::parse("de-x-foo-bar").unwrap();
    /// assert_eq!(language_tag.private_use(), Some("x-foo-bar"));
    /// ```
    #[inline]
    pub fn private_use(&self) -> Option<&str> {
        if self.tag.starts_with("x-") {
            Some(&self.tag)
        } else if self.positions.extension_end == self.tag.len() {
            None
        } else {
            Some(&self.tag[self.positions.extension_end + 1..])
        }
    }

    /// Iterates on the [private use subtags](https://tools.ietf.org/html/rfc5646#section-2.2.7).
    ///
    /// ```
    /// use oxilangtag::LanguageTag;
    ///
    /// let language_tag = LanguageTag::parse("de-x-foo-bar").unwrap();
    /// assert_eq!(language_tag.private_use_subtags().collect::<Vec<_>>(), vec!["foo", "bar"]);
    /// ```
    #[inline]
    pub fn private_use_subtags(&self) -> impl Iterator<Item = &str> {
        self.private_use()
            .map(|part| &part[2..])
            .unwrap_or("")
            .split_terminator('-')
    }
}

impl LanguageTag<String> {
    /// Parses a language tag acccording to [RFC 5646](https://tools.ietf.org/html/rfc5646)
    /// and normalizes its case.
    ///
    /// This parser accepts the language tags that are "well-formed" according to
    /// [RFC 5646](https://tools.ietf.org/html/rfc5646#section-2.2.9).
    ///
    /// This operation does heap allocation.
    ///
    /// ```
    /// use oxilangtag::LanguageTag;
    ///
    /// let language_tag = LanguageTag::parse_and_normalize("en-us").unwrap();
    /// assert_eq!(language_tag.into_inner(), "en-US")
    /// ```
    pub fn parse_and_normalize(tag: &str) -> Result<Self, LanguageTagParseError> {
        let mut output_buffer = String::with_capacity(tag.len());
        let positions = parse_language_tag(tag, &mut output_buffer)?;
        Ok(Self {
            tag: output_buffer,
            positions,
        })
    }
}

impl<Lft: PartialEq<Rhs>, Rhs> PartialEq<LanguageTag<Rhs>> for LanguageTag<Lft> {
    #[inline]
    fn eq(&self, other: &LanguageTag<Rhs>) -> bool {
        self.tag.eq(&other.tag)
    }
}

impl<T: PartialEq<str>> PartialEq<str> for LanguageTag<T> {
    #[inline]
    fn eq(&self, other: &str) -> bool {
        self.tag.eq(other)
    }
}

impl<'a, T: PartialEq<&'a str>> PartialEq<&'a str> for LanguageTag<T> {
    #[inline]
    fn eq(&self, other: &&'a str) -> bool {
        self.tag.eq(other)
    }
}

impl<T: PartialEq<String>> PartialEq<String> for LanguageTag<T> {
    #[inline]
    fn eq(&self, other: &String) -> bool {
        self.tag.eq(other)
    }
}

impl<'a, T: PartialEq<Cow<'a, str>>> PartialEq<Cow<'a, str>> for LanguageTag<T> {
    #[inline]
    fn eq(&self, other: &Cow<'a, str>) -> bool {
        self.tag.eq(other)
    }
}

impl<T: PartialEq<str>> PartialEq<LanguageTag<T>> for str {
    #[inline]
    fn eq(&self, other: &LanguageTag<T>) -> bool {
        other.tag.eq(self)
    }
}

impl<'a, T: PartialEq<&'a str>> PartialEq<LanguageTag<T>> for &'a str {
    #[inline]
    fn eq(&self, other: &LanguageTag<T>) -> bool {
        other.tag.eq(self)
    }
}

impl<T: PartialEq<String>> PartialEq<LanguageTag<T>> for String {
    #[inline]
    fn eq(&self, other: &LanguageTag<T>) -> bool {
        other.tag.eq(self)
    }
}

impl<'a, T: PartialEq<Cow<'a, str>>> PartialEq<LanguageTag<T>> for Cow<'a, str> {
    #[inline]
    fn eq(&self, other: &LanguageTag<T>) -> bool {
        other.tag.eq(self)
    }
}

impl<T: Eq> Eq for LanguageTag<T> {}

impl<T: Hash> Hash for LanguageTag<T> {
    #[inline]
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.tag.hash(state)
    }
}

impl<T: PartialOrd> PartialOrd for LanguageTag<T> {
    #[inline]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        self.tag.partial_cmp(&other.tag)
    }
}

impl<T: Ord> Ord for LanguageTag<T> {
    #[inline]
    fn cmp(&self, other: &Self) -> Ordering {
        self.tag.cmp(&other.tag)
    }
}

impl<T: Deref<Target = str>> Deref for LanguageTag<T> {
    type Target = str;

    #[inline]
    fn deref(&self) -> &str {
        self.tag.deref()
    }
}

impl<T: AsRef<str>> AsRef<str> for LanguageTag<T> {
    #[inline]
    fn as_ref(&self) -> &str {
        self.tag.as_ref()
    }
}

impl<T: Borrow<str>> Borrow<str> for LanguageTag<T> {
    #[inline]
    fn borrow(&self) -> &str {
        self.tag.borrow()
    }
}

impl<T: fmt::Debug> fmt::Debug for LanguageTag<T> {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.tag.fmt(f)
    }
}

impl<T: fmt::Display> fmt::Display for LanguageTag<T> {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.tag.fmt(f)
    }
}

impl FromStr for LanguageTag<String> {
    type Err = LanguageTagParseError;

    #[inline]
    fn from_str(tag: &str) -> Result<Self, LanguageTagParseError> {
        Self::parse_and_normalize(tag)
    }
}

impl<'a> From<LanguageTag<&'a str>> for LanguageTag<String> {
    #[inline]
    fn from(tag: LanguageTag<&'a str>) -> Self {
        Self {
            tag: tag.tag.into(),
            positions: tag.positions,
        }
    }
}

impl<'a> From<LanguageTag<Cow<'a, str>>> for LanguageTag<String> {
    #[inline]
    fn from(tag: LanguageTag<Cow<'a, str>>) -> Self {
        Self {
            tag: tag.tag.into(),
            positions: tag.positions,
        }
    }
}

impl From<LanguageTag<Box<str>>> for LanguageTag<String> {
    #[inline]
    fn from(tag: LanguageTag<Box<str>>) -> Self {
        Self {
            tag: tag.tag.into(),
            positions: tag.positions,
        }
    }
}

impl<'a> From<LanguageTag<&'a str>> for LanguageTag<Cow<'a, str>> {
    #[inline]
    fn from(tag: LanguageTag<&'a str>) -> Self {
        Self {
            tag: tag.tag.into(),
            positions: tag.positions,
        }
    }
}

impl<'a> From<LanguageTag<String>> for LanguageTag<Cow<'a, str>> {
    #[inline]
    fn from(tag: LanguageTag<String>) -> Self {
        Self {
            tag: tag.tag.into(),
            positions: tag.positions,
        }
    }
}

#[cfg(feature = "serde")]
impl<T: Serialize> Serialize for LanguageTag<T> {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        self.tag.serialize(serializer)
    }
}

#[cfg(feature = "serde")]
impl<'de, T: Deref<Target = str> + Deserialize<'de>> Deserialize<'de> for LanguageTag<T> {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<LanguageTag<T>, D::Error> {
        use serde::de::Error;

        Self::parse(T::deserialize(deserializer)?).map_err(D::Error::custom)
    }
}

/// An error raised during [`LanguageTag`](struct.LanguageTag.html) validation.
#[derive(Debug)]
pub struct LanguageTagParseError {
    kind: TagParseErrorKind,
}

impl fmt::Display for LanguageTagParseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self.kind {
            TagParseErrorKind::EmptyExtension => {
                write!(f, "If an extension subtag is present, it must not be empty")
            }
            TagParseErrorKind::EmptyPrivateUse => {
                write!(f, "If the `x` subtag is present, it must not be empty")
            }
            TagParseErrorKind::ForbiddenChar => {
                write!(f, "The langtag contains a char not allowed")
            }
            TagParseErrorKind::InvalidSubtag => write!(
                f,
                "A subtag fails to parse, it does not match any other subtags"
            ),
            TagParseErrorKind::InvalidLanguage => write!(f, "The given language subtag is invalid"),
            TagParseErrorKind::SubtagTooLong => {
                write!(f, "A subtag may be eight characters in length at maximum")
            }
            TagParseErrorKind::EmptySubtag => write!(f, "A subtag should not be empty"),
            TagParseErrorKind::TooManyExtlangs => {
                write!(f, "At maximum three extlangs are allowed")
            }
        }
    }
}

impl Error for LanguageTagParseError {}

#[derive(Debug)]
enum TagParseErrorKind {
    /// If an extension subtag is present, it must not be empty.
    EmptyExtension,
    /// If the `x` subtag is present, it must not be empty.
    EmptyPrivateUse,
    /// The langtag contains a char that is not A-Z, a-z, 0-9 or the dash.
    ForbiddenChar,
    /// A subtag fails to parse, it does not match any other subtags.
    InvalidSubtag,
    /// The given language subtag is invalid.
    InvalidLanguage,
    /// A subtag may be eight characters in length at maximum.
    SubtagTooLong,
    /// A subtag should not be empty.
    EmptySubtag,
    /// At maximum three extlangs are allowed, but zero to one extlangs are preferred.
    TooManyExtlangs,
}

#[derive(Copy, Clone, Debug)]
struct TagElementsPositions {
    language_end: usize,
    extlang_end: usize,
    script_end: usize,
    region_end: usize,
    variant_end: usize,
    extension_end: usize,
}

trait OutputBuffer: Extend<char> {
    fn push(&mut self, c: char);

    fn push_str(&mut self, s: &str);
}

#[derive(Default)]
struct VoidOutputBuffer {}

impl OutputBuffer for VoidOutputBuffer {
    #[inline]
    fn push(&mut self, _: char) {}

    #[inline]
    fn push_str(&mut self, _: &str) {}
}

impl Extend<char> for VoidOutputBuffer {
    #[inline]
    fn extend<T: IntoIterator<Item = char>>(&mut self, _: T) {}
}

impl OutputBuffer for String {
    #[inline]
    fn push(&mut self, c: char) {
        self.push(c);
    }

    #[inline]
    fn push_str(&mut self, s: &str) {
        self.push_str(s);
    }
}

/// Parses language tag following [the RFC5646 grammar](https://tools.ietf.org/html/rfc5646#section-2.1)
fn parse_language_tag(
    input: &str,
    output: &mut impl OutputBuffer,
) -> Result<TagElementsPositions, LanguageTagParseError> {
    //grandfathered tags
    if let Some(tag) = GRANDFATHEREDS
        .iter()
        .find(|record| record.eq_ignore_ascii_case(input))
    {
        output.push_str(tag);
        Ok(TagElementsPositions {
            language_end: tag.len(),
            extlang_end: tag.len(),
            script_end: tag.len(),
            region_end: tag.len(),
            variant_end: tag.len(),
            extension_end: tag.len(),
        })
    } else if input.starts_with("x-") || input.starts_with("X-") {
        // private use
        if !is_alphanumeric_or_dash(input) {
            Err(LanguageTagParseError {
                kind: TagParseErrorKind::ForbiddenChar,
            })
        } else if input.len() == 2 {
            Err(LanguageTagParseError {
                kind: TagParseErrorKind::EmptyPrivateUse,
            })
        } else {
            output.extend(input.chars().map(|c| c.to_ascii_lowercase()));
            Ok(TagElementsPositions {
                language_end: input.len(),
                extlang_end: input.len(),
                script_end: input.len(),
                region_end: input.len(),
                variant_end: input.len(),
                extension_end: input.len(),
            })
        }
    } else {
        parse_langtag(input, output)
    }
}

/// Handles normal tags.
fn parse_langtag(
    input: &str,
    output: &mut impl OutputBuffer,
) -> Result<TagElementsPositions, LanguageTagParseError> {
    #[derive(PartialEq, Eq)]
    enum State {
        Start,
        AfterLanguage,
        AfterExtLang,
        AfterScript,
        AfterRegion,
        InExtension { expected: bool },
        InPrivateUse { expected: bool },
    }

    let mut state = State::Start;
    let mut language_end = 0;
    let mut extlang_end = 0;
    let mut script_end = 0;
    let mut region_end = 0;
    let mut variant_end = 0;
    let mut extension_end = 0;
    let mut extlangs_count = 0;
    for (subtag, end) in SubTagIterator::new(input) {
        if subtag.is_empty() {
            return Err(LanguageTagParseError {
                kind: TagParseErrorKind::EmptySubtag,
            });
        }
        if subtag.len() > 8 {
            return Err(LanguageTagParseError {
                kind: TagParseErrorKind::SubtagTooLong,
            });
        }
        if state == State::Start {
            // Primary language
            if subtag.len() < 2 || !is_alphabetic(subtag) {
                return Err(LanguageTagParseError {
                    kind: TagParseErrorKind::InvalidLanguage,
                });
            }
            language_end = end;
            output.extend(to_lowercase(subtag));
            if subtag.len() < 4 {
                // extlangs are only allowed for short language tags
                state = State::AfterLanguage;
            } else {
                state = State::AfterExtLang;
            }
        } else if let State::InPrivateUse { .. } = state {
            if !is_alphanumeric(subtag) {
                return Err(LanguageTagParseError {
                    kind: TagParseErrorKind::InvalidSubtag,
                });
            }
            output.push('-');
            output.extend(to_lowercase(subtag));
            state = State::InPrivateUse { expected: false };
        } else if subtag == "x" || subtag == "X" {
            // We make sure extension is found
            if let State::InExtension { expected: true } = state {
                return Err(LanguageTagParseError {
                    kind: TagParseErrorKind::EmptyExtension,
                });
            }
            output.push('-');
            output.push('x');
            state = State::InPrivateUse { expected: true };
        } else if subtag.len() == 1 && is_alphanumeric(subtag) {
            // We make sure extension is found
            if let State::InExtension { expected: true } = state {
                return Err(LanguageTagParseError {
                    kind: TagParseErrorKind::EmptyExtension,
                });
            }
            let extension_tag = subtag.chars().next().unwrap().to_ascii_lowercase();
            output.push('-');
            output.push(extension_tag);
            state = State::InExtension { expected: true };
        } else if let State::InExtension { .. } = state {
            if !is_alphanumeric(subtag) {
                return Err(LanguageTagParseError {
                    kind: TagParseErrorKind::InvalidSubtag,
                });
            }
            extension_end = end;
            output.push('-');
            output.extend(to_lowercase(subtag));
            state = State::InExtension { expected: false };
        } else if state == State::AfterLanguage && subtag.len() == 3 && is_alphabetic(subtag) {
            extlangs_count += 1;
            if extlangs_count > 3 {
                return Err(LanguageTagParseError {
                    kind: TagParseErrorKind::TooManyExtlangs,
                });
            }
            // valid extlangs
            extlang_end = end;
            output.push('-');
            output.extend(to_lowercase(subtag));
        } else if (state == State::AfterLanguage || state == State::AfterExtLang)
            && subtag.len() == 4
            && is_alphabetic(subtag)
        {
            // Script
            script_end = end;
            output.push('-');
            output.extend(to_uppercase_first(subtag));
            state = State::AfterScript;
        } else if (state == State::AfterLanguage
            || state == State::AfterExtLang
            || state == State::AfterScript)
            && (subtag.len() == 2 && is_alphabetic(subtag)
                || subtag.len() == 3 && is_numeric(subtag))
        {
            // Region
            region_end = end;
            output.push('-');
            output.extend(to_uppercase(subtag));
            state = State::AfterRegion;
        } else if (state == State::AfterLanguage
            || state == State::AfterExtLang
            || state == State::AfterScript
            || state == State::AfterRegion)
            && is_alphanumeric(subtag)
            && (subtag.len() >= 5 && is_alphabetic(&subtag[0..1])
                || subtag.len() >= 4 && is_numeric(&subtag[0..1]))
        {
            // Variant
            variant_end = end;
            output.push('-');
            output.extend(to_lowercase(subtag));
            state = State::AfterRegion;
        } else {
            return Err(LanguageTagParseError {
                kind: TagParseErrorKind::InvalidSubtag,
            });
        }
    }

    //We make sure we are in a correct final state
    if let State::InExtension { expected: true } = state {
        return Err(LanguageTagParseError {
            kind: TagParseErrorKind::EmptyExtension,
        });
    }
    if let State::InPrivateUse { expected: true } = state {
        return Err(LanguageTagParseError {
            kind: TagParseErrorKind::EmptyPrivateUse,
        });
    }

    //We make sure we have not skipped anyone
    if extlang_end < language_end {
        extlang_end = language_end;
    }
    if script_end < extlang_end {
        script_end = extlang_end;
    }
    if region_end < script_end {
        region_end = script_end;
    }
    if variant_end < region_end {
        variant_end = region_end;
    }
    if extension_end < variant_end {
        extension_end = variant_end;
    }

    Ok(TagElementsPositions {
        language_end,
        extlang_end,
        script_end,
        region_end,
        variant_end,
        extension_end,
    })
}

struct ExtensionsIterator<'a> {
    input: &'a str,
}

impl<'a> ExtensionsIterator<'a> {
    fn new(input: &'a str) -> Self {
        Self { input }
    }
}

impl<'a> Iterator for ExtensionsIterator<'a> {
    type Item = (char, &'a str);

    fn next(&mut self) -> Option<(char, &'a str)> {
        let mut parts_iterator = self.input.split_terminator('-');
        let singleton = parts_iterator.next()?.chars().next().unwrap();
        let mut content_size: usize = 2;
        for part in parts_iterator {
            if part.len() == 1 {
                let content = &self.input[2..content_size - 1];
                self.input = &self.input[content_size..];
                return Some((singleton, content));
            } else {
                content_size += part.len() + 1;
            }
        }
        let result = self.input.get(2..).map(|content| (singleton, content));
        self.input = "";
        result
    }
}

struct SubTagIterator<'a> {
    split: Split<'a, char>,
    position: usize,
}

impl<'a> SubTagIterator<'a> {
    #[inline]
    fn new(input: &'a str) -> Self {
        Self {
            split: input.split('-'),
            position: 0,
        }
    }
}

impl<'a> Iterator for SubTagIterator<'a> {
    type Item = (&'a str, usize);

    #[inline]
    fn next(&mut self) -> Option<(&'a str, usize)> {
        let tag = self.split.next()?;
        let tag_end = self.position + tag.len();
        self.position = tag_end + 1;
        Some((tag, tag_end))
    }
}

#[inline]
fn is_alphabetic(s: &str) -> bool {
    s.chars().all(|x| x.is_ascii_alphabetic())
}

#[inline]
fn is_numeric(s: &str) -> bool {
    s.chars().all(|x| x.is_ascii_digit())
}

#[inline]
fn is_alphanumeric(s: &str) -> bool {
    s.chars().all(|x| x.is_ascii_alphanumeric())
}

#[inline]
fn is_alphanumeric_or_dash(s: &str) -> bool {
    s.chars().all(|x| x.is_ascii_alphanumeric() || x == '-')
}

#[inline]
fn to_uppercase(s: &str) -> impl Iterator<Item = char> + '_ {
    s.chars().map(|c| c.to_ascii_uppercase())
}

// Beware: panics if s.len() == 0 (should never happen in our code)
#[inline]
fn to_uppercase_first(s: &str) -> impl Iterator<Item = char> + '_ {
    let mut chars = s.chars();
    once(chars.next().unwrap().to_ascii_uppercase()).chain(chars.map(|c| c.to_ascii_lowercase()))
}

#[inline]
fn to_lowercase(s: &str) -> impl Iterator<Item = char> + '_ {
    s.chars().map(|c| c.to_ascii_lowercase())
}

const GRANDFATHEREDS: [&str; 26] = [
    "art-lojban",
    "cel-gaulish",
    "en-GB-oed",
    "i-ami",
    "i-bnn",
    "i-default",
    "i-enochian",
    "i-hak",
    "i-klingon",
    "i-lux",
    "i-mingo",
    "i-navajo",
    "i-pwn",
    "i-tao",
    "i-tay",
    "i-tsu",
    "no-bok",
    "no-nyn",
    "sgn-BE-FR",
    "sgn-BE-NL",
    "sgn-CH-DE",
    "zh-guoyu",
    "zh-hakka",
    "zh-min",
    "zh-min-nan",
    "zh-xiang",
];
