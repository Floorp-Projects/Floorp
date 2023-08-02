#ifndef ICU4XDecomposingNormalizer_HPP
#define ICU4XDecomposingNormalizer_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XDecomposingNormalizer.h"

class ICU4XDataProvider;
class ICU4XDecomposingNormalizer;
#include "ICU4XError.hpp"

/**
 * A destruction policy for using ICU4XDecomposingNormalizer with std::unique_ptr.
 */
struct ICU4XDecomposingNormalizerDeleter {
  void operator()(capi::ICU4XDecomposingNormalizer* l) const noexcept {
    capi::ICU4XDecomposingNormalizer_destroy(l);
  }
};

/**
 * 
 * 
 * See the [Rust documentation for `DecomposingNormalizer`](https://docs.rs/icu/latest/icu/normalizer/struct.DecomposingNormalizer.html) for more information.
 */
class ICU4XDecomposingNormalizer {
 public:

  /**
   * Construct a new ICU4XDecomposingNormalizer instance for NFC
   * 
   * See the [Rust documentation for `try_new_nfd_unstable`](https://docs.rs/icu/latest/icu/normalizer/struct.DecomposingNormalizer.html#method.try_new_nfd_unstable) for more information.
   */
  static diplomat::result<ICU4XDecomposingNormalizer, ICU4XError> create_nfd(const ICU4XDataProvider& provider);

  /**
   * Construct a new ICU4XDecomposingNormalizer instance for NFKC
   * 
   * See the [Rust documentation for `try_new_nfkd_unstable`](https://docs.rs/icu/latest/icu/normalizer/struct.DecomposingNormalizer.html#method.try_new_nfkd_unstable) for more information.
   */
  static diplomat::result<ICU4XDecomposingNormalizer, ICU4XError> create_nfkd(const ICU4XDataProvider& provider);

  /**
   * Normalize a (potentially ill-formed) UTF8 string
   * 
   * Errors are mapped to REPLACEMENT CHARACTER
   * 
   * See the [Rust documentation for `normalize_utf8`](https://docs.rs/icu/latest/icu/normalizer/struct.DecomposingNormalizer.html#method.normalize_utf8) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> normalize_to_writeable(const std::string_view s, W& write) const;

  /**
   * Normalize a (potentially ill-formed) UTF8 string
   * 
   * Errors are mapped to REPLACEMENT CHARACTER
   * 
   * See the [Rust documentation for `normalize_utf8`](https://docs.rs/icu/latest/icu/normalizer/struct.DecomposingNormalizer.html#method.normalize_utf8) for more information.
   */
  diplomat::result<std::string, ICU4XError> normalize(const std::string_view s) const;

  /**
   * Check if a (potentially ill-formed) UTF8 string is normalized
   * 
   * Errors are mapped to REPLACEMENT CHARACTER
   * 
   * See the [Rust documentation for `is_normalized_utf8`](https://docs.rs/icu/latest/icu/normalizer/struct.DecomposingNormalizer.html#method.is_normalized_utf8) for more information.
   */
  bool is_normalized(const std::string_view s) const;
  inline const capi::ICU4XDecomposingNormalizer* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XDecomposingNormalizer* AsFFIMut() { return this->inner.get(); }
  inline ICU4XDecomposingNormalizer(capi::ICU4XDecomposingNormalizer* i) : inner(i) {}
  ICU4XDecomposingNormalizer() = default;
  ICU4XDecomposingNormalizer(ICU4XDecomposingNormalizer&&) noexcept = default;
  ICU4XDecomposingNormalizer& operator=(ICU4XDecomposingNormalizer&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XDecomposingNormalizer, ICU4XDecomposingNormalizerDeleter> inner;
};

#include "ICU4XDataProvider.hpp"

inline diplomat::result<ICU4XDecomposingNormalizer, ICU4XError> ICU4XDecomposingNormalizer::create_nfd(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XDecomposingNormalizer_create_nfd(provider.AsFFI());
  diplomat::result<ICU4XDecomposingNormalizer, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XDecomposingNormalizer>(std::move(ICU4XDecomposingNormalizer(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XDecomposingNormalizer, ICU4XError> ICU4XDecomposingNormalizer::create_nfkd(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XDecomposingNormalizer_create_nfkd(provider.AsFFI());
  diplomat::result<ICU4XDecomposingNormalizer, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XDecomposingNormalizer>(std::move(ICU4XDecomposingNormalizer(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XDecomposingNormalizer::normalize_to_writeable(const std::string_view s, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XDecomposingNormalizer_normalize(this->inner.get(), s.data(), s.size(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XDecomposingNormalizer::normalize(const std::string_view s) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XDecomposingNormalizer_normalize(this->inner.get(), s.data(), s.size(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
inline bool ICU4XDecomposingNormalizer::is_normalized(const std::string_view s) const {
  return capi::ICU4XDecomposingNormalizer_is_normalized(this->inner.get(), s.data(), s.size());
}
#endif
