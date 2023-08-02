// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use self::ffi::ICU4XError;
use core::fmt;
#[cfg(feature = "icu_decimal")]
use fixed_decimal::Error as FixedDecimalError;
#[cfg(any(
    feature = "icu_datetime",
    feature = "icu_timezone",
    feature = "icu_calendar"
))]
use icu_calendar::CalendarError;
#[cfg(feature = "icu_collator")]
use icu_collator::CollatorError;
#[cfg(feature = "icu_datetime")]
use icu_datetime::DateTimeError;
#[cfg(any(feature = "icu_decimal", feature = "icu_datetime"))]
use icu_decimal::DecimalError;
#[cfg(feature = "icu_list")]
use icu_list::ListError;
use icu_locid::ParserError;
#[cfg(feature = "icu_locid_transform")]
use icu_locid_transform::LocaleTransformError;
#[cfg(feature = "icu_normalizer")]
use icu_normalizer::NormalizerError;
#[cfg(any(feature = "icu_plurals", feature = "icu_datetime"))]
use icu_plurals::PluralsError;
#[cfg(feature = "icu_properties")]
use icu_properties::PropertiesError;
use icu_provider::{DataError, DataErrorKind};
#[cfg(feature = "icu_segmenter")]
use icu_segmenter::SegmenterError;
#[cfg(any(feature = "icu_timezone", feature = "icu_datetime"))]
use icu_timezone::TimeZoneError;
use tinystr::TinyStrError;

#[diplomat::bridge]
pub mod ffi {
    use alloc::boxed::Box;

    #[derive(Debug, Copy, Clone, PartialEq, Eq)]
    #[repr(C)]
    /// A common enum for errors that ICU4X may return, organized by API
    ///
    /// The error names are stable and can be checked against as strings in the JS API
    #[diplomat::rust_link(fixed_decimal::Error, Enum, compact)]
    #[diplomat::rust_link(icu::calendar::CalendarError, Enum, compact)]
    #[diplomat::rust_link(icu::collator::CollatorError, Enum, compact)]
    #[diplomat::rust_link(icu::datetime::DateTimeError, Enum, compact)]
    #[diplomat::rust_link(icu::decimal::DecimalError, Enum, compact)]
    #[diplomat::rust_link(icu::list::ListError, Enum, compact)]
    #[diplomat::rust_link(icu::locid::ParserError, Enum, compact)]
    #[diplomat::rust_link(icu::locid_transform::LocaleTransformError, Enum, compact)]
    #[diplomat::rust_link(icu::normalizer::NormalizerError, Enum, compact)]
    #[diplomat::rust_link(icu::plurals::PluralsError, Enum, compact)]
    #[diplomat::rust_link(icu::properties::PropertiesError, Enum, compact)]
    #[diplomat::rust_link(icu::provider::DataError, Struct, compact)]
    #[diplomat::rust_link(icu::provider::DataErrorKind, Enum, compact)]
    #[diplomat::rust_link(icu::segmenter::SegmenterError, Enum, compact)]
    #[diplomat::rust_link(icu::timezone::TimeZoneError, Enum, compact)]
    pub enum ICU4XError {
        // general errors
        /// The error is not currently categorized as ICU4XError.
        /// Please file a bug
        UnknownError = 0x00,
        /// An error arising from writing to a string
        /// Typically found when not enough space is allocated
        /// Most APIs that return a string may return this error
        WriteableError = 0x01,
        // Some input was out of bounds
        OutOfBoundsError = 0x02,

        // general data errors
        // See DataError
        DataMissingDataKeyError = 0x1_00,
        DataMissingVariantError = 0x1_01,
        DataMissingLocaleError = 0x1_02,
        DataNeedsVariantError = 0x1_03,
        DataNeedsLocaleError = 0x1_04,
        DataExtraneousLocaleError = 0x1_05,
        DataFilteredResourceError = 0x1_06,
        DataMismatchedTypeError = 0x1_07,
        DataMissingPayloadError = 0x1_08,
        DataInvalidStateError = 0x1_09,
        DataCustomError = 0x1_0A,
        DataIoError = 0x1_0B,
        DataUnavailableBufferFormatError = 0x1_0C,
        DataMismatchedAnyBufferError = 0x1_0D,

        // locale errors
        /// The subtag being requested was not set
        LocaleUndefinedSubtagError = 0x2_00,
        /// The locale or subtag string failed to parse
        LocaleParserLanguageError = 0x2_01,
        LocaleParserSubtagError = 0x2_02,
        LocaleParserExtensionError = 0x2_03,

        // data struct errors
        /// Attempted to construct an invalid data struct
        DataStructValidityError = 0x3_00,

        // property errors
        PropertyUnknownScriptIdError = 0x4_00,
        PropertyUnknownGeneralCategoryGroupError = 0x4_01,
        PropertyUnexpectedPropertyNameError = 0x4_02,

        // fixed_decimal errors
        FixedDecimalLimitError = 0x5_00,
        FixedDecimalSyntaxError = 0x5_01,

        // plural errors
        PluralsParserError = 0x6_00,

        // datetime errors
        CalendarParseError = 0x7_00,
        CalendarOverflowError = 0x7_01,
        CalendarUnderflowError = 0x7_02,
        CalendarOutOfRangeError = 0x7_03,
        CalendarUnknownEraError = 0x7_04,
        CalendarUnknownMonthCodeError = 0x7_05,
        CalendarMissingInputError = 0x7_06,
        CalendarUnknownKindError = 0x7_07,
        CalendarMissingError = 0x7_08,

        // datetime format errors
        DateTimePatternError = 0x8_00,
        DateTimeMissingInputFieldError = 0x8_01,
        DateTimeSkeletonError = 0x8_02,
        DateTimeUnsupportedFieldError = 0x8_03,
        DateTimeUnsupportedOptionsError = 0x8_04,
        DateTimeMissingWeekdaySymbolError = 0x8_05,
        DateTimeMissingMonthSymbolError = 0x8_06,
        DateTimeFixedDecimalError = 0x8_07,
        DateTimeMismatchedCalendarError = 0x8_08,

        // tinystr errors
        TinyStrTooLargeError = 0x9_00,
        TinyStrContainsNullError = 0x9_01,
        TinyStrNonAsciiError = 0x9_02,

        // timezone errors
        TimeZoneOffsetOutOfBoundsError = 0xA_00,
        TimeZoneInvalidOffsetError = 0xA_01,
        TimeZoneMissingInputError = 0xA_02,

        // normalizer errors
        NormalizerFutureExtensionError = 0xB_00,
        NormalizerValidationError = 0xB_01,
    }
}

impl ICU4XError {
    #[cfg(feature = "logging")]
    #[inline]
    pub(crate) fn log_original<T: core::fmt::Display + ?Sized>(self, e: &T) -> Self {
        use core::any;
        log::warn!(
            "Returning ICU4XError::{:?} based on original {}: {}",
            self,
            any::type_name::<T>(),
            e
        );
        self
    }

    #[cfg(not(feature = "logging"))]
    #[inline]
    pub(crate) fn log_original<T: core::fmt::Display + ?Sized>(self, _e: &T) -> Self {
        self
    }
}

impl From<fmt::Error> for ICU4XError {
    fn from(e: fmt::Error) -> Self {
        ICU4XError::WriteableError.log_original(&e)
    }
}

impl From<DataError> for ICU4XError {
    fn from(e: DataError) -> Self {
        match e.kind {
            DataErrorKind::MissingDataKey => ICU4XError::DataMissingDataKeyError,
            DataErrorKind::MissingLocale => ICU4XError::DataMissingLocaleError,
            DataErrorKind::NeedsLocale => ICU4XError::DataNeedsLocaleError,
            DataErrorKind::ExtraneousLocale => ICU4XError::DataExtraneousLocaleError,
            DataErrorKind::FilteredResource => ICU4XError::DataFilteredResourceError,
            DataErrorKind::MismatchedType(..) => ICU4XError::DataMismatchedTypeError,
            DataErrorKind::MissingPayload => ICU4XError::DataMissingPayloadError,
            DataErrorKind::InvalidState => ICU4XError::DataInvalidStateError,
            DataErrorKind::Custom => ICU4XError::DataCustomError,
            #[cfg(all(
                feature = "provider_fs",
                not(any(target_arch = "wasm32", target_os = "none"))
            ))]
            DataErrorKind::Io(..) => ICU4XError::DataIoError,
            // datagen only
            // DataErrorKind::MissingSourceData(..) => ..,
            DataErrorKind::UnavailableBufferFormat(..) => {
                ICU4XError::DataUnavailableBufferFormatError
            }
            _ => ICU4XError::UnknownError,
        }
        .log_original(&e)
    }
}

#[cfg(feature = "icu_collator")]
impl From<CollatorError> for ICU4XError {
    fn from(e: CollatorError) -> Self {
        match e {
            CollatorError::NotFound => ICU4XError::DataMissingPayloadError,
            CollatorError::MalformedData => ICU4XError::DataInvalidStateError,
            CollatorError::Data(_) => ICU4XError::DataIoError,
            _ => ICU4XError::DataIoError,
        }
        .log_original(&e)
    }
}

#[cfg(feature = "icu_properties")]
impl From<PropertiesError> for ICU4XError {
    fn from(e: PropertiesError) -> Self {
        match e {
            PropertiesError::PropDataLoad(e) => e.into(),
            PropertiesError::UnknownScriptId(..) => ICU4XError::PropertyUnknownScriptIdError,
            PropertiesError::UnknownGeneralCategoryGroup(..) => {
                ICU4XError::PropertyUnknownGeneralCategoryGroupError
            }
            PropertiesError::UnexpectedPropertyName => {
                ICU4XError::PropertyUnexpectedPropertyNameError
            }
            _ => ICU4XError::UnknownError,
        }
        .log_original(&e)
    }
}

#[cfg(any(
    feature = "icu_datetime",
    feature = "icu_timezone",
    feature = "icu_calendar"
))]
impl From<CalendarError> for ICU4XError {
    fn from(e: CalendarError) -> Self {
        match e {
            CalendarError::Parse => ICU4XError::CalendarParseError,
            CalendarError::Overflow { field: _, max: _ } => ICU4XError::CalendarOverflowError,
            CalendarError::Underflow { field: _, min: _ } => ICU4XError::CalendarUnderflowError,
            CalendarError::OutOfRange => ICU4XError::CalendarOutOfRangeError,
            CalendarError::UnknownEra(..) => ICU4XError::CalendarUnknownEraError,
            CalendarError::UnknownMonthCode(..) => ICU4XError::CalendarUnknownMonthCodeError,
            CalendarError::MissingInput(_) => ICU4XError::CalendarMissingInputError,
            CalendarError::UnknownAnyCalendarKind(_) => ICU4XError::CalendarUnknownKindError,
            CalendarError::MissingCalendar => ICU4XError::CalendarMissingError,
            CalendarError::Data(e) => e.into(),
            _ => ICU4XError::UnknownError,
        }
        .log_original(&e)
    }
}

#[cfg(feature = "icu_datetime")]
impl From<DateTimeError> for ICU4XError {
    fn from(e: DateTimeError) -> Self {
        match e {
            DateTimeError::Pattern(_) => ICU4XError::DateTimePatternError,
            DateTimeError::Format(err) => err.into(),
            DateTimeError::Data(err) => err.into(),
            DateTimeError::MissingInputField(_) => ICU4XError::DateTimeMissingInputFieldError,
            // TODO(#1324): Add back skeleton errors
            // DateTimeFormatterError::Skeleton(_) => ICU4XError::DateTimeFormatSkeletonError,
            DateTimeError::UnsupportedField(_) => ICU4XError::DateTimeUnsupportedFieldError,
            DateTimeError::UnsupportedOptions => ICU4XError::DateTimeUnsupportedOptionsError,
            DateTimeError::PluralRules(err) => err.into(),
            DateTimeError::DateTimeInput(err) => err.into(),
            DateTimeError::MissingWeekdaySymbol(_) => ICU4XError::DateTimeMissingWeekdaySymbolError,
            DateTimeError::MissingMonthSymbol(_) => ICU4XError::DateTimeMissingMonthSymbolError,
            DateTimeError::FixedDecimal => ICU4XError::DateTimeFixedDecimalError,
            DateTimeError::FixedDecimalFormatter(err) => err.into(),
            DateTimeError::MismatchedAnyCalendar(_, _) => {
                ICU4XError::DateTimeMismatchedCalendarError
            }
            _ => ICU4XError::UnknownError,
        }
        .log_original(&e)
    }
}

#[cfg(feature = "icu_decimal")]
impl From<FixedDecimalError> for ICU4XError {
    fn from(e: FixedDecimalError) -> Self {
        match e {
            FixedDecimalError::Limit => ICU4XError::FixedDecimalLimitError,
            FixedDecimalError::Syntax => ICU4XError::FixedDecimalSyntaxError,
            _ => ICU4XError::UnknownError,
        }
        .log_original(&e)
    }
}

#[cfg(any(feature = "icu_plurals", feature = "icu_datetime"))]
impl From<PluralsError> for ICU4XError {
    fn from(e: PluralsError) -> Self {
        match e {
            PluralsError::Data(e) => e.into(),
            _ => ICU4XError::UnknownError,
        }
        .log_original(&e)
    }
}

#[cfg(any(feature = "icu_decimal", feature = "icu_datetime"))]
impl From<DecimalError> for ICU4XError {
    fn from(e: DecimalError) -> Self {
        match e {
            DecimalError::Data(e) => e.into(),
            _ => ICU4XError::UnknownError,
        }
        .log_original(&e)
    }
}

#[cfg(feature = "icu_locid_transform")]
impl From<LocaleTransformError> for ICU4XError {
    fn from(e: LocaleTransformError) -> Self {
        match e {
            LocaleTransformError::Data(e) => e.into(),
            _ => ICU4XError::UnknownError,
        }
        .log_original(&e)
    }
}

#[cfg(feature = "icu_segmenter")]
impl From<SegmenterError> for ICU4XError {
    fn from(e: SegmenterError) -> Self {
        match e {
            SegmenterError::Data(e) => e.into(),
            _ => ICU4XError::UnknownError,
        }
        .log_original(&e)
    }
}

#[cfg(feature = "icu_list")]
impl From<ListError> for ICU4XError {
    fn from(e: ListError) -> Self {
        match e {
            ListError::Data(e) => e.into(),
            _ => ICU4XError::UnknownError,
        }
        .log_original(&e)
    }
}

impl From<ParserError> for ICU4XError {
    fn from(e: ParserError) -> Self {
        match e {
            ParserError::InvalidLanguage => ICU4XError::LocaleParserLanguageError,
            ParserError::InvalidSubtag => ICU4XError::LocaleParserSubtagError,
            ParserError::InvalidExtension => ICU4XError::LocaleParserExtensionError,
            _ => ICU4XError::UnknownError,
        }
        .log_original(&e)
    }
}

impl From<TinyStrError> for ICU4XError {
    fn from(e: TinyStrError) -> Self {
        match e {
            TinyStrError::TooLarge { .. } => ICU4XError::TinyStrTooLargeError,
            TinyStrError::ContainsNull => ICU4XError::TinyStrContainsNullError,
            TinyStrError::NonAscii => ICU4XError::TinyStrNonAsciiError,
            _ => ICU4XError::UnknownError,
        }
        .log_original(&e)
    }
}

#[cfg(any(feature = "icu_timezone", feature = "icu_datetime"))]
impl From<TimeZoneError> for ICU4XError {
    fn from(e: TimeZoneError) -> Self {
        match e {
            TimeZoneError::OffsetOutOfBounds => ICU4XError::TimeZoneOffsetOutOfBoundsError,
            TimeZoneError::InvalidOffset => ICU4XError::TimeZoneInvalidOffsetError,
            TimeZoneError::Data(err) => err.into(),
            _ => ICU4XError::UnknownError,
        }
        .log_original(&e)
    }
}

#[cfg(feature = "icu_normalizer")]
impl From<NormalizerError> for ICU4XError {
    fn from(e: NormalizerError) -> Self {
        match e {
            NormalizerError::FutureExtension => ICU4XError::NormalizerFutureExtensionError,
            NormalizerError::ValidationError => ICU4XError::NormalizerValidationError,
            NormalizerError::Data(err) => err.into(),
            _ => ICU4XError::UnknownError,
        }
        .log_original(&e)
    }
}
