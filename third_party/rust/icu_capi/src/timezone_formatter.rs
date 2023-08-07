// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_datetime::time_zone::{FallbackFormat, TimeZoneFormatterOptions};

#[diplomat::bridge]
pub mod ffi {
    use crate::errors::ffi::ICU4XError;
    use crate::locale::ffi::ICU4XLocale;
    use crate::provider::ffi::ICU4XDataProvider;
    use crate::timezone::ffi::ICU4XCustomTimeZone;
    use alloc::boxed::Box;
    use icu_datetime::time_zone::FallbackFormat;
    use icu_datetime::time_zone::IsoFormat;
    use icu_datetime::time_zone::IsoMinutes;
    use icu_datetime::time_zone::IsoSeconds;
    use icu_datetime::time_zone::TimeZoneFormatter;
    use writeable::Writeable;

    #[diplomat::opaque]
    /// An ICU4X TimeZoneFormatter object capable of formatting an [`ICU4XCustomTimeZone`] type (and others) as a string
    #[diplomat::rust_link(icu::datetime::time_zone::TimeZoneFormatter, Struct)]
    pub struct ICU4XTimeZoneFormatter(pub TimeZoneFormatter);

    #[diplomat::enum_convert(IsoFormat, needs_wildcard)]
    #[diplomat::rust_link(icu::datetime::time_zone::IsoFormat, Enum)]
    pub enum ICU4XIsoTimeZoneFormat {
        Basic,
        Extended,
        UtcBasic,
        UtcExtended,
    }

    #[diplomat::enum_convert(IsoMinutes, needs_wildcard)]
    #[diplomat::rust_link(icu::datetime::time_zone::IsoMinutes, Enum)]
    pub enum ICU4XIsoTimeZoneMinuteDisplay {
        Required,
        Optional,
    }

    #[diplomat::enum_convert(IsoSeconds, needs_wildcard)]
    #[diplomat::rust_link(icu::datetime::time_zone::IsoSeconds, Enum)]
    pub enum ICU4XIsoTimeZoneSecondDisplay {
        Optional,
        Never,
    }

    pub struct ICU4XIsoTimeZoneOptions {
        pub format: ICU4XIsoTimeZoneFormat,
        pub minutes: ICU4XIsoTimeZoneMinuteDisplay,
        pub seconds: ICU4XIsoTimeZoneSecondDisplay,
    }

    impl ICU4XTimeZoneFormatter {
        /// Creates a new [`ICU4XTimeZoneFormatter`] from locale data.
        ///
        /// Uses localized GMT as the fallback format.
        #[diplomat::rust_link(
            icu::datetime::time_zone::TimeZoneFormatter::try_new_unstable,
            FnInStruct
        )]
        #[diplomat::rust_link(icu::datetime::time_zone::FallbackFormat, Enum, compact)]
        #[diplomat::rust_link(icu::datetime::time_zone::TimeZoneFormatterOptions, Struct, hidden)]
        pub fn create_with_localized_gmt_fallback(
            provider: &ICU4XDataProvider,
            locale: &ICU4XLocale,
        ) -> Result<Box<ICU4XTimeZoneFormatter>, ICU4XError> {
            let locale = locale.to_datalocale();

            Ok(Box::new(ICU4XTimeZoneFormatter(
                TimeZoneFormatter::try_new_unstable(
                    &provider.0,
                    &locale,
                    FallbackFormat::LocalizedGmt.into(),
                )?,
            )))
        }

        /// Creates a new [`ICU4XTimeZoneFormatter`] from locale data.
        ///
        /// Uses ISO-8601 as the fallback format.
        #[diplomat::rust_link(
            icu::datetime::time_zone::TimeZoneFormatter::try_new_unstable,
            FnInStruct
        )]
        #[diplomat::rust_link(icu::datetime::time_zone::FallbackFormat, Enum, compact)]
        #[diplomat::rust_link(icu::datetime::time_zone::TimeZoneFormatterOptions, Struct, hidden)]
        pub fn create_with_iso_8601_fallback(
            provider: &ICU4XDataProvider,
            locale: &ICU4XLocale,
            options: ICU4XIsoTimeZoneOptions,
        ) -> Result<Box<ICU4XTimeZoneFormatter>, ICU4XError> {
            let locale = locale.to_datalocale();

            Ok(Box::new(ICU4XTimeZoneFormatter(
                TimeZoneFormatter::try_new_unstable(&provider.0, &locale, options.into())?,
            )))
        }

        /// Loads generic non-location long format. Example: "Pacific Time"
        #[diplomat::rust_link(
            icu::datetime::time_zone::TimeZoneFormatter::load_generic_non_location_long,
            FnInStruct
        )]
        pub fn load_generic_non_location_long(
            &mut self,
            provider: &ICU4XDataProvider,
        ) -> Result<(), ICU4XError> {
            self.0.load_generic_non_location_long(&provider.0)?;
            Ok(())
        }

        /// Loads generic non-location short format. Example: "PT"
        #[diplomat::rust_link(
            icu::datetime::time_zone::TimeZoneFormatter::load_generic_non_location_short,
            FnInStruct
        )]
        pub fn load_generic_non_location_short(
            &mut self,
            provider: &ICU4XDataProvider,
        ) -> Result<(), ICU4XError> {
            self.0.load_generic_non_location_short(&provider.0)?;
            Ok(())
        }

        /// Loads specific non-location long format. Example: "Pacific Standard Time"
        #[diplomat::rust_link(
            icu::datetime::time_zone::TimeZoneFormatter::load_specific_non_location_long,
            FnInStruct
        )]
        pub fn load_specific_non_location_long(
            &mut self,
            provider: &ICU4XDataProvider,
        ) -> Result<(), ICU4XError> {
            self.0.load_specific_non_location_long(&provider.0)?;
            Ok(())
        }

        /// Loads specific non-location short format. Example: "PST"
        #[diplomat::rust_link(
            icu::datetime::time_zone::TimeZoneFormatter::load_specific_non_location_short,
            FnInStruct
        )]
        pub fn load_specific_non_location_short(
            &mut self,
            provider: &ICU4XDataProvider,
        ) -> Result<(), ICU4XError> {
            self.0.load_specific_non_location_short(&provider.0)?;
            Ok(())
        }

        /// Loads generic location format. Example: "Los Angeles Time"
        #[diplomat::rust_link(
            icu::datetime::time_zone::TimeZoneFormatter::load_generic_location_format,
            FnInStruct
        )]
        pub fn load_generic_location_format(
            &mut self,
            provider: &ICU4XDataProvider,
        ) -> Result<(), ICU4XError> {
            self.0.load_generic_location_format(&provider.0)?;
            Ok(())
        }

        /// Loads localized GMT format. Example: "GMT-07:00"
        #[diplomat::rust_link(
            icu::datetime::time_zone::TimeZoneFormatter::load_localized_gmt_format,
            FnInStruct
        )]
        pub fn load_localized_gmt_format(&mut self) -> Result<(), ICU4XError> {
            self.0.load_localized_gmt_format()?;
            Ok(())
        }

        /// Loads ISO-8601 format. Example: "-07:00"
        #[diplomat::rust_link(
            icu::datetime::time_zone::TimeZoneFormatter::load_iso_8601_format,
            FnInStruct
        )]
        pub fn load_iso_8601_format(
            &mut self,
            options: ICU4XIsoTimeZoneOptions,
        ) -> Result<(), ICU4XError> {
            self.0.load_iso_8601_format(
                options.format.into(),
                options.minutes.into(),
                options.seconds.into(),
            )?;
            Ok(())
        }

        /// Formats a [`ICU4XCustomTimeZone`] to a string.
        #[diplomat::rust_link(icu::datetime::time_zone::TimeZoneFormatter::format, FnInStruct)]
        #[diplomat::rust_link(
            icu::datetime::time_zone::TimeZoneFormatter::format_to_string,
            FnInStruct
        )]
        pub fn format_custom_time_zone(
            &self,
            value: &ICU4XCustomTimeZone,
            write: &mut diplomat_runtime::DiplomatWriteable,
        ) -> Result<(), ICU4XError> {
            self.0.format(&value.0).write_to(write)?;
            Ok(())
        }
    }
}

impl From<ffi::ICU4XIsoTimeZoneOptions> for TimeZoneFormatterOptions {
    fn from(other: ffi::ICU4XIsoTimeZoneOptions) -> Self {
        FallbackFormat::Iso8601(
            other.format.into(),
            other.minutes.into(),
            other.seconds.into(),
        )
        .into()
    }
}
