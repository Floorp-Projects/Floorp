// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data provider struct definitions for vertical fallback.
//!
//! Read more about data providers: [`icu_provider`]

// Provider structs must be stable
#![allow(clippy::exhaustive_structs)]

use icu_locid::extensions::unicode::Key;
use icu_locid::subtags::{Language, Region, Script};
use icu_locid::{subtags_region as region, subtags_script as script};
use tinystr::TinyAsciiStr;

use icu_provider::prelude::*;

use zerovec::ule::UnvalidatedStr;
use zerovec::ZeroMap;
use zerovec::ZeroMap2d;

// We use raw TinyAsciiStrs for map keys, as we then don't have to
// validate them as subtags on deserialization. Map lookup can be
// done even if they are not valid tags (an invalid key will just
// become inaccessible).
type UnvalidatedLanguage = TinyAsciiStr<3>;
type UnvalidatedScript = TinyAsciiStr<4>;
type UnvalidatedRegion = TinyAsciiStr<3>;

/// Locale fallback rules derived from likely subtags data.
#[icu_provider::data_struct(LocaleFallbackLikelySubtagsV1Marker = "fallback/likelysubtags@1")]
#[derive(Default, Clone, PartialEq, Debug)]
#[cfg_attr(
    feature = "datagen",
    derive(serde::Serialize, databake::Bake),
    databake(path = icu_provider_adapters::fallback::provider),
)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub struct LocaleFallbackLikelySubtagsV1<'data> {
    /// Map from language to the default script in that language. Languages whose default script
    /// is `Latn` are not included in the map for data size savings.
    ///
    /// Example: "zh" defaults to "Hans", which is in this map.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub l2s: ZeroMap<'data, UnvalidatedLanguage, Script>,
    /// Map from language-region pairs to a script. Only populated if the script is different
    /// from the one in `l2s` for that language.
    ///
    /// Example: "zh-TW" defaults to "Hant", which is in this map.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub lr2s: ZeroMap2d<'data, UnvalidatedLanguage, UnvalidatedRegion, Script>,
    /// Map from language to the default region in that language. Languages whose default region
    /// is `ZZ` are not included in the map for data size savings.
    ///
    /// Example: "zh" defaults to "CN".
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub l2r: ZeroMap<'data, UnvalidatedLanguage, Region>,
    /// Map from language-script pairs to a region. Only populated if the region is different
    /// from the one in `l2r` for that language.
    ///
    /// Example: "zh-Hant" defaults to "TW".
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub ls2r: ZeroMap2d<'data, UnvalidatedLanguage, UnvalidatedScript, Region>,
}

/// `Latn` is the most common script, so it is defaulted for data size savings.
pub const DEFAULT_SCRIPT: Script = script!("Latn");

/// `ZZ` is the most common region, so it is defaulted for data size savings.
pub const DEFAULT_REGION: Region = region!("ZZ");

/// Locale fallback rules derived from CLDR parent locales data.
#[icu_provider::data_struct(LocaleFallbackParentsV1Marker = "fallback/parents@1")]
#[derive(Default, Clone, PartialEq, Debug)]
#[cfg_attr(
    feature = "datagen",
    derive(serde::Serialize, databake::Bake),
    databake(path = icu_provider_adapters::fallback::provider),
)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub struct LocaleFallbackParentsV1<'data> {
    /// Map from language identifier to language identifier, indicating that the language on the
    /// left should inherit from the language on the right.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub parents: ZeroMap<'data, UnvalidatedStr, (Language, Option<Script>, Option<Region>)>,
}

/// Key-specific supplemental fallback data.
#[icu_provider::data_struct(marker(
    CollationFallbackSupplementV1Marker,
    "fallback/supplement/co@1"
))]
#[derive(Default, Clone, PartialEq, Debug)]
#[cfg_attr(
    feature = "datagen",
    derive(serde::Serialize, databake::Bake),
    databake(path = icu_provider_adapters::fallback::provider),
)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub struct LocaleFallbackSupplementV1<'data> {
    /// Additional parent locales to supplement the common ones.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub parents: ZeroMap<'data, UnvalidatedStr, (Language, Option<Script>, Option<Region>)>,
    /// Default values for Unicode extension keywords.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub unicode_extension_defaults: ZeroMap2d<'data, Key, UnvalidatedStr, UnvalidatedStr>,
}
