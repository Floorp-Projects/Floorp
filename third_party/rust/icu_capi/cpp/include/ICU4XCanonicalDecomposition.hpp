#ifndef ICU4XCanonicalDecomposition_HPP
#define ICU4XCanonicalDecomposition_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XCanonicalDecomposition.h"

class ICU4XDataProvider;
class ICU4XCanonicalDecomposition;
#include "ICU4XError.hpp"
struct ICU4XDecomposed;

/**
 * A destruction policy for using ICU4XCanonicalDecomposition with std::unique_ptr.
 */
struct ICU4XCanonicalDecompositionDeleter {
  void operator()(capi::ICU4XCanonicalDecomposition* l) const noexcept {
    capi::ICU4XCanonicalDecomposition_destroy(l);
  }
};

/**
 * The raw (non-recursive) canonical decomposition operation.
 * 
 * Callers should generally use ICU4XDecomposingNormalizer unless they specifically need raw composition operations
 * 
 * See the [Rust documentation for `CanonicalDecomposition`](https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalDecomposition.html) for more information.
 */
class ICU4XCanonicalDecomposition {
 public:

  /**
   * Construct a new ICU4XCanonicalDecomposition instance for NFC
   * 
   * See the [Rust documentation for `try_new_unstable`](https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalDecomposition.html#method.try_new_unstable) for more information.
   */
  static diplomat::result<ICU4XCanonicalDecomposition, ICU4XError> create(const ICU4XDataProvider& provider);

  /**
   * Performs non-recursive canonical decomposition (including for Hangul).
   * 
   * See the [Rust documentation for `decompose`](https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalDecomposition.html#method.decompose) for more information.
   */
  ICU4XDecomposed decompose(char32_t c) const;
  inline const capi::ICU4XCanonicalDecomposition* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XCanonicalDecomposition* AsFFIMut() { return this->inner.get(); }
  inline ICU4XCanonicalDecomposition(capi::ICU4XCanonicalDecomposition* i) : inner(i) {}
  ICU4XCanonicalDecomposition() = default;
  ICU4XCanonicalDecomposition(ICU4XCanonicalDecomposition&&) noexcept = default;
  ICU4XCanonicalDecomposition& operator=(ICU4XCanonicalDecomposition&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XCanonicalDecomposition, ICU4XCanonicalDecompositionDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XDecomposed.hpp"

inline diplomat::result<ICU4XCanonicalDecomposition, ICU4XError> ICU4XCanonicalDecomposition::create(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XCanonicalDecomposition_create(provider.AsFFI());
  diplomat::result<ICU4XCanonicalDecomposition, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XCanonicalDecomposition>(std::move(ICU4XCanonicalDecomposition(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline ICU4XDecomposed ICU4XCanonicalDecomposition::decompose(char32_t c) const {
  capi::ICU4XDecomposed diplomat_raw_struct_out_value = capi::ICU4XCanonicalDecomposition_decompose(this->inner.get(), c);
  return ICU4XDecomposed{ .first = std::move(diplomat_raw_struct_out_value.first), .second = std::move(diplomat_raw_struct_out_value.second) };
}
#endif
