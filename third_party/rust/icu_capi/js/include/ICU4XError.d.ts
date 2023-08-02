
/**

 * A common enum for errors that ICU4X may return, organized by API

 * The error names are stable and can be checked against as strings in the JS API

 * Additional information: {@link https://docs.rs/fixed_decimal/latest/fixed_decimal/enum.Error.html 1}, {@link https://docs.rs/icu/latest/icu/calendar/enum.CalendarError.html 2}, {@link https://docs.rs/icu/latest/icu/collator/enum.CollatorError.html 3}, {@link https://docs.rs/icu/latest/icu/datetime/enum.DateTimeError.html 4}, {@link https://docs.rs/icu/latest/icu/decimal/enum.DecimalError.html 5}, {@link https://docs.rs/icu/latest/icu/list/enum.ListError.html 6}, {@link https://docs.rs/icu/latest/icu/locid/enum.ParserError.html 7}, {@link https://docs.rs/icu/latest/icu/locid_transform/enum.LocaleTransformError.html 8}, {@link https://docs.rs/icu/latest/icu/normalizer/enum.NormalizerError.html 9}, {@link https://docs.rs/icu/latest/icu/plurals/enum.PluralsError.html 10}, {@link https://docs.rs/icu/latest/icu/properties/enum.PropertiesError.html 11}, {@link https://docs.rs/icu/latest/icu/provider/struct.DataError.html 12}, {@link https://docs.rs/icu/latest/icu/provider/enum.DataErrorKind.html 13}, {@link https://docs.rs/icu/latest/icu/segmenter/enum.SegmenterError.html 14}, {@link https://docs.rs/icu/latest/icu/timezone/enum.TimeZoneError.html 15}
 */
export enum ICU4XError {
  /**

   * The error is not currently categorized as ICU4XError. Please file a bug
   */
  UnknownError = 'UnknownError',
  /**

   * An error arising from writing to a string Typically found when not enough space is allocated Most APIs that return a string may return this error
   */
  WriteableError = 'WriteableError',
  /**
   */
  OutOfBoundsError = 'OutOfBoundsError',
  /**
   */
  DataMissingDataKeyError = 'DataMissingDataKeyError',
  /**
   */
  DataMissingVariantError = 'DataMissingVariantError',
  /**
   */
  DataMissingLocaleError = 'DataMissingLocaleError',
  /**
   */
  DataNeedsVariantError = 'DataNeedsVariantError',
  /**
   */
  DataNeedsLocaleError = 'DataNeedsLocaleError',
  /**
   */
  DataExtraneousLocaleError = 'DataExtraneousLocaleError',
  /**
   */
  DataFilteredResourceError = 'DataFilteredResourceError',
  /**
   */
  DataMismatchedTypeError = 'DataMismatchedTypeError',
  /**
   */
  DataMissingPayloadError = 'DataMissingPayloadError',
  /**
   */
  DataInvalidStateError = 'DataInvalidStateError',
  /**
   */
  DataCustomError = 'DataCustomError',
  /**
   */
  DataIoError = 'DataIoError',
  /**
   */
  DataUnavailableBufferFormatError = 'DataUnavailableBufferFormatError',
  /**
   */
  DataMismatchedAnyBufferError = 'DataMismatchedAnyBufferError',
  /**

   * The subtag being requested was not set
   */
  LocaleUndefinedSubtagError = 'LocaleUndefinedSubtagError',
  /**

   * The locale or subtag string failed to parse
   */
  LocaleParserLanguageError = 'LocaleParserLanguageError',
  /**
   */
  LocaleParserSubtagError = 'LocaleParserSubtagError',
  /**
   */
  LocaleParserExtensionError = 'LocaleParserExtensionError',
  /**

   * Attempted to construct an invalid data struct
   */
  DataStructValidityError = 'DataStructValidityError',
  /**
   */
  PropertyUnknownScriptIdError = 'PropertyUnknownScriptIdError',
  /**
   */
  PropertyUnknownGeneralCategoryGroupError = 'PropertyUnknownGeneralCategoryGroupError',
  /**
   */
  PropertyUnexpectedPropertyNameError = 'PropertyUnexpectedPropertyNameError',
  /**
   */
  FixedDecimalLimitError = 'FixedDecimalLimitError',
  /**
   */
  FixedDecimalSyntaxError = 'FixedDecimalSyntaxError',
  /**
   */
  PluralsParserError = 'PluralsParserError',
  /**
   */
  CalendarParseError = 'CalendarParseError',
  /**
   */
  CalendarOverflowError = 'CalendarOverflowError',
  /**
   */
  CalendarUnderflowError = 'CalendarUnderflowError',
  /**
   */
  CalendarOutOfRangeError = 'CalendarOutOfRangeError',
  /**
   */
  CalendarUnknownEraError = 'CalendarUnknownEraError',
  /**
   */
  CalendarUnknownMonthCodeError = 'CalendarUnknownMonthCodeError',
  /**
   */
  CalendarMissingInputError = 'CalendarMissingInputError',
  /**
   */
  CalendarUnknownKindError = 'CalendarUnknownKindError',
  /**
   */
  CalendarMissingError = 'CalendarMissingError',
  /**
   */
  DateTimePatternError = 'DateTimePatternError',
  /**
   */
  DateTimeMissingInputFieldError = 'DateTimeMissingInputFieldError',
  /**
   */
  DateTimeSkeletonError = 'DateTimeSkeletonError',
  /**
   */
  DateTimeUnsupportedFieldError = 'DateTimeUnsupportedFieldError',
  /**
   */
  DateTimeUnsupportedOptionsError = 'DateTimeUnsupportedOptionsError',
  /**
   */
  DateTimeMissingWeekdaySymbolError = 'DateTimeMissingWeekdaySymbolError',
  /**
   */
  DateTimeMissingMonthSymbolError = 'DateTimeMissingMonthSymbolError',
  /**
   */
  DateTimeFixedDecimalError = 'DateTimeFixedDecimalError',
  /**
   */
  DateTimeMismatchedCalendarError = 'DateTimeMismatchedCalendarError',
  /**
   */
  TinyStrTooLargeError = 'TinyStrTooLargeError',
  /**
   */
  TinyStrContainsNullError = 'TinyStrContainsNullError',
  /**
   */
  TinyStrNonAsciiError = 'TinyStrNonAsciiError',
  /**
   */
  TimeZoneOffsetOutOfBoundsError = 'TimeZoneOffsetOutOfBoundsError',
  /**
   */
  TimeZoneInvalidOffsetError = 'TimeZoneInvalidOffsetError',
  /**
   */
  TimeZoneMissingInputError = 'TimeZoneMissingInputError',
  /**
   */
  NormalizerFutureExtensionError = 'NormalizerFutureExtensionError',
  /**
   */
  NormalizerValidationError = 'NormalizerValidationError',
}