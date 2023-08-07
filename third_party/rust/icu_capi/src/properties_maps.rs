// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use crate::provider::ffi::ICU4XDataProvider;
    use alloc::boxed::Box;
    use icu_collections::codepointtrie::TrieValue;
    use icu_properties::{maps, GeneralCategory, GeneralCategoryGroup};

    use crate::errors::ffi::ICU4XError;
    use crate::properties_iter::ffi::CodePointRangeIterator;
    use crate::properties_sets::ffi::ICU4XCodePointSetData;

    #[diplomat::opaque]
    /// An ICU4X Unicode Map Property object, capable of querying whether a code point (key) to obtain the Unicode property value, for a specific Unicode property.
    ///
    /// For properties whose values fit into 8 bits.
    #[diplomat::rust_link(icu::properties, Mod)]
    #[diplomat::rust_link(icu::properties::maps::CodePointMapData, Struct)]
    #[diplomat::rust_link(
        icu::properties::maps::CodePointMapData::as_borrowed,
        FnInStruct,
        hidden
    )]
    #[diplomat::rust_link(icu::properties::maps::CodePointMapData::from_data, FnInStruct, hidden)]
    #[diplomat::rust_link(
        icu::properties::maps::CodePointMapData::try_into_converted,
        FnInStruct,
        hidden
    )]
    #[diplomat::rust_link(icu::properties::maps::CodePointMapDataBorrowed, Struct)]
    pub struct ICU4XCodePointMapData8(maps::CodePointMapData<u8>);

    fn convert_8<P: TrieValue>(data: maps::CodePointMapData<P>) -> Box<ICU4XCodePointMapData8> {
        #[allow(clippy::expect_used)] // infallible for the chosen properties
        Box::new(ICU4XCodePointMapData8(
            data.try_into_converted()
                .expect("try_into_converted to u8 must be infallible"),
        ))
    }

    impl ICU4XCodePointMapData8 {
        /// Gets the value for a code point.
        #[diplomat::rust_link(icu::properties::maps::CodePointMapDataBorrowed::get, FnInStruct)]
        pub fn get(&self, cp: char) -> u8 {
            self.0.as_borrowed().get(cp)
        }

        /// Gets the value for a code point (specified as a 32 bit integer, in UTF-32)
        #[diplomat::rust_link(
            icu::properties::maps::CodePointMapDataBorrowed::get32,
            FnInStruct,
            hidden
        )]
        pub fn get32(&self, cp: u32) -> u8 {
            self.0.as_borrowed().get32(cp)
        }

        /// Converts a general category to its corresponding mask value
        ///
        /// Nonexistant general categories will map to the empty mask
        #[diplomat::rust_link(icu::properties::GeneralCategoryGroup, Struct)]
        pub fn general_category_to_mask(gc: u8) -> u32 {
            if let Ok(gc) = GeneralCategory::try_from(gc) {
                let group: GeneralCategoryGroup = gc.into();
                group.into()
            } else {
                0
            }
        }

        /// Produces an iterator over ranges of code points that map to `value`
        #[diplomat::rust_link(
            icu::properties::maps::CodePointMapDataBorrowed::iter_ranges_for_value,
            FnInStruct
        )]
        pub fn iter_ranges_for_value<'a>(&'a self, value: u8) -> Box<CodePointRangeIterator<'a>> {
            Box::new(CodePointRangeIterator(Box::new(
                self.0.as_borrowed().iter_ranges_for_value(value),
            )))
        }

        /// Produces an iterator over ranges of code points that do not map to `value`
        #[diplomat::rust_link(
            icu::properties::maps::CodePointMapDataBorrowed::iter_ranges_for_value_complemented,
            FnInStruct
        )]
        pub fn iter_ranges_for_value_complemented<'a>(
            &'a self,
            value: u8,
        ) -> Box<CodePointRangeIterator<'a>> {
            Box::new(CodePointRangeIterator(Box::new(
                self.0
                    .as_borrowed()
                    .iter_ranges_for_value_complemented(value),
            )))
        }

        /// Given a mask value (the nth bit marks property value = n), produce an iterator over ranges of code points
        /// whose property values are contained in the mask.
        ///
        /// The main mask property supported is that for General_Category, which can be obtained via `general_category_to_mask()` or
        /// by using `ICU4XGeneralCategoryNameToMaskMapper`
        ///
        /// Should only be used on maps for properties with values less than 32 (like Generak_Category),
        /// other maps will have unpredictable results
        #[diplomat::rust_link(
            icu::properties::maps::CodePointMapDataBorrowed::iter_ranges_for_group,
            FnInStruct
        )]
        pub fn iter_ranges_for_mask<'a>(&'a self, mask: u32) -> Box<CodePointRangeIterator<'a>> {
            let ranges = self
                .0
                .as_borrowed()
                .iter_ranges_mapped(move |v| {
                    let val_mask = 1_u32.checked_shl(v.into()).unwrap_or(0);
                    val_mask & mask != 0
                })
                .filter(|v| v.value)
                .map(|v| v.range);
            Box::new(CodePointRangeIterator(Box::new(ranges)))
        }

        /// Gets a [`ICU4XCodePointSetData`] representing all entries in this map that map to the given value
        #[diplomat::rust_link(
            icu::properties::maps::CodePointMapDataBorrowed::get_set_for_value,
            FnInStruct
        )]
        pub fn get_set_for_value(&self, value: u8) -> Box<ICU4XCodePointSetData> {
            Box::new(ICU4XCodePointSetData(
                self.0.as_borrowed().get_set_for_value(value),
            ))
        }

        #[diplomat::rust_link(icu::properties::maps::load_general_category, Fn)]
        pub fn load_general_category(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointMapData8>, ICU4XError> {
            Ok(convert_8(maps::load_general_category(&provider.0)?))
        }

        #[diplomat::rust_link(icu::properties::maps::load_bidi_class, Fn)]
        pub fn load_bidi_class(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointMapData8>, ICU4XError> {
            Ok(convert_8(maps::load_bidi_class(&provider.0)?))
        }

        #[diplomat::rust_link(icu::properties::maps::load_east_asian_width, Fn)]
        pub fn load_east_asian_width(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointMapData8>, ICU4XError> {
            Ok(convert_8(maps::load_east_asian_width(&provider.0)?))
        }

        #[diplomat::rust_link(icu::properties::maps::load_line_break, Fn)]
        pub fn load_line_break(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointMapData8>, ICU4XError> {
            Ok(convert_8(maps::load_line_break(&provider.0)?))
        }

        #[diplomat::rust_link(icu::properties::maps::load_grapheme_cluster_break, Fn)]
        pub fn try_grapheme_cluster_break(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointMapData8>, ICU4XError> {
            Ok(convert_8(maps::load_grapheme_cluster_break(&provider.0)?))
        }

        #[diplomat::rust_link(icu::properties::maps::load_word_break, Fn)]
        pub fn load_word_break(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointMapData8>, ICU4XError> {
            Ok(convert_8(maps::load_word_break(&provider.0)?))
        }

        #[diplomat::rust_link(icu::properties::maps::load_sentence_break, Fn)]
        pub fn load_sentence_break(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointMapData8>, ICU4XError> {
            Ok(convert_8(maps::load_sentence_break(&provider.0)?))
        }
    }

    #[diplomat::opaque]
    /// An ICU4X Unicode Map Property object, capable of querying whether a code point (key) to obtain the Unicode property value, for a specific Unicode property.
    ///
    /// For properties whose values fit into 16 bits.
    #[diplomat::rust_link(icu::properties, Mod)]
    #[diplomat::rust_link(icu::properties::maps::CodePointMapData, Struct)]
    #[diplomat::rust_link(icu::properties::maps::CodePointMapDataBorrowed, Struct)]
    pub struct ICU4XCodePointMapData16(maps::CodePointMapData<u16>);

    impl ICU4XCodePointMapData16 {
        /// Gets the value for a code point.
        #[diplomat::rust_link(icu::properties::maps::CodePointMapDataBorrowed::get, FnInStruct)]
        pub fn get(&self, cp: char) -> u16 {
            self.0.as_borrowed().get(cp)
        }

        /// Gets the value for a code point (specified as a 32 bit integer, in UTF-32)
        #[diplomat::rust_link(
            icu::properties::maps::CodePointMapDataBorrowed::get32,
            FnInStruct,
            hidden
        )]
        pub fn get32(&self, cp: u32) -> u16 {
            self.0.as_borrowed().get32(cp)
        }

        /// Produces an iterator over ranges of code points that map to `value`
        #[diplomat::rust_link(
            icu::properties::maps::CodePointMapDataBorrowed::iter_ranges_for_value,
            FnInStruct
        )]
        pub fn iter_ranges_for_value<'a>(&'a self, value: u16) -> Box<CodePointRangeIterator<'a>> {
            Box::new(CodePointRangeIterator(Box::new(
                self.0.as_borrowed().iter_ranges_for_value(value),
            )))
        }

        /// Produces an iterator over ranges of code points that do not map to `value`
        #[diplomat::rust_link(
            icu::properties::maps::CodePointMapDataBorrowed::iter_ranges_for_value_complemented,
            FnInStruct
        )]
        pub fn iter_ranges_for_value_complemented<'a>(
            &'a self,
            value: u16,
        ) -> Box<CodePointRangeIterator<'a>> {
            Box::new(CodePointRangeIterator(Box::new(
                self.0
                    .as_borrowed()
                    .iter_ranges_for_value_complemented(value),
            )))
        }

        /// Gets a [`ICU4XCodePointSetData`] representing all entries in this map that map to the given value
        #[diplomat::rust_link(
            icu::properties::maps::CodePointMapDataBorrowed::get_set_for_value,
            FnInStruct
        )]
        pub fn get_set_for_value(&self, value: u16) -> Box<ICU4XCodePointSetData> {
            Box::new(ICU4XCodePointSetData(
                self.0.as_borrowed().get_set_for_value(value),
            ))
        }

        #[diplomat::rust_link(icu::properties::maps::load_script, Fn)]
        pub fn load_script(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCodePointMapData16>, ICU4XError> {
            #[allow(clippy::expect_used)] // script is a 16-bit property
            Ok(Box::new(ICU4XCodePointMapData16(
                maps::load_script(&provider.0)?
                    .try_into_converted()
                    .expect("try_into_converted to u16 must be infallible"),
            )))
        }
    }
}
