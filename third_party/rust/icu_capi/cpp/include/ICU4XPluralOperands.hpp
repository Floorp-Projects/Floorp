#ifndef ICU4XPluralOperands_HPP
#define ICU4XPluralOperands_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XPluralOperands.h"

class ICU4XPluralOperands;
#include "ICU4XError.hpp"

/**
 * A destruction policy for using ICU4XPluralOperands with std::unique_ptr.
 */
struct ICU4XPluralOperandsDeleter {
  void operator()(capi::ICU4XPluralOperands* l) const noexcept {
    capi::ICU4XPluralOperands_destroy(l);
  }
};

/**
 * FFI version of `PluralOperands`.
 * 
 * See the [Rust documentation for `PluralOperands`](https://docs.rs/icu/latest/icu/plurals/struct.PluralOperands.html) for more information.
 */
class ICU4XPluralOperands {
 public:

  /**
   * Construct for a given string representing a number
   * 
   * See the [Rust documentation for `from_str`](https://docs.rs/icu/latest/icu/plurals/struct.PluralOperands.html#method.from_str) for more information.
   */
  static diplomat::result<ICU4XPluralOperands, ICU4XError> create_from_string(const std::string_view s);
  inline const capi::ICU4XPluralOperands* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XPluralOperands* AsFFIMut() { return this->inner.get(); }
  inline ICU4XPluralOperands(capi::ICU4XPluralOperands* i) : inner(i) {}
  ICU4XPluralOperands() = default;
  ICU4XPluralOperands(ICU4XPluralOperands&&) noexcept = default;
  ICU4XPluralOperands& operator=(ICU4XPluralOperands&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XPluralOperands, ICU4XPluralOperandsDeleter> inner;
};


inline diplomat::result<ICU4XPluralOperands, ICU4XError> ICU4XPluralOperands::create_from_string(const std::string_view s) {
  auto diplomat_result_raw_out_value = capi::ICU4XPluralOperands_create_from_string(s.data(), s.size());
  diplomat::result<ICU4XPluralOperands, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XPluralOperands>(std::move(ICU4XPluralOperands(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
#endif
