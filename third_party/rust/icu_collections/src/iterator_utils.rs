// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::codepointtrie::CodePointMapRange;

/// This is an iterator that coalesces adjacent ranges in an iterator over code
/// point ranges
pub(crate) struct RangeListIteratorCoalescer<I, T> {
    iter: I,
    peek: Option<CodePointMapRange<T>>,
}

impl<I, T: Eq> RangeListIteratorCoalescer<I, T>
where
    I: Iterator<Item = CodePointMapRange<T>>,
{
    pub fn new(iter: I) -> Self {
        Self { iter, peek: None }
    }
}

impl<I, T: Eq> Iterator for RangeListIteratorCoalescer<I, T>
where
    I: Iterator<Item = CodePointMapRange<T>>,
{
    type Item = CodePointMapRange<T>;

    fn next(&mut self) -> Option<Self::Item> {
        // Get the initial range we're working with: either a leftover
        // range from last time, or the next range
        let mut ret = if let Some(peek) = self.peek.take() {
            peek
        } else if let Some(next) = self.iter.next() {
            next
        } else {
            // No ranges, exit early
            return None;
        };

        // Keep pulling ranges
        #[allow(clippy::while_let_on_iterator)]
        // can't move the iterator, also we want it to be explicit that we're not draining the iterator
        while let Some(next) = self.iter.next() {
            if *next.range.start() == ret.range.end() + 1 && next.value == ret.value {
                // Range has no gap, coalesce
                ret.range = *ret.range.start()..=*next.range.end();
            } else {
                // Range has a gap, return what we have so far, update
                // peek
                self.peek = Some(next);
                return Some(ret);
            }
        }

        // Ran out of elements, exit
        Some(ret)
    }
}

#[cfg(test)]
mod tests {
    use core::fmt::Debug;
    use icu::collections::codepointinvlist::CodePointInversionListBuilder;
    use icu::collections::codepointtrie::TrieValue;
    use icu::properties::maps::{self, CodePointMapData};
    use icu::properties::sets::{self, CodePointSetData};
    use icu::properties::{GeneralCategory, Script};

    fn test_set(d: CodePointSetData, name: &str) {
        let data = d.as_borrowed();
        let mut builder = CodePointInversionListBuilder::new();
        let mut builder_complement = CodePointInversionListBuilder::new();

        for range in data.iter_ranges() {
            builder.add_range_u32(&range)
        }

        for range in data.iter_ranges_complemented() {
            builder_complement.add_range_u32(&range)
        }

        builder.complement();
        let set1 = builder.build();
        let set2 = builder_complement.build();
        assert_eq!(set1, set2, "Set {name} failed to complement correctly");
    }

    fn test_map<T: TrieValue + Debug>(d: &CodePointMapData<T>, value: T, name: &str) {
        let data = d.as_borrowed();
        let mut builder = CodePointInversionListBuilder::new();
        let mut builder_complement = CodePointInversionListBuilder::new();

        for range in data.iter_ranges_for_value(value) {
            builder.add_range_u32(&range)
        }

        for range in data.iter_ranges_for_value_complemented(value) {
            builder_complement.add_range_u32(&range)
        }

        builder.complement();
        let set1 = builder.build();
        let set2 = builder_complement.build();
        assert_eq!(
            set1, set2,
            "Map {name} failed to complement correctly with value {value:?}"
        );
    }

    #[test]
    fn test_complement_sets() {
        // Stress test the RangeListIteratorComplementer logic by ensuring it works for
        // a whole bunch of binary properties
        let dp = icu_testdata::unstable();
        test_set(sets::load_ascii_hex_digit(&dp).unwrap(), "ASCII_Hex_Digit");
        test_set(sets::load_alnum(&dp).unwrap(), "Alnum");
        test_set(sets::load_alphabetic(&dp).unwrap(), "Alphabetic");
        test_set(sets::load_bidi_control(&dp).unwrap(), "Bidi_Control");
        test_set(sets::load_bidi_mirrored(&dp).unwrap(), "Bidi_Mirrored");
        test_set(sets::load_blank(&dp).unwrap(), "Blank");
        test_set(sets::load_cased(&dp).unwrap(), "Cased");
        test_set(sets::load_case_ignorable(&dp).unwrap(), "Case_Ignorable");
        test_set(
            sets::load_full_composition_exclusion(&dp).unwrap(),
            "Full_Composition_Exclusion",
        );
        test_set(
            sets::load_changes_when_casefolded(&dp).unwrap(),
            "Changes_When_Casefolded",
        );
        test_set(
            sets::load_changes_when_casemapped(&dp).unwrap(),
            "Changes_When_Casemapped",
        );
        test_set(
            sets::load_changes_when_nfkc_casefolded(&dp).unwrap(),
            "Changes_When_NFKC_Casefolded",
        );
        test_set(
            sets::load_changes_when_lowercased(&dp).unwrap(),
            "Changes_When_Lowercased",
        );
        test_set(
            sets::load_changes_when_titlecased(&dp).unwrap(),
            "Changes_When_Titlecased",
        );
        test_set(
            sets::load_changes_when_uppercased(&dp).unwrap(),
            "Changes_When_Uppercased",
        );
        test_set(sets::load_dash(&dp).unwrap(), "Dash");
        test_set(sets::load_deprecated(&dp).unwrap(), "Deprecated");
        test_set(
            sets::load_default_ignorable_code_point(&dp).unwrap(),
            "Default_Ignorable_Code_Point",
        );
        test_set(sets::load_diacritic(&dp).unwrap(), "Diacritic");
        test_set(
            sets::load_emoji_modifier_base(&dp).unwrap(),
            "Emoji_Modifier_Base",
        );
        test_set(sets::load_emoji_component(&dp).unwrap(), "Emoji_Component");
        test_set(sets::load_emoji_modifier(&dp).unwrap(), "Emoji_Modifier");
        test_set(sets::load_emoji(&dp).unwrap(), "Emoji");
        test_set(
            sets::load_emoji_presentation(&dp).unwrap(),
            "Emoji_Presentation",
        );
        test_set(sets::load_extender(&dp).unwrap(), "Extender");
        test_set(
            sets::load_extended_pictographic(&dp).unwrap(),
            "Extended_Pictographic",
        );
        test_set(sets::load_graph(&dp).unwrap(), "Graph");
        test_set(sets::load_grapheme_base(&dp).unwrap(), "Grapheme_Base");
        test_set(sets::load_grapheme_extend(&dp).unwrap(), "Grapheme_Extend");
        test_set(sets::load_grapheme_link(&dp).unwrap(), "Grapheme_Link");
        test_set(sets::load_hex_digit(&dp).unwrap(), "Hex_Digit");
        test_set(sets::load_hyphen(&dp).unwrap(), "Hyphen");
        test_set(sets::load_id_continue(&dp).unwrap(), "Id_Continue");
        test_set(sets::load_ideographic(&dp).unwrap(), "Ideographic");
        test_set(sets::load_id_start(&dp).unwrap(), "Id_Start");
        test_set(
            sets::load_ids_binary_operator(&dp).unwrap(),
            "Ids_Binary_Operator",
        );
        test_set(
            sets::load_ids_trinary_operator(&dp).unwrap(),
            "Ids_Trinary_Operator",
        );
        test_set(sets::load_join_control(&dp).unwrap(), "Join_Control");
        test_set(
            sets::load_logical_order_exception(&dp).unwrap(),
            "Logical_Order_Exception",
        );
        test_set(sets::load_lowercase(&dp).unwrap(), "Lowercase");
        test_set(sets::load_math(&dp).unwrap(), "Math");
        test_set(
            sets::load_noncharacter_code_point(&dp).unwrap(),
            "Noncharacter_Code_Point",
        );
        test_set(sets::load_nfc_inert(&dp).unwrap(), "NFC_Inert");
        test_set(sets::load_nfd_inert(&dp).unwrap(), "NFD_Inert");
        test_set(sets::load_nfkc_inert(&dp).unwrap(), "NFKC_Inert");
        test_set(sets::load_nfkd_inert(&dp).unwrap(), "NFKD_Inert");
        test_set(sets::load_pattern_syntax(&dp).unwrap(), "Pattern_Syntax");
        test_set(
            sets::load_pattern_white_space(&dp).unwrap(),
            "Pattern_White_Space",
        );
        test_set(
            sets::load_prepended_concatenation_mark(&dp).unwrap(),
            "Prepended_Concatenation_Mark",
        );
        test_set(sets::load_print(&dp).unwrap(), "Print");
        test_set(sets::load_quotation_mark(&dp).unwrap(), "Quotation_Mark");
        test_set(sets::load_radical(&dp).unwrap(), "Radical");
        test_set(
            sets::load_regional_indicator(&dp).unwrap(),
            "Regional_Indicator",
        );
        test_set(sets::load_soft_dotted(&dp).unwrap(), "Soft_Dotted");
        test_set(sets::load_segment_starter(&dp).unwrap(), "Segment_Starter");
        test_set(sets::load_case_sensitive(&dp).unwrap(), "Case_Sensitive");
        test_set(
            sets::load_sentence_terminal(&dp).unwrap(),
            "Sentence_Terminal",
        );
        test_set(
            sets::load_terminal_punctuation(&dp).unwrap(),
            "Terminal_Punctuation",
        );
        test_set(
            sets::load_unified_ideograph(&dp).unwrap(),
            "Unified_Ideograph",
        );
        test_set(sets::load_uppercase(&dp).unwrap(), "Uppercase");
        test_set(
            sets::load_variation_selector(&dp).unwrap(),
            "Variation_Selector",
        );
        test_set(sets::load_white_space(&dp).unwrap(), "White_Space");
        test_set(sets::load_xdigit(&dp).unwrap(), "Xdigit");
        test_set(sets::load_xid_continue(&dp).unwrap(), "XID_Continue");
        test_set(sets::load_xid_start(&dp).unwrap(), "XID_Start");
    }

    #[test]
    fn test_complement_maps() {
        let dp = icu_testdata::unstable();
        let gc = maps::load_general_category(&dp).unwrap();
        let script = maps::load_script(&dp).unwrap();
        test_map(&gc, GeneralCategory::UppercaseLetter, "gc");
        test_map(&gc, GeneralCategory::OtherPunctuation, "gc");
        test_map(&script, Script::Devanagari, "script");
        test_map(&script, Script::Latin, "script");
        test_map(&script, Script::Common, "script");
    }
}
