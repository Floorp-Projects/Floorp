#ifndef ICU4XListFormatter_HPP
#define ICU4XListFormatter_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XListFormatter.h"

class ICU4XDataProvider;
class ICU4XLocale;
#include "ICU4XListLength.hpp"
class ICU4XListFormatter;
#include "ICU4XError.hpp"
class ICU4XList;

/**
 * A destruction policy for using ICU4XListFormatter with std::unique_ptr.
 */
struct ICU4XListFormatterDeleter {
  void operator()(capi::ICU4XListFormatter* l) const noexcept {
    capi::ICU4XListFormatter_destroy(l);
  }
};

/**
 * 
 * 
 * See the [Rust documentation for `ListFormatter`](https://docs.rs/icu/latest/icu/list/struct.ListFormatter.html) for more information.
 */
class ICU4XListFormatter {
 public:

  /**
   * Construct a new ICU4XListFormatter instance for And patterns
   * 
   * See the [Rust documentation for `try_new_and_with_length_unstable`](https://docs.rs/icu/latest/icu/normalizer/struct.ListFormatter.html#method.try_new_and_with_length_unstable) for more information.
   */
  static diplomat::result<ICU4XListFormatter, ICU4XError> create_and_with_length(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XListLength length);

  /**
   * Construct a new ICU4XListFormatter instance for And patterns
   * 
   * See the [Rust documentation for `try_new_or_with_length_unstable`](https://docs.rs/icu/latest/icu/normalizer/struct.ListFormatter.html#method.try_new_or_with_length_unstable) for more information.
   */
  static diplomat::result<ICU4XListFormatter, ICU4XError> create_or_with_length(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XListLength length);

  /**
   * Construct a new ICU4XListFormatter instance for And patterns
   * 
   * See the [Rust documentation for `try_new_unit_with_length_unstable`](https://docs.rs/icu/latest/icu/normalizer/struct.ListFormatter.html#method.try_new_unit_with_length_unstable) for more information.
   */
  static diplomat::result<ICU4XListFormatter, ICU4XError> create_unit_with_length(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XListLength length);

  /**
   * 
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/normalizer/struct.ListFormatter.html#method.format) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> format_to_writeable(const ICU4XList& list, W& write) const;

  /**
   * 
   * 
   * See the [Rust documentation for `format`](https://docs.rs/icu/latest/icu/normalizer/struct.ListFormatter.html#method.format) for more information.
   */
  diplomat::result<std::string, ICU4XError> format(const ICU4XList& list) const;
  inline const capi::ICU4XListFormatter* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XListFormatter* AsFFIMut() { return this->inner.get(); }
  inline ICU4XListFormatter(capi::ICU4XListFormatter* i) : inner(i) {}
  ICU4XListFormatter() = default;
  ICU4XListFormatter(ICU4XListFormatter&&) noexcept = default;
  ICU4XListFormatter& operator=(ICU4XListFormatter&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XListFormatter, ICU4XListFormatterDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XLocale.hpp"
#include "ICU4XList.hpp"

inline diplomat::result<ICU4XListFormatter, ICU4XError> ICU4XListFormatter::create_and_with_length(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XListLength length) {
  auto diplomat_result_raw_out_value = capi::ICU4XListFormatter_create_and_with_length(provider.AsFFI(), locale.AsFFI(), static_cast<capi::ICU4XListLength>(length));
  diplomat::result<ICU4XListFormatter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XListFormatter>(std::move(ICU4XListFormatter(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XListFormatter, ICU4XError> ICU4XListFormatter::create_or_with_length(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XListLength length) {
  auto diplomat_result_raw_out_value = capi::ICU4XListFormatter_create_or_with_length(provider.AsFFI(), locale.AsFFI(), static_cast<capi::ICU4XListLength>(length));
  diplomat::result<ICU4XListFormatter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XListFormatter>(std::move(ICU4XListFormatter(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XListFormatter, ICU4XError> ICU4XListFormatter::create_unit_with_length(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XListLength length) {
  auto diplomat_result_raw_out_value = capi::ICU4XListFormatter_create_unit_with_length(provider.AsFFI(), locale.AsFFI(), static_cast<capi::ICU4XListLength>(length));
  diplomat::result<ICU4XListFormatter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XListFormatter>(std::move(ICU4XListFormatter(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XListFormatter::format_to_writeable(const ICU4XList& list, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XListFormatter_format(this->inner.get(), list.AsFFI(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XListFormatter::format(const ICU4XList& list) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XListFormatter_format(this->inner.get(), list.AsFFI(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
#endif
