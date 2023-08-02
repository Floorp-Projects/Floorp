#ifndef ICU4XError_HPP
#define ICU4XError_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XError.h"



/**
 * A common enum for errors that ICU4X may return, organized by API
 * 
 * The error names are stable and can be checked against as strings in the JS API
 * 
 *  Additional information: [1](https://docs.rs/fixed_decimal/latest/fixed_decimal/enum.Error.html), [2](https://docs.rs/icu/latest/icu/calendar/enum.CalendarError.html), [3](https://docs.rs/icu/latest/icu/collator/enum.CollatorError.html), [4](https://docs.rs/icu/latest/icu/datetime/enum.DateTimeError.html), [5](https://docs.rs/icu/latest/icu/decimal/enum.DecimalError.html), [6](https://docs.rs/icu/latest/icu/list/enum.ListError.html), [7](https://docs.rs/icu/latest/icu/locid/enum.ParserError.html), [8](https://docs.rs/icu/latest/icu/locid_transform/enum.LocaleTransformError.html), [9](https://docs.rs/icu/latest/icu/normalizer/enum.NormalizerError.html), [10](https://docs.rs/icu/latest/icu/plurals/enum.PluralsError.html), [11](https://docs.rs/icu/latest/icu/properties/enum.PropertiesError.html), [12](https://docs.rs/icu/latest/icu/provider/struct.DataError.html), [13](https://docs.rs/icu/latest/icu/provider/enum.DataErrorKind.html), [14](https://docs.rs/icu/latest/icu/segmenter/enum.SegmenterError.html), [15](https://docs.rs/icu/latest/icu/timezone/enum.TimeZoneError.html)
 */
enum struct ICU4XError {

  /**
   * The error is not currently categorized as ICU4XError.
   * Please file a bug
   */
  UnknownError = 0,

  /**
   * An error arising from writing to a string
   * Typically found when not enough space is allocated
   * Most APIs that return a string may return this error
   */
  WriteableError = 1,
  OutOfBoundsError = 2,
  DataMissingDataKeyError = 256,
  DataMissingVariantError = 257,
  DataMissingLocaleError = 258,
  DataNeedsVariantError = 259,
  DataNeedsLocaleError = 260,
  DataExtraneousLocaleError = 261,
  DataFilteredResourceError = 262,
  DataMismatchedTypeError = 263,
  DataMissingPayloadError = 264,
  DataInvalidStateError = 265,
  DataCustomError = 266,
  DataIoError = 267,
  DataUnavailableBufferFormatError = 268,
  DataMismatchedAnyBufferError = 269,

  /**
   * The subtag being requested was not set
   */
  LocaleUndefinedSubtagError = 512,

  /**
   * The locale or subtag string failed to parse
   */
  LocaleParserLanguageError = 513,
  LocaleParserSubtagError = 514,
  LocaleParserExtensionError = 515,

  /**
   * Attempted to construct an invalid data struct
   */
  DataStructValidityError = 768,
  PropertyUnknownScriptIdError = 1024,
  PropertyUnknownGeneralCategoryGroupError = 1025,
  PropertyUnexpectedPropertyNameError = 1026,
  FixedDecimalLimitError = 1280,
  FixedDecimalSyntaxError = 1281,
  PluralsParserError = 1536,
  CalendarParseError = 1792,
  CalendarOverflowError = 1793,
  CalendarUnderflowError = 1794,
  CalendarOutOfRangeError = 1795,
  CalendarUnknownEraError = 1796,
  CalendarUnknownMonthCodeError = 1797,
  CalendarMissingInputError = 1798,
  CalendarUnknownKindError = 1799,
  CalendarMissingError = 1800,
  DateTimePatternError = 2048,
  DateTimeMissingInputFieldError = 2049,
  DateTimeSkeletonError = 2050,
  DateTimeUnsupportedFieldError = 2051,
  DateTimeUnsupportedOptionsError = 2052,
  DateTimeMissingWeekdaySymbolError = 2053,
  DateTimeMissingMonthSymbolError = 2054,
  DateTimeFixedDecimalError = 2055,
  DateTimeMismatchedCalendarError = 2056,
  TinyStrTooLargeError = 2304,
  TinyStrContainsNullError = 2305,
  TinyStrNonAsciiError = 2306,
  TimeZoneOffsetOutOfBoundsError = 2560,
  TimeZoneInvalidOffsetError = 2561,
  TimeZoneMissingInputError = 2562,
  NormalizerFutureExtensionError = 2816,
  NormalizerValidationError = 2817,
};

#endif
