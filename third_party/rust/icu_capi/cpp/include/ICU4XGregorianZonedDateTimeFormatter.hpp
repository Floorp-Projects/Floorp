#ifndef ICU4XGregorianZonedDateTimeFormatter_HPP
#define ICU4XGregorianZonedDateTimeFormatter_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XGregorianZonedDateTimeFormatter.h"

class ICU4XDataProvider;
class ICU4XLocale;
#include "ICU4XDateLength.hpp"
#include "ICU4XTimeLength.hpp"
class ICU4XGregorianZonedDateTimeFormatter;
#include "ICU4XError.hpp"
struct ICU4XIsoTimeZoneOptions;
class ICU4XIsoDateTime;
class ICU4XCustomTimeZone;

/**
 * A destruction policy for using ICU4XGregorianZonedDateTimeFormatter with std::unique_ptr.
 */
struct ICU4XGregorianZonedDateTimeFormatterDeleter {
  void operator()(capi::ICU4XGregorianZonedDateTimeFormatter* l) const noexcept {
    capi::ICU4XGregorianZonedDateTimeFormatter_destroy(l);
  }
};

/**
 * An object capable of formatting a date time with time zone to a string.
 * 
 * See the [Rust documentation for `TypedZonedDateTimeFormatter`](https://docs.rs/icu/latest/icu/datetime/struct.TypedZonedDateTimeFormatter.html) for more information.
 */
class ICU4XGregorianZonedDateTimeFormatter {
 public:

  /**
   * Creates a new [`ICU4XGregorianZonedDateTimeFormatter`] from locale data.
   * 
   * This function has `date_length` and `time_length` arguments and uses default options
   * for the time zone.
   * 
   * See the [Rust documentation for `try_new_unstable`](https://docs.rs/icu/latest/icu/datetime/struct.TypedZonedDateTimeFormatter.html#method.try_new_unstable) for more information.
   */
  static diplomat::result<ICU4XGregorianZonedDateTimeFormatter, ICU4XError> create_with_lengths(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XDateLength date_length, ICU4XTimeLength time_length);

  /**
   * Creates a new [`ICU4XGregorianZonedDateTimeFormatter`] from locale data.
   * 
   * This function has `date_length` and `time_length` arguments and uses an ISO-8601 style
   * fallback for the time zone with the given configurations.
   * 
   * See the [Rust documentation for `try_new_unstable`](https://docs.rs/icu/latest/icu/datetime/struct.TypedZonedDateTimeFormatter.html#method.try_new_unstable) for more information.
   */
  static diplomat::result<ICU4XGregorianZonedDateTimeFormatter, ICU4XError> create_with_lengths_and_iso_8601_time_zone_fallback(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XDateLength date_length, ICU4XTimeLength time_length, ICU4XIsoTimeZoneOptions zone_options);

  /**
   * Formats a [`ICU4XIsoDateTime`] and [`ICU4XCustomTimeZone`] to a string.
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/struct.TypedZonedDateTimeFormatter.html#method.format) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> format_iso_datetime_with_custom_time_zone_to_writeable(const ICU4XIsoDateTime& datetime, const ICU4XCustomTimeZone& time_zone, W& write) const;

  /**
   * Formats a [`ICU4XIsoDateTime`] and [`ICU4XCustomTimeZone`] to a string.
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/struct.TypedZonedDateTimeFormatter.html#method.format) for more information.
   */
  diplomat::result<std::string, ICU4XError> format_iso_datetime_with_custom_time_zone(const ICU4XIsoDateTime& datetime, const ICU4XCustomTimeZone& time_zone) const;
  inline const capi::ICU4XGregorianZonedDateTimeFormatter* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XGregorianZonedDateTimeFormatter* AsFFIMut() { return this->inner.get(); }
  inline ICU4XGregorianZonedDateTimeFormatter(capi::ICU4XGregorianZonedDateTimeFormatter* i) : inner(i) {}
  ICU4XGregorianZonedDateTimeFormatter() = default;
  ICU4XGregorianZonedDateTimeFormatter(ICU4XGregorianZonedDateTimeFormatter&&) noexcept = default;
  ICU4XGregorianZonedDateTimeFormatter& operator=(ICU4XGregorianZonedDateTimeFormatter&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XGregorianZonedDateTimeFormatter, ICU4XGregorianZonedDateTimeFormatterDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XLocale.hpp"
#include "ICU4XIsoTimeZoneOptions.hpp"
#include "ICU4XIsoDateTime.hpp"
#include "ICU4XCustomTimeZone.hpp"

inline diplomat::result<ICU4XGregorianZonedDateTimeFormatter, ICU4XError> ICU4XGregorianZonedDateTimeFormatter::create_with_lengths(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XDateLength date_length, ICU4XTimeLength time_length) {
  auto diplomat_result_raw_out_value = capi::ICU4XGregorianZonedDateTimeFormatter_create_with_lengths(provider.AsFFI(), locale.AsFFI(), static_cast<capi::ICU4XDateLength>(date_length), static_cast<capi::ICU4XTimeLength>(time_length));
  diplomat::result<ICU4XGregorianZonedDateTimeFormatter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XGregorianZonedDateTimeFormatter>(std::move(ICU4XGregorianZonedDateTimeFormatter(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XGregorianZonedDateTimeFormatter, ICU4XError> ICU4XGregorianZonedDateTimeFormatter::create_with_lengths_and_iso_8601_time_zone_fallback(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XDateLength date_length, ICU4XTimeLength time_length, ICU4XIsoTimeZoneOptions zone_options) {
  ICU4XIsoTimeZoneOptions diplomat_wrapped_struct_zone_options = zone_options;
  auto diplomat_result_raw_out_value = capi::ICU4XGregorianZonedDateTimeFormatter_create_with_lengths_and_iso_8601_time_zone_fallback(provider.AsFFI(), locale.AsFFI(), static_cast<capi::ICU4XDateLength>(date_length), static_cast<capi::ICU4XTimeLength>(time_length), capi::ICU4XIsoTimeZoneOptions{ .format = static_cast<capi::ICU4XIsoTimeZoneFormat>(diplomat_wrapped_struct_zone_options.format), .minutes = static_cast<capi::ICU4XIsoTimeZoneMinuteDisplay>(diplomat_wrapped_struct_zone_options.minutes), .seconds = static_cast<capi::ICU4XIsoTimeZoneSecondDisplay>(diplomat_wrapped_struct_zone_options.seconds) });
  diplomat::result<ICU4XGregorianZonedDateTimeFormatter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XGregorianZonedDateTimeFormatter>(std::move(ICU4XGregorianZonedDateTimeFormatter(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XGregorianZonedDateTimeFormatter::format_iso_datetime_with_custom_time_zone_to_writeable(const ICU4XIsoDateTime& datetime, const ICU4XCustomTimeZone& time_zone, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XGregorianZonedDateTimeFormatter_format_iso_datetime_with_custom_time_zone(this->inner.get(), datetime.AsFFI(), time_zone.AsFFI(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XGregorianZonedDateTimeFormatter::format_iso_datetime_with_custom_time_zone(const ICU4XIsoDateTime& datetime, const ICU4XCustomTimeZone& time_zone) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XGregorianZonedDateTimeFormatter_format_iso_datetime_with_custom_time_zone(this->inner.get(), datetime.AsFFI(), time_zone.AsFFI(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
#endif
