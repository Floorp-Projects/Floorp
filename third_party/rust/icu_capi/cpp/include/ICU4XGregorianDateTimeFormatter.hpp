#ifndef ICU4XGregorianDateTimeFormatter_HPP
#define ICU4XGregorianDateTimeFormatter_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XGregorianDateTimeFormatter.h"

class ICU4XDataProvider;
class ICU4XLocale;
#include "ICU4XDateLength.hpp"
#include "ICU4XTimeLength.hpp"
class ICU4XGregorianDateTimeFormatter;
#include "ICU4XError.hpp"
class ICU4XIsoDateTime;

/**
 * A destruction policy for using ICU4XGregorianDateTimeFormatter with std::unique_ptr.
 */
struct ICU4XGregorianDateTimeFormatterDeleter {
  void operator()(capi::ICU4XGregorianDateTimeFormatter* l) const noexcept {
    capi::ICU4XGregorianDateTimeFormatter_destroy(l);
  }
};

/**
 * An ICU4X TypedDateTimeFormatter object capable of formatting a [`ICU4XIsoDateTime`] as a string,
 * using the Gregorian Calendar.
 * 
 * See the [Rust documentation for `TypedDateTimeFormatter`](https://docs.rs/icu/latest/icu/datetime/struct.TypedDateTimeFormatter.html) for more information.
 */
class ICU4XGregorianDateTimeFormatter {
 public:

  /**
   * Creates a new [`ICU4XGregorianDateFormatter`] from locale data.
   * 
   * See the [Rust documentation for `try_new_unstable`](https://docs.rs/icu/latest/icu/datetime/struct.TypedDateTimeFormatter.html#method.try_new_unstable) for more information.
   */
  static diplomat::result<ICU4XGregorianDateTimeFormatter, ICU4XError> create_with_lengths(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XDateLength date_length, ICU4XTimeLength time_length);

  /**
   * Formats a [`ICU4XIsoDateTime`] to a string.
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/struct.TypedDateTimeFormatter.html#method.format) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> format_iso_datetime_to_writeable(const ICU4XIsoDateTime& value, W& write) const;

  /**
   * Formats a [`ICU4XIsoDateTime`] to a string.
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/struct.TypedDateTimeFormatter.html#method.format) for more information.
   */
  diplomat::result<std::string, ICU4XError> format_iso_datetime(const ICU4XIsoDateTime& value) const;
  inline const capi::ICU4XGregorianDateTimeFormatter* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XGregorianDateTimeFormatter* AsFFIMut() { return this->inner.get(); }
  inline ICU4XGregorianDateTimeFormatter(capi::ICU4XGregorianDateTimeFormatter* i) : inner(i) {}
  ICU4XGregorianDateTimeFormatter() = default;
  ICU4XGregorianDateTimeFormatter(ICU4XGregorianDateTimeFormatter&&) noexcept = default;
  ICU4XGregorianDateTimeFormatter& operator=(ICU4XGregorianDateTimeFormatter&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XGregorianDateTimeFormatter, ICU4XGregorianDateTimeFormatterDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XLocale.hpp"
#include "ICU4XIsoDateTime.hpp"

inline diplomat::result<ICU4XGregorianDateTimeFormatter, ICU4XError> ICU4XGregorianDateTimeFormatter::create_with_lengths(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XDateLength date_length, ICU4XTimeLength time_length) {
  auto diplomat_result_raw_out_value = capi::ICU4XGregorianDateTimeFormatter_create_with_lengths(provider.AsFFI(), locale.AsFFI(), static_cast<capi::ICU4XDateLength>(date_length), static_cast<capi::ICU4XTimeLength>(time_length));
  diplomat::result<ICU4XGregorianDateTimeFormatter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XGregorianDateTimeFormatter>(std::move(ICU4XGregorianDateTimeFormatter(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XGregorianDateTimeFormatter::format_iso_datetime_to_writeable(const ICU4XIsoDateTime& value, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XGregorianDateTimeFormatter_format_iso_datetime(this->inner.get(), value.AsFFI(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XGregorianDateTimeFormatter::format_iso_datetime(const ICU4XIsoDateTime& value) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XGregorianDateTimeFormatter_format_iso_datetime(this->inner.get(), value.AsFFI(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
#endif
