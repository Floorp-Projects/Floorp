// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use crate::provider::ffi::ICU4XDataProvider;
    use alloc::boxed::Box;
    use core::str;
    use icu_properties::sets;

    use crate::errors::ffi::ICU4XError;
    use crate::properties_iter::ffi::CodePointRangeIterator;

    #[diplomat::opaque]
    /// An ICU4X Unicode Set Property object, capable of querying whether a code point is contained in a set based on a Unicode property.
    #[diplomat::rust_link(icu::properties, Mod)]
    #[diplomat::rust_link(icu::properties::sets::CodePointSetData, Struct)]
    #[diplomat::rust_link(
        icu::properties::sets::CodePointSetData::as_borrowed,
        FnInStruct,
        hidden
    )]
    #[diplomat::rust_link(icu::properties::sets::CodePointSetData::from_data, FnInStruct, hidden)]
    #[diplomat::rust_link(icu::properties::sets::CodePointSetDataBorrowed, Struct)]
    pub struct ICU4XCodePointSetData(pub sets::CodePointSetData);

    impl ICU4XCodePointSetData {
        /// Checks whether the code point is in the set.
        #[diplomat::rust_link(
            icu::properties::sets::CodePointSetDataBorrowed::contains,
            FnInStruct
        )]
        pub fn contains(&self, cp: char) -> bool {
            self.0.as_borrowed().contains(cp)
        }
        /// Checks whether the code point (specified as a 32 bit integer, in UTF-32) is in the set.
        #[diplomat::rust_link(
            icu::properties::sets::CodePointSetDataBorrowed::contains32,
            FnInStruct,
            hidden
        )]
        pub fn contains32(&self, cp: u32) -> bool {
            self.0.as_borrowed().contains32(cp)
        }

        /// Produces an iterator over ranges of code points contained in this set
        #[diplomat::rust_link(
            icu::properties::sets::CodePointSetDataBorrowed::iter_ranges,
            FnInStruct
        )]
        pub fn iter_ranges<'a>(&'a self) -> Box<CodePointRangeIterator<'a>> {
            Box::new(CodePointRangeIterator(Box::new(
                self.0.as_borrowed().iter_ranges(),
            )))
        }

        /// Produces an iterator over ranges of code points not contained in this set
        #[diplomat::rust_link(
            icu::properties::sets::CodePointSetDataBorrowed::iter_ranges_complemented,
            FnInStruct
        )]
        pub fn iter_ranges_complemented<'a>(&'a self) -> Box<CodePointRangeIterator<'a>> {
            Box::new(CodePointRangeIterator(Box::new(
                self.0.as_borrowed().iter_ranges_complemented(),
            )))
        }

        /// which is a mask with the same format as the `U_GC_XX_MASK` mask in ICU4C
        #[diplomat::rust_link(icu::properties::sets::load_for_general_category_group, Fn)]
        pub fn load_for_general_category_group(
            provider: &ICU4XDataProvider,
            group: u32,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_for_general_category_group(&provider.0, group.into())?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_ascii_hex_digit, Fn)]
        pub fn load_ascii_hex_digit(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_ascii_hex_digit(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_alnum, Fn)]
        pub fn load_alnum(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_alnum(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_alphabetic, Fn)]
        pub fn load_alphabetic(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_alphabetic(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_bidi_control, Fn)]
        pub fn load_bidi_control(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_bidi_control(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_bidi_mirrored, Fn)]
        pub fn load_bidi_mirrored(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_bidi_mirrored(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_blank, Fn)]
        pub fn load_blank(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_blank(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_cased, Fn)]
        pub fn load_cased(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_cased(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_case_ignorable, Fn)]
        pub fn load_case_ignorable(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_case_ignorable(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_full_composition_exclusion, Fn)]
        pub fn load_full_composition_exclusion(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_full_composition_exclusion(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_changes_when_casefolded, Fn)]
        pub fn load_changes_when_casefolded(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_changes_when_casefolded(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_changes_when_casemapped, Fn)]
        pub fn load_changes_when_casemapped(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_changes_when_casemapped(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_changes_when_nfkc_casefolded, Fn)]
        pub fn load_changes_when_nfkc_casefolded(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_changes_when_nfkc_casefolded(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_changes_when_lowercased, Fn)]
        pub fn load_changes_when_lowercased(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_changes_when_lowercased(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_changes_when_titlecased, Fn)]
        pub fn load_changes_when_titlecased(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_changes_when_titlecased(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_changes_when_uppercased, Fn)]
        pub fn load_changes_when_uppercased(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_changes_when_uppercased(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_dash, Fn)]
        pub fn load_dash(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_dash(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_deprecated, Fn)]
        pub fn load_deprecated(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_deprecated(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_default_ignorable_code_point, Fn)]
        pub fn load_default_ignorable_code_point(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_default_ignorable_code_point(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_diacritic, Fn)]
        pub fn load_diacritic(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_diacritic(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_emoji_modifier_base, Fn)]
        pub fn load_emoji_modifier_base(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_emoji_modifier_base(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_emoji_component, Fn)]
        pub fn load_emoji_component(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_emoji_component(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_emoji_modifier, Fn)]
        pub fn load_emoji_modifier(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_emoji_modifier(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_emoji, Fn)]
        pub fn load_emoji(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_emoji(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_emoji_presentation, Fn)]
        pub fn load_emoji_presentation(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_emoji_presentation(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_extender, Fn)]
        pub fn load_extender(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_extender(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_extended_pictographic, Fn)]
        pub fn load_extended_pictographic(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_extended_pictographic(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_graph, Fn)]
        pub fn load_graph(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_graph(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_grapheme_base, Fn)]
        pub fn load_grapheme_base(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_grapheme_base(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_grapheme_extend, Fn)]
        pub fn load_grapheme_extend(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_grapheme_extend(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_grapheme_link, Fn)]
        pub fn load_grapheme_link(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_grapheme_link(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_hex_digit, Fn)]
        pub fn load_hex_digit(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_hex_digit(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_hyphen, Fn)]
        pub fn load_hyphen(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_hyphen(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_id_continue, Fn)]
        pub fn load_id_continue(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_id_continue(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_ideographic, Fn)]
        pub fn load_ideographic(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_ideographic(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_id_start, Fn)]
        pub fn load_id_start(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_id_start(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_ids_binary_operator, Fn)]
        pub fn load_ids_binary_operator(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_ids_binary_operator(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_ids_trinary_operator, Fn)]
        pub fn load_ids_trinary_operator(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_ids_trinary_operator(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_join_control, Fn)]
        pub fn load_join_control(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_join_control(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_logical_order_exception, Fn)]
        pub fn load_logical_order_exception(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_logical_order_exception(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_lowercase, Fn)]
        pub fn load_lowercase(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_lowercase(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_math, Fn)]
        pub fn load_math(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_math(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_noncharacter_code_point, Fn)]
        pub fn load_noncharacter_code_point(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_noncharacter_code_point(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_nfc_inert, Fn)]
        pub fn load_nfc_inert(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_nfc_inert(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_nfd_inert, Fn)]
        pub fn load_nfd_inert(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_nfd_inert(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_nfkc_inert, Fn)]
        pub fn load_nfkc_inert(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_nfkc_inert(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_nfkd_inert, Fn)]
        pub fn load_nfkd_inert(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_nfkd_inert(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_pattern_syntax, Fn)]
        pub fn load_pattern_syntax(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_pattern_syntax(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_pattern_white_space, Fn)]
        pub fn load_pattern_white_space(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_pattern_white_space(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_prepended_concatenation_mark, Fn)]
        pub fn load_prepended_concatenation_mark(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_prepended_concatenation_mark(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_print, Fn)]
        pub fn load_print(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_print(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_quotation_mark, Fn)]
        pub fn load_quotation_mark(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_quotation_mark(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_radical, Fn)]
        pub fn load_radical(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_radical(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_regional_indicator, Fn)]
        pub fn load_regional_indicator(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_regional_indicator(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_soft_dotted, Fn)]
        pub fn load_soft_dotted(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_soft_dotted(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_segment_starter, Fn)]
        pub fn load_segment_starter(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_segment_starter(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_case_sensitive, Fn)]
        pub fn load_case_sensitive(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_case_sensitive(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_sentence_terminal, Fn)]
        pub fn load_sentence_terminal(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_sentence_terminal(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_terminal_punctuation, Fn)]
        pub fn load_terminal_punctuation(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_terminal_punctuation(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_unified_ideograph, Fn)]
        pub fn load_unified_ideograph(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_unified_ideograph(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_uppercase, Fn)]
        pub fn load_uppercase(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_uppercase(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_variation_selector, Fn)]
        pub fn load_variation_selector(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_variation_selector(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::sets::load_white_space, Fn)]
        pub fn load_white_space(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_white_space(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_xdigit, Fn)]
        pub fn load_xdigit(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_xdigit(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_xid_continue, Fn)]
        pub fn load_xid_continue(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_xid_continue(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::sets::load_xid_start, Fn)]
        pub fn load_xid_start(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            Ok(Box::new(ICU4XCodePointSetData(sets::load_xid_start(
                &provider.0,
            )?)))
        }

        /// Loads data for a property specified as a string as long as it is one of the
        /// [ECMA-262 binary properties][ecma] (not including Any, ASCII, and Assigned pseudoproperties).
        ///
        /// Returns `ICU4XError::PropertyUnexpectedPropertyNameError` in case the string does not
        /// match any property in the list
        ///
        /// [ecma]: https://tc39.es/ecma262/#table-binary-unicode-properties
        #[diplomat::rust_link(icu::properties::sets::load_for_ecma262_unstable, Fn)]
        #[diplomat::rust_link(
            icu::properties::sets::load_for_ecma262_with_any_provider,
            Fn,
            hidden
        )]
        #[diplomat::rust_link(
            icu::properties::sets::load_for_ecma262_with_buffer_provider,
            Fn,
            hidden
        )]
        pub fn load_for_ecma262(
            provider: &ICU4XDataProvider,
            property_name: &str,
        ) -> Result<Box<ICU4XCodePointSetData>, ICU4XError> {
            let name = property_name.as_bytes(); // #2520
            let name = if let Ok(s) = str::from_utf8(name) {
                s
            } else {
                return Err(ICU4XError::TinyStrNonAsciiError);
            };
            Ok(Box::new(ICU4XCodePointSetData(
                sets::load_for_ecma262_unstable(&provider.0, name)?,
            )))
        }
    }
}
