// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use core::str::{self};

    use alloc::boxed::Box;

    use fixed_decimal::FixedDecimal;
    use icu_plurals::{PluralCategory, PluralOperands, PluralRules};

    use crate::{locale::ffi::ICU4XLocale, provider::ffi::ICU4XDataProvider};

    use crate::errors::ffi::ICU4XError;

    /// FFI version of `PluralCategory`.
    #[diplomat::rust_link(icu::plurals::PluralCategory, Enum)]
    #[diplomat::enum_convert(PluralCategory)]
    pub enum ICU4XPluralCategory {
        Zero,
        One,
        Two,
        Few,
        Many,
        Other,
    }

    impl ICU4XPluralCategory {
        /// Construct from a string in the format
        /// [specified in TR35](https://unicode.org/reports/tr35/tr35-numbers.html#Language_Plural_Rules)
        #[diplomat::rust_link(icu::plurals::PluralCategory::get_for_cldr_string, FnInEnum)]
        #[diplomat::rust_link(icu::plurals::PluralCategory::get_for_cldr_bytes, FnInEnum)]
        pub fn get_for_cldr_string(s: &str) -> Result<ICU4XPluralCategory, ()> {
            let s = s.as_bytes(); // #2520
            PluralCategory::get_for_cldr_bytes(s)
                .ok_or(())
                .map(Into::into)
        }
    }

    /// FFI version of `PluralRules`.
    #[diplomat::rust_link(icu::plurals::PluralRules, Struct)]
    #[diplomat::opaque]
    pub struct ICU4XPluralRules(PluralRules);

    impl ICU4XPluralRules {
        /// Construct an [`ICU4XPluralRules`] for the given locale, for cardinal numbers
        #[diplomat::rust_link(icu::plurals::PluralRules::try_new_cardinal_unstable, FnInStruct)]
        #[diplomat::rust_link(icu::plurals::PluralRules::try_new_unstable, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::plurals::PluralRuleType, Enum, hidden)]
        pub fn create_cardinal(
            provider: &ICU4XDataProvider,
            locale: &ICU4XLocale,
        ) -> Result<Box<ICU4XPluralRules>, ICU4XError> {
            let locale = locale.to_datalocale();
            Ok(Box::new(ICU4XPluralRules(
                PluralRules::try_new_cardinal_unstable(&provider.0, &locale)?,
            )))
        }

        /// Construct an [`ICU4XPluralRules`] for the given locale, for ordinal numbers
        #[diplomat::rust_link(icu::plurals::PluralRules::try_new_ordinal_unstable, FnInStruct)]
        #[diplomat::rust_link(icu::plurals::PluralRules::try_new_unstable, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::plurals::PluralRuleType, Enum, hidden)]
        pub fn create_ordinal(
            provider: &ICU4XDataProvider,
            locale: &ICU4XLocale,
        ) -> Result<Box<ICU4XPluralRules>, ICU4XError> {
            let locale = locale.to_datalocale();
            Ok(Box::new(ICU4XPluralRules(
                PluralRules::try_new_ordinal_unstable(&provider.0, &locale)?,
            )))
        }

        /// Get the category for a given number represented as operands
        #[diplomat::rust_link(icu::plurals::PluralRules::category_for, FnInStruct)]
        pub fn category_for(&self, op: &ICU4XPluralOperands) -> ICU4XPluralCategory {
            self.0.category_for(op.0).into()
        }

        /// Get all of the categories needed in the current locale
        #[diplomat::rust_link(icu::plurals::PluralRules::categories, FnInStruct)]
        pub fn categories(&self) -> ICU4XPluralCategories {
            self.0.categories().fold(
                ICU4XPluralCategories {
                    zero: false,
                    one: false,
                    two: false,
                    few: false,
                    many: false,
                    other: false,
                },
                |mut categories, category| {
                    match category {
                        PluralCategory::Zero => categories.zero = true,
                        PluralCategory::One => categories.one = true,
                        PluralCategory::Two => categories.two = true,
                        PluralCategory::Few => categories.few = true,
                        PluralCategory::Many => categories.many = true,
                        PluralCategory::Other => categories.other = true,
                    };
                    categories
                },
            )
        }
    }

    /// FFI version of `PluralOperands`.
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::plurals::PluralOperands, Struct)]
    pub struct ICU4XPluralOperands(pub icu_plurals::PluralOperands);

    impl ICU4XPluralOperands {
        /// Construct for a given string representing a number
        #[diplomat::rust_link(icu::plurals::PluralOperands::from_str, FnInStruct)]
        pub fn create_from_string(s: &str) -> Result<Box<ICU4XPluralOperands>, ICU4XError> {
            let s = s.as_bytes(); // #2520
            Ok(Box::new(ICU4XPluralOperands(PluralOperands::from(
                // XXX should this have its own errors?
                &FixedDecimal::try_from(s).map_err(|_| ICU4XError::PluralsParserError)?,
            ))))
        }
    }

    /// FFI version of `PluralRules::categories()` data.
    pub struct ICU4XPluralCategories {
        pub zero: bool,
        pub one: bool,
        pub two: bool,
        pub few: bool,
        pub many: bool,
        pub other: bool,
    }
}
