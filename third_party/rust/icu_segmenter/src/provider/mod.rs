// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! 🚧 \[Unstable\] Data provider struct definitions for this ICU4X component.
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. While the serde representation of data structs is guaranteed
//! to be stable, their Rust representation might not be. Use with caution.
//! </div>
//!
//! Read more about data providers: [`icu_provider`]

// Provider structs must be stable
#![allow(clippy::exhaustive_structs, clippy::exhaustive_enums)]

mod lstm;
pub use lstm::*;

// Re-export this from the provider module because it is needed by datagen
#[cfg(feature = "datagen")]
pub use crate::rule_segmenter::RuleStatusType;

use icu_collections::codepointtrie::CodePointTrie;
use icu_provider::prelude::*;
use zerovec::ZeroVec;

#[cfg(feature = "compiled_data")]
#[derive(Debug)]
/// Baked data
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
pub struct Baked;

#[cfg(feature = "compiled_data")]
const _: () = {
    pub mod icu {
        pub use crate as segmenter;
        pub use icu_collections as collections;
    }
    icu_segmenter_data::make_provider!(Baked);
    icu_segmenter_data::impl_segmenter_dictionary_w_auto_v1!(Baked);
    icu_segmenter_data::impl_segmenter_dictionary_wl_ext_v1!(Baked);
    icu_segmenter_data::impl_segmenter_grapheme_v1!(Baked);
    icu_segmenter_data::impl_segmenter_line_v1!(Baked);
    #[cfg(feature = "lstm")]
    icu_segmenter_data::impl_segmenter_lstm_wl_auto_v1!(Baked);
    icu_segmenter_data::impl_segmenter_sentence_v1!(Baked);
    icu_segmenter_data::impl_segmenter_word_v1!(Baked);
};

#[cfg(feature = "datagen")]
/// The latest minimum set of keys required by this component.
pub const KEYS: &[DataKey] = &[
    DictionaryForWordLineExtendedV1Marker::KEY,
    DictionaryForWordOnlyAutoV1Marker::KEY,
    GraphemeClusterBreakDataV1Marker::KEY,
    LineBreakDataV1Marker::KEY,
    LstmForWordLineAutoV1Marker::KEY,
    SentenceBreakDataV1Marker::KEY,
    WordBreakDataV1Marker::KEY,
];

/// Pre-processed Unicode data in the form of tables to be used for rule-based breaking.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(
    marker(LineBreakDataV1Marker, "segmenter/line@1", singleton),
    marker(WordBreakDataV1Marker, "segmenter/word@1", singleton),
    marker(GraphemeClusterBreakDataV1Marker, "segmenter/grapheme@1", singleton),
    marker(SentenceBreakDataV1Marker, "segmenter/sentence@1", singleton)
)]
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(
    feature = "datagen",
    derive(serde::Serialize,databake::Bake),
    databake(path = icu_segmenter::provider),
)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct RuleBreakDataV1<'data> {
    /// Property table for rule-based breaking.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub property_table: RuleBreakPropertyTable<'data>,

    /// Break state table for rule-based breaking.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub break_state_table: RuleBreakStateTable<'data>,

    /// Rule status table for rule-based breaking.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub rule_status_table: RuleStatusTable<'data>,

    /// Number of properties; should be the square root of the length of [`Self::break_state_table`].
    pub property_count: u8,

    /// The index of the last simple state for [`Self::break_state_table`]. (A simple state has no
    /// `left` nor `right` in SegmenterProperty).
    pub last_codepoint_property: i8,

    /// The index of SOT (start of text) state for [`Self::break_state_table`].
    pub sot_property: u8,

    /// The index of EOT (end of text) state [`Self::break_state_table`].
    pub eot_property: u8,

    /// The index of "SA" state (or 127 if the complex language isn't handled) for
    /// [`Self::break_state_table`].
    pub complex_property: u8,
}

/// Property table for rule-based breaking.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, PartialEq, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(
    feature = "datagen",
    derive(serde::Serialize,databake::Bake),
    databake(path = icu_segmenter::provider),
)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct RuleBreakPropertyTable<'data>(
    #[cfg_attr(feature = "serde", serde(borrow))] pub CodePointTrie<'data, u8>,
);

/// Break state table for rule-based breaking.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, PartialEq, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(
    feature = "datagen",
    derive(serde::Serialize,databake::Bake),
    databake(path = icu_segmenter::provider),
)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct RuleBreakStateTable<'data>(
    #[cfg_attr(feature = "serde", serde(borrow))] pub ZeroVec<'data, i8>,
);

/// Rules status data for rule_status and is_word_like of word segmenter.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, PartialEq, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(
    feature = "datagen",
    derive(serde::Serialize,databake::Bake),
    databake(path = icu_segmenter::provider),
)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct RuleStatusTable<'data>(
    #[cfg_attr(feature = "serde", serde(borrow))] pub ZeroVec<'data, u8>,
);

/// char16trie data for dictionary break
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(
    DictionaryForWordOnlyAutoV1Marker = "segmenter/dictionary/w_auto@1",
    DictionaryForWordLineExtendedV1Marker = "segmenter/dictionary/wl_ext@1"
)]
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(
    feature = "datagen",
    derive(serde::Serialize,databake::Bake),
    databake(path = icu_segmenter::provider),
)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct UCharDictionaryBreakDataV1<'data> {
    /// Dictionary data of char16trie.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub trie_data: ZeroVec<'data, u16>,
}

pub(crate) struct UCharDictionaryBreakDataV1Marker;

impl DataMarker for UCharDictionaryBreakDataV1Marker {
    type Yokeable = UCharDictionaryBreakDataV1<'static>;
}
