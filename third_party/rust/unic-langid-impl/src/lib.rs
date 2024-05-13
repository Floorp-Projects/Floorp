mod errors;
mod layout_table;
#[cfg(feature = "likelysubtags")]
pub mod likelysubtags;
#[doc(hidden)]
pub mod parser;
#[cfg(feature = "serde")]
mod serde;
pub mod subtags;

pub use crate::errors::LanguageIdentifierError;
use std::fmt::Write;
use std::iter::Peekable;
use std::str::FromStr;

/// Enum representing available character direction orientations.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum CharacterDirection {
    /// Right To Left
    ///
    /// Used in languages such as Arabic, Hebrew, Fula, Kurdish etc.
    RTL,
    /// Left To Right
    ///
    /// Used in languages such as French, Spanish, English, German etc.
    LTR,
    /// Top To Bottom
    ///
    /// Used in Traditional Mongolian
    TTB,
}

type PartsTuple = (
    subtags::Language,
    Option<subtags::Script>,
    Option<subtags::Region>,
    Vec<subtags::Variant>,
);

/// `LanguageIdentifier` is a core struct representing a Unicode Language Identifier.
///
/// # Examples
///
/// ```
/// use unic_langid_impl::LanguageIdentifier;
///
/// let li: LanguageIdentifier = "en-US".parse()
///     .expect("Failed to parse.");
///
/// assert_eq!(li.language, "en");
/// assert_eq!(li.script, None);
/// assert_eq!(li.region.as_ref().map(Into::into), Some("US"));
/// assert_eq!(li.variants().len(), 0);
/// ```
///
/// # Parsing
///
/// Unicode recognizes three levels of standard conformance for any language identifier:
///
///  * *well-formed* - syntactically correct
///  * *valid* - well-formed and only uses registered language subtags, extensions, keywords, types...
///  * *canonical* - valid and no deprecated codes or structure.
///
/// At the moment parsing normalizes a well-formed language identifier converting
/// `_` separators to `-` and adjusting casing to conform to the Unicode standard.
///
/// Any bogus subtags will cause the parsing to fail with an error.
/// No subtag validation is performed.
///
/// # Examples:
///
/// ```
/// use unic_langid_impl::LanguageIdentifier;
///
/// let li: LanguageIdentifier = "eN_latn_Us-Valencia".parse()
///     .expect("Failed to parse.");
///
/// assert_eq!(li.language, "en");
/// assert_eq!(li.script.as_ref().map(Into::into), Some("Latn"));
/// assert_eq!(li.region.as_ref().map(Into::into), Some("US"));
/// assert_eq!(li.variants().map(|v| v.as_str()).collect::<Vec<_>>(), &["valencia"]);
/// ```
#[derive(Default, Debug, PartialEq, Eq, Clone, Hash, PartialOrd, Ord)]
pub struct LanguageIdentifier {
    pub language: subtags::Language,
    pub script: Option<subtags::Script>,
    pub region: Option<subtags::Region>,
    variants: Option<Box<[subtags::Variant]>>,
}

impl LanguageIdentifier {
    /// A constructor which takes a utf8 slice, parses it and
    /// produces a well-formed `LanguageIdentifier`.
    ///
    /// # Examples
    ///
    /// ```
    /// use unic_langid_impl::LanguageIdentifier;
    ///
    /// let li = LanguageIdentifier::from_bytes("en-US".as_bytes())
    ///     .expect("Parsing failed.");
    ///
    /// assert_eq!(li.to_string(), "en-US");
    /// ```
    pub fn from_bytes(v: &[u8]) -> Result<Self, LanguageIdentifierError> {
        Ok(parser::parse_language_identifier(v)?)
    }

    /// A constructor which takes optional subtags as `AsRef<[u8]>`, parses them and
    /// produces a well-formed `LanguageIdentifier`.
    ///
    /// # Examples
    ///
    /// ```
    /// use unic_langid_impl::LanguageIdentifier;
    ///
    /// let li = LanguageIdentifier::from_parts(
    ///     "fr".parse().expect("Parsing failed."),
    ///     None,
    ///     Some("CA".parse().expect("Parsing failed.")),
    ///     &[]
    /// );
    ///
    /// assert_eq!(li.to_string(), "fr-CA");
    /// ```
    pub fn from_parts(
        language: subtags::Language,
        script: Option<subtags::Script>,
        region: Option<subtags::Region>,
        variants: &[subtags::Variant],
    ) -> Self {
        let variants = if !variants.is_empty() {
            let mut v = variants.to_vec();
            v.sort_unstable();
            v.dedup();
            Some(v.into_boxed_slice())
        } else {
            None
        };

        Self {
            language,
            script,
            region,
            variants,
        }
    }

    /// # Unchecked
    ///
    /// This function accepts subtags expecting variants
    /// to be deduplicated and ordered.
    pub const fn from_raw_parts_unchecked(
        language: subtags::Language,
        script: Option<subtags::Script>,
        region: Option<subtags::Region>,
        variants: Option<Box<[subtags::Variant]>>,
    ) -> Self {
        Self {
            language,
            script,
            region,
            variants,
        }
    }

    #[doc(hidden)]
    /// This method is used by `unic-locale` to handle partial
    /// subtag iterator.
    ///
    /// Not stable.
    pub fn try_from_iter<'a>(
        iter: &mut Peekable<impl Iterator<Item = &'a [u8]>>,
        allow_extension: bool,
    ) -> Result<LanguageIdentifier, LanguageIdentifierError> {
        Ok(parser::parse_language_identifier_from_iter(
            iter,
            allow_extension,
        )?)
    }

    /// Consumes `LanguageIdentifier` and produces raw internal representations
    /// of all subtags in form of `u64`/`u32`.
    ///
    /// Primarily used for storing internal representation and restoring via
    /// `from_raw_parts_unchecked`.
    ///
    /// # Examples
    ///
    /// ```
    /// use unic_langid_impl::LanguageIdentifier;
    /// use tinystr::{TinyStr8, TinyStr4};
    ///
    /// let li: LanguageIdentifier = "en-US".parse()
    ///     .expect("Parsing failed.");
    ///
    /// let (lang, script, region, variants) = li.into_parts();
    ///
    /// // let li2 = LanguageIdentifier::from_raw_parts_unchecked(
    /// //     lang.map(|l| unsafe { TinyStr8::new_unchecked(l) }),
    /// //    script.map(|s| unsafe { TinyStr4::new_unchecked(s) }),
    /// //    region.map(|r| unsafe { TinyStr4::new_unchecked(r) }),
    /// //    variants.map(|v| v.into_iter().map(|v| unsafe { TinyStr8::new_unchecked(*v) }).collect()),
    /// //);
    ///
    /// //assert_eq!(li2.to_string(), "en-US");
    /// ```
    pub fn into_parts(self) -> PartsTuple {
        (
            self.language,
            self.script,
            self.region,
            self.variants.map_or_else(Vec::new, |v| v.to_vec()),
        )
    }

    /// Compares a `LanguageIdentifier` to another `AsRef<LanguageIdentifier`
    /// allowing for either side to use the missing fields as wildcards.
    ///
    /// This allows for matching between `en` (treated as `en-*-*-*`) and `en-US`.
    ///
    /// # Examples
    ///
    /// ```
    /// use unic_langid_impl::LanguageIdentifier;
    ///
    /// let li1: LanguageIdentifier = "en".parse()
    ///     .expect("Parsing failed.");
    ///
    /// let li2: LanguageIdentifier = "en-US".parse()
    ///     .expect("Parsing failed.");
    ///
    /// assert_ne!(li1, li2); // "en" != "en-US"
    /// assert_ne!(li1.to_string(), li2.to_string()); // "en" != "en-US"
    ///
    /// assert_eq!(li1.matches(&li2, false, false), false); // "en" != "en-US"
    /// assert_eq!(li1.matches(&li2, true, false), true); // "en-*-*-*" == "en-US"
    /// assert_eq!(li1.matches(&li2, false, true), false); // "en" != "en-*-US-*"
    /// assert_eq!(li1.matches(&li2, true, true), true); // "en-*-*-*" == "en-*-US-*"
    /// ```
    pub fn matches<O: AsRef<Self>>(
        &self,
        other: &O,
        self_as_range: bool,
        other_as_range: bool,
    ) -> bool {
        let other = other.as_ref();
        self.language
            .matches(other.language, self_as_range, other_as_range)
            && subtag_matches(&self.script, &other.script, self_as_range, other_as_range)
            && subtag_matches(&self.region, &other.region, self_as_range, other_as_range)
            && subtags_match(
                &self.variants,
                &other.variants,
                self_as_range,
                other_as_range,
            )
    }

    /// Returns a vector of variants subtags of the `LanguageIdentifier`.
    ///
    /// # Examples
    ///
    /// ```
    /// use unic_langid_impl::LanguageIdentifier;
    ///
    /// let li1: LanguageIdentifier = "ca-ES-valencia".parse()
    ///     .expect("Parsing failed.");
    ///
    /// assert_eq!(li1.variants().map(|v| v.as_str()).collect::<Vec<_>>(), &["valencia"]);
    ///
    /// let li2: LanguageIdentifier = "de".parse()
    ///     .expect("Parsing failed.");
    ///
    /// assert_eq!(li2.variants().len(), 0);
    /// ```
    pub fn variants(&self) -> impl ExactSizeIterator<Item = &subtags::Variant> {
        let variants: &[_] = match self.variants {
            Some(ref v) => v,
            None => &[],
        };

        variants.iter()
    }

    /// Sets variant subtags of the `LanguageIdentifier`.
    ///
    /// # Examples
    ///
    /// ```
    /// use unic_langid_impl::LanguageIdentifier;
    ///
    /// let mut li: LanguageIdentifier = "ca-ES".parse()
    ///     .expect("Parsing failed.");
    ///
    /// li.set_variants(&["valencia".parse().expect("Parsing failed.")]);
    ///
    /// assert_eq!(li.to_string(), "ca-ES-valencia");
    /// ```
    pub fn set_variants(&mut self, variants: &[subtags::Variant]) {
        let mut v = variants.to_vec();

        if v.is_empty() {
            self.variants = None;
        } else {
            v.sort_unstable();
            v.dedup();
            self.variants = Some(v.into_boxed_slice());
        }
    }

    /// Tests if a variant subtag is present in the `LanguageIdentifier`.
    ///
    /// # Examples
    ///
    /// ```
    /// use unic_langid_impl::LanguageIdentifier;
    ///
    /// let mut li: LanguageIdentifier = "ca-ES-macos".parse()
    ///     .expect("Parsing failed.");
    ///
    /// assert_eq!(li.has_variant("valencia".parse().unwrap()), false);
    /// assert_eq!(li.has_variant("macos".parse().unwrap()), true);
    /// ```
    pub fn has_variant(&self, variant: subtags::Variant) -> bool {
        if let Some(variants) = &self.variants {
            variants.contains(&variant)
        } else {
            false
        }
    }

    /// Clears variant subtags of the `LanguageIdentifier`.
    ///
    /// # Examples
    ///
    /// ```
    /// use unic_langid_impl::LanguageIdentifier;
    ///
    /// let mut li: LanguageIdentifier = "ca-ES-valencia".parse()
    ///     .expect("Parsing failed.");
    ///
    /// li.clear_variants();
    ///
    /// assert_eq!(li.to_string(), "ca-ES");
    /// ```
    pub fn clear_variants(&mut self) {
        self.variants = None;
    }

    /// Extends the `LanguageIdentifier` adding likely subtags based
    /// on tables provided by CLDR.
    ///
    /// # Examples
    ///
    /// ```
    /// use unic_langid_impl::LanguageIdentifier;
    ///
    /// let mut li: LanguageIdentifier = "en-US".parse()
    ///     .expect("Parsing failed.");
    ///
    /// assert_eq!(li.maximize(), true);
    /// assert_eq!(li.to_string(), "en-Latn-US");
    /// ```
    #[cfg(feature = "likelysubtags")]
    pub fn maximize(&mut self) -> bool {
        if let Some(new_li) = likelysubtags::maximize(self.language, self.script, self.region) {
            self.language = new_li.0;
            self.script = new_li.1;
            self.region = new_li.2;
            true
        } else {
            false
        }
    }

    /// Extends the `LanguageIdentifier` removing likely subtags based
    /// on tables provided by CLDR.
    ///
    /// # Examples
    ///
    /// ```
    /// use unic_langid_impl::LanguageIdentifier;
    ///
    /// let mut li: LanguageIdentifier = "en-Latn-US".parse()
    ///     .expect("Parsing failed.");
    ///
    /// assert_eq!(li.minimize(), true);
    /// assert_eq!(li.to_string(), "en");
    /// ```
    #[cfg(feature = "likelysubtags")]
    pub fn minimize(&mut self) -> bool {
        if let Some(new_li) = likelysubtags::minimize(self.language, self.script, self.region) {
            self.language = new_li.0;
            self.script = new_li.1;
            self.region = new_li.2;
            true
        } else {
            false
        }
    }

    /// Returns character direction of the `LanguageIdentifier`.
    ///
    /// # Examples
    ///
    /// ```
    /// use unic_langid_impl::{LanguageIdentifier, CharacterDirection};
    ///
    /// let li1: LanguageIdentifier = "es-AR".parse()
    ///     .expect("Parsing failed.");
    /// let li2: LanguageIdentifier = "fa".parse()
    ///     .expect("Parsing failed.");
    ///
    /// assert_eq!(li1.character_direction(), CharacterDirection::LTR);
    /// assert_eq!(li2.character_direction(), CharacterDirection::RTL);
    /// ```
    pub fn character_direction(&self) -> CharacterDirection {
        match (self.language.into(), self.script) {
            (_, Some(script))
                if layout_table::SCRIPTS_CHARACTER_DIRECTION_LTR.contains(&script.into()) =>
            {
                CharacterDirection::LTR
            }
            (_, Some(script))
                if layout_table::SCRIPTS_CHARACTER_DIRECTION_RTL.contains(&script.into()) =>
            {
                CharacterDirection::RTL
            }
            (_, Some(script))
                if layout_table::SCRIPTS_CHARACTER_DIRECTION_TTB.contains(&script.into()) =>
            {
                CharacterDirection::TTB
            }
            (Some(lang), _) if layout_table::LANGS_CHARACTER_DIRECTION_RTL.contains(&lang) => {
                #[cfg(feature = "likelysubtags")]
                if let Some((_, Some(script), _)) =
                    likelysubtags::maximize(self.language, None, self.region)
                {
                    if layout_table::SCRIPTS_CHARACTER_DIRECTION_LTR.contains(&script.into()) {
                        return CharacterDirection::LTR;
                    }
                }
                CharacterDirection::RTL
            }
            _ => CharacterDirection::LTR,
        }
    }
}

impl FromStr for LanguageIdentifier {
    type Err = LanguageIdentifierError;

    fn from_str(source: &str) -> Result<Self, Self::Err> {
        Self::from_bytes(source.as_bytes())
    }
}

impl AsRef<LanguageIdentifier> for LanguageIdentifier {
    #[inline(always)]
    fn as_ref(&self) -> &LanguageIdentifier {
        self
    }
}

impl std::fmt::Display for LanguageIdentifier {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        self.language.fmt(f)?;
        if let Some(ref script) = self.script {
            f.write_char('-')?;
            script.fmt(f)?;
        }
        if let Some(ref region) = self.region {
            f.write_char('-')?;
            region.fmt(f)?;
        }
        if let Some(variants) = &self.variants {
            for variant in variants.iter() {
                f.write_char('-')?;
                variant.fmt(f)?;
            }
        }
        Ok(())
    }
}

impl PartialEq<&str> for LanguageIdentifier {
    fn eq(&self, other: &&str) -> bool {
        self.to_string().as_str() == *other
    }
}

fn subtag_matches<P: PartialEq>(
    subtag1: &Option<P>,
    subtag2: &Option<P>,
    as_range1: bool,
    as_range2: bool,
) -> bool {
    (as_range1 && subtag1.is_none()) || (as_range2 && subtag2.is_none()) || subtag1 == subtag2
}

fn is_option_empty<P: PartialEq>(subtag: &Option<Box<[P]>>) -> bool {
    subtag.as_ref().map_or(true, |t| t.is_empty())
}

fn subtags_match<P: PartialEq>(
    subtag1: &Option<Box<[P]>>,
    subtag2: &Option<Box<[P]>>,
    as_range1: bool,
    as_range2: bool,
) -> bool {
    // or is some and is empty!
    (as_range1 && is_option_empty(subtag1))
        || (as_range2 && is_option_empty(subtag2))
        || subtag1 == subtag2
}

/// This is a best-effort operation that performs all available levels of canonicalization.
///
/// At the moment the operation will normalize casing and the separator, but in the future
/// it may also validate and update from deprecated subtags to canonical ones.
///
/// # Examples
///
/// ```
/// use unic_langid_impl::canonicalize;
///
/// assert_eq!(canonicalize("pL_latn_pl"), Ok("pl-Latn-PL".to_string()));
/// ```
pub fn canonicalize<S: AsRef<[u8]>>(input: S) -> Result<String, LanguageIdentifierError> {
    let lang_id = LanguageIdentifier::from_bytes(input.as_ref())?;
    Ok(lang_id.to_string())
}

#[test]
fn invalid_subtag() {
    assert!(LanguageIdentifier::from_bytes("en-ÁÁÁÁ".as_bytes()).is_err());
}
