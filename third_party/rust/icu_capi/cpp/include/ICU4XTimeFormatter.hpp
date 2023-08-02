#ifndef ICU4XTimeFormatter_HPP
#define ICU4XTimeFormatter_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XTimeFormatter.h"

class ICU4XDataProvider;
class ICU4XLocale;
#include "ICU4XTimeLength.hpp"
class ICU4XTimeFormatter;
#include "ICU4XError.hpp"
class ICU4XTime;
class ICU4XDateTime;
class ICU4XIsoDateTime;

/**
 * A destruction policy for using ICU4XTimeFormatter with std::unique_ptr.
 */
struct ICU4XTimeFormatterDeleter {
  void operator()(capi::ICU4XTimeFormatter* l) const noexcept {
    capi::ICU4XTimeFormatter_destroy(l);
  }
};

/**
 * An ICU4X TimeFormatter object capable of formatting an [`ICU4XTime`] type (and others) as a string
 * 
 * See the [Rust documentation for `TimeFormatter`](https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html) for more information.
 */
class ICU4XTimeFormatter {
 public:

  /**
   * Creates a new [`ICU4XTimeFormatter`] from locale data.
   * 
   * See the [Rust documentation for `try_new_with_length_unstable`](https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html#method.try_new_with_length_unstable) for more information.
   */
  static diplomat::result<ICU4XTimeFormatter, ICU4XError> create_with_length(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XTimeLength length);

  /**
   * Formats a [`ICU4XTime`] to a string.
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html#method.format) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> format_time_to_writeable(const ICU4XTime& value, W& write) const;

  /**
   * Formats a [`ICU4XTime`] to a string.
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html#method.format) for more information.
   */
  diplomat::result<std::string, ICU4XError> format_time(const ICU4XTime& value) const;

  /**
   * Formats a [`ICU4XDateTime`] to a string.
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html#method.format) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> format_datetime_to_writeable(const ICU4XDateTime& value, W& write) const;

  /**
   * Formats a [`ICU4XDateTime`] to a string.
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html#method.format) for more information.
   */
  diplomat::result<std::string, ICU4XError> format_datetime(const ICU4XDateTime& value) const;

  /**
   * Formats a [`ICU4XIsoDateTime`] to a string.
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html#method.format) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> format_iso_datetime_to_writeable(const ICU4XIsoDateTime& value, W& write) const;

  /**
   * Formats a [`ICU4XIsoDateTime`] to a string.
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/datetime/struct.TimeFormatter.html#method.format) for more information.
   */
  diplomat::result<std::string, ICU4XError> format_iso_datetime(const ICU4XIsoDateTime& value) const;
  inline const capi::ICU4XTimeFormatter* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XTimeFormatter* AsFFIMut() { return this->inner.get(); }
  inline ICU4XTimeFormatter(capi::ICU4XTimeFormatter* i) : inner(i) {}
  ICU4XTimeFormatter() = default;
  ICU4XTimeFormatter(ICU4XTimeFormatter&&) noexcept = default;
  ICU4XTimeFormatter& operator=(ICU4XTimeFormatter&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XTimeFormatter, ICU4XTimeFormatterDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XLocale.hpp"
#include "ICU4XTime.hpp"
#include "ICU4XDateTime.hpp"
#include "ICU4XIsoDateTime.hpp"

inline diplomat::result<ICU4XTimeFormatter, ICU4XError> ICU4XTimeFormatter::create_with_length(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XTimeLength length) {
  auto diplomat_result_raw_out_value = capi::ICU4XTimeFormatter_create_with_length(provider.AsFFI(), locale.AsFFI(), static_cast<capi::ICU4XTimeLength>(length));
  diplomat::result<ICU4XTimeFormatter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XTimeFormatter>(std::move(ICU4XTimeFormatter(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XTimeFormatter::format_time_to_writeable(const ICU4XTime& value, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XTimeFormatter_format_time(this->inner.get(), value.AsFFI(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XTimeFormatter::format_time(const ICU4XTime& value) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XTimeFormatter_format_time(this->inner.get(), value.AsFFI(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XTimeFormatter::format_datetime_to_writeable(const ICU4XDateTime& value, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XTimeFormatter_format_datetime(this->inner.get(), value.AsFFI(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XTimeFormatter::format_datetime(const ICU4XDateTime& value) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XTimeFormatter_format_datetime(this->inner.get(), value.AsFFI(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XTimeFormatter::format_iso_datetime_to_writeable(const ICU4XIsoDateTime& value, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XTimeFormatter_format_iso_datetime(this->inner.get(), value.AsFFI(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XTimeFormatter::format_iso_datetime(const ICU4XIsoDateTime& value) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XTimeFormatter_format_iso_datetime(this->inner.get(), value.AsFFI(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
#endif
