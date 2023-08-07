#ifndef ICU4XLocaleDisplayNamesFormatter_HPP
#define ICU4XLocaleDisplayNamesFormatter_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XLocaleDisplayNamesFormatter.h"

class ICU4XDataProvider;
class ICU4XLocale;
struct ICU4XDisplayNamesOptionsV1;
class ICU4XLocaleDisplayNamesFormatter;
#include "ICU4XError.hpp"

/**
 * A destruction policy for using ICU4XLocaleDisplayNamesFormatter with std::unique_ptr.
 */
struct ICU4XLocaleDisplayNamesFormatterDeleter {
  void operator()(capi::ICU4XLocaleDisplayNamesFormatter* l) const noexcept {
    capi::ICU4XLocaleDisplayNamesFormatter_destroy(l);
  }
};

/**
 * 
 * 
 * See the [Rust documentation for `LocaleDisplayNamesFormatter`](https://docs.rs/icu/latest/icu/displaynames/struct.LocaleDisplayNamesFormatter.html) for more information.
 */
class ICU4XLocaleDisplayNamesFormatter {
 public:

  /**
   * Creates a new `LocaleDisplayNamesFormatter` from locale data and an options bag.
   * 
   * See the [Rust documentation for `try_new_unstable`](https://docs.rs/icu/latest/icu/displaynames/struct.LocaleDisplayNamesFormatter.html#method.try_new_unstable) for more information.
   */
  static diplomat::result<ICU4XLocaleDisplayNamesFormatter, ICU4XError> try_new_unstable(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XDisplayNamesOptionsV1 options);

  /**
   * Returns the locale-specific display name of a locale.
   * 
   * See the [Rust documentation for `of`](https://docs.rs/icu/latest/icu/displaynames/struct.LocaleDisplayNamesFormatter.html#method.of) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> of_to_writeable(const ICU4XLocale& locale, W& write) const;

  /**
   * Returns the locale-specific display name of a locale.
   * 
   * See the [Rust documentation for `of`](https://docs.rs/icu/latest/icu/displaynames/struct.LocaleDisplayNamesFormatter.html#method.of) for more information.
   */
  diplomat::result<std::string, ICU4XError> of(const ICU4XLocale& locale) const;
  inline const capi::ICU4XLocaleDisplayNamesFormatter* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XLocaleDisplayNamesFormatter* AsFFIMut() { return this->inner.get(); }
  inline ICU4XLocaleDisplayNamesFormatter(capi::ICU4XLocaleDisplayNamesFormatter* i) : inner(i) {}
  ICU4XLocaleDisplayNamesFormatter() = default;
  ICU4XLocaleDisplayNamesFormatter(ICU4XLocaleDisplayNamesFormatter&&) noexcept = default;
  ICU4XLocaleDisplayNamesFormatter& operator=(ICU4XLocaleDisplayNamesFormatter&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XLocaleDisplayNamesFormatter, ICU4XLocaleDisplayNamesFormatterDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XLocale.hpp"
#include "ICU4XDisplayNamesOptionsV1.hpp"

inline diplomat::result<ICU4XLocaleDisplayNamesFormatter, ICU4XError> ICU4XLocaleDisplayNamesFormatter::try_new_unstable(const ICU4XDataProvider& provider, const ICU4XLocale& locale, ICU4XDisplayNamesOptionsV1 options) {
  ICU4XDisplayNamesOptionsV1 diplomat_wrapped_struct_options = options;
  auto diplomat_result_raw_out_value = capi::ICU4XLocaleDisplayNamesFormatter_try_new_unstable(provider.AsFFI(), locale.AsFFI(), capi::ICU4XDisplayNamesOptionsV1{ .style = static_cast<capi::ICU4XDisplayNamesStyle>(diplomat_wrapped_struct_options.style), .fallback = static_cast<capi::ICU4XDisplayNamesFallback>(diplomat_wrapped_struct_options.fallback), .language_display = static_cast<capi::ICU4XLanguageDisplay>(diplomat_wrapped_struct_options.language_display) });
  diplomat::result<ICU4XLocaleDisplayNamesFormatter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XLocaleDisplayNamesFormatter>(std::move(ICU4XLocaleDisplayNamesFormatter(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XLocaleDisplayNamesFormatter::of_to_writeable(const ICU4XLocale& locale, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XLocaleDisplayNamesFormatter_of(this->inner.get(), locale.AsFFI(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XLocaleDisplayNamesFormatter::of(const ICU4XLocale& locale) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XLocaleDisplayNamesFormatter_of(this->inner.get(), locale.AsFFI(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
#endif
