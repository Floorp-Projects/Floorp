// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use crate::locale::ffi::ICU4XLocale;
    use crate::provider::ffi::ICU4XDataProvider;
    use alloc::boxed::Box;
    use core::str;
    use icu_properties::{exemplar_chars, sets};

    use crate::errors::ffi::ICU4XError;

    #[diplomat::opaque]
    /// An ICU4X Unicode Set Property object, capable of querying whether a code point is contained in a set based on a Unicode property.
    #[diplomat::rust_link(icu::properties, Mod)]
    #[diplomat::rust_link(icu::properties::sets::UnicodeSetData, Struct)]
    #[diplomat::rust_link(icu::properties::sets::UnicodeSetData::as_borrowed, FnInStruct, hidden)]
    #[diplomat::rust_link(icu::properties::sets::UnicodeSetData::from_data, FnInStruct, hidden)]
    #[diplomat::rust_link(icu::properties::sets::UnicodeSetDataBorrowed, Struct)]
    pub struct ICU4XUnicodeSetData(pub sets::UnicodeSetData);

    impl ICU4XUnicodeSetData {
        /// Checks whether the string is in the set.
        #[diplomat::rust_link(icu::properties::sets::UnicodeSetDataBorrowed::contains, FnInStruct)]
        pub fn contains(&self, s: &str) -> bool {
            let s = s.as_bytes(); // #2520
            let s = if let Ok(s) = str::from_utf8(s) {
                s
            } else {
                return false;
            };
            self.0.as_borrowed().contains(s)
        }
        /// Checks whether the code point is in the set.
        #[diplomat::rust_link(
            icu::properties::sets::UnicodeSetDataBorrowed::contains_char,
            FnInStruct
        )]
        pub fn contains_char(&self, cp: char) -> bool {
            self.0.as_borrowed().contains_char(cp)
        }
        /// Checks whether the code point (specified as a 32 bit integer, in UTF-32) is in the set.
        #[diplomat::rust_link(
            icu::properties::sets::UnicodeSetDataBorrowed::contains32,
            FnInStruct,
            hidden
        )]
        pub fn contains32(&self, cp: u32) -> bool {
            self.0.as_borrowed().contains32(cp)
        }

        #[diplomat::rust_link(icu::properties::sets::load_basic_emoji, Fn)]
        pub fn load_basic_emoji(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XUnicodeSetData>, ICU4XError> {
            Ok(Box::new(ICU4XUnicodeSetData(sets::load_basic_emoji(
                &provider.0,
            )?)))
        }

        #[diplomat::rust_link(icu::properties::exemplar_chars::load_exemplars_main, Fn)]
        pub fn load_exemplars_main(
            provider: &ICU4XDataProvider,
            locale: &ICU4XLocale,
        ) -> Result<Box<ICU4XUnicodeSetData>, ICU4XError> {
            let locale = locale.to_datalocale();
            Ok(Box::new(ICU4XUnicodeSetData(
                exemplar_chars::load_exemplars_main(&provider.0, &locale)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::exemplar_chars::load_exemplars_auxiliary, Fn)]
        pub fn load_exemplars_auxiliary(
            provider: &ICU4XDataProvider,
            locale: &ICU4XLocale,
        ) -> Result<Box<ICU4XUnicodeSetData>, ICU4XError> {
            let locale = locale.to_datalocale();
            Ok(Box::new(ICU4XUnicodeSetData(
                exemplar_chars::load_exemplars_auxiliary(&provider.0, &locale)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::exemplar_chars::load_exemplars_punctuation, Fn)]
        pub fn load_exemplars_punctuation(
            provider: &ICU4XDataProvider,
            locale: &ICU4XLocale,
        ) -> Result<Box<ICU4XUnicodeSetData>, ICU4XError> {
            let locale = locale.to_datalocale();
            Ok(Box::new(ICU4XUnicodeSetData(
                exemplar_chars::load_exemplars_punctuation(&provider.0, &locale)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::exemplar_chars::load_exemplars_numbers, Fn)]
        pub fn load_exemplars_numbers(
            provider: &ICU4XDataProvider,
            locale: &ICU4XLocale,
        ) -> Result<Box<ICU4XUnicodeSetData>, ICU4XError> {
            let locale = locale.to_datalocale();
            Ok(Box::new(ICU4XUnicodeSetData(
                exemplar_chars::load_exemplars_numbers(&provider.0, &locale)?,
            )))
        }

        #[diplomat::rust_link(icu::properties::exemplar_chars::load_exemplars_index, Fn)]
        pub fn load_exemplars_index(
            provider: &ICU4XDataProvider,
            locale: &ICU4XLocale,
        ) -> Result<Box<ICU4XUnicodeSetData>, ICU4XError> {
            let locale = locale.to_datalocale();
            Ok(Box::new(ICU4XUnicodeSetData(
                exemplar_chars::load_exemplars_index(&provider.0, &locale)?,
            )))
        }
    }
}
