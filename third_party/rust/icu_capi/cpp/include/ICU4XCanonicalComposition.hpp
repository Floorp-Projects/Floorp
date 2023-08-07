#ifndef ICU4XCanonicalComposition_HPP
#define ICU4XCanonicalComposition_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XCanonicalComposition.h"

class ICU4XDataProvider;
class ICU4XCanonicalComposition;
#include "ICU4XError.hpp"

/**
 * A destruction policy for using ICU4XCanonicalComposition with std::unique_ptr.
 */
struct ICU4XCanonicalCompositionDeleter {
  void operator()(capi::ICU4XCanonicalComposition* l) const noexcept {
    capi::ICU4XCanonicalComposition_destroy(l);
  }
};

/**
 * The raw canonical composition operation.
 * 
 * Callers should generally use ICU4XComposingNormalizer unless they specifically need raw composition operations
 * 
 * See the [Rust documentation for `CanonicalComposition`](https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalComposition.html) for more information.
 */
class ICU4XCanonicalComposition {
 public:

  /**
   * Construct a new ICU4XCanonicalComposition instance for NFC
   * 
   * See the [Rust documentation for `try_new_unstable`](https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalComposition.html#method.try_new_unstable) for more information.
   */
  static diplomat::result<ICU4XCanonicalComposition, ICU4XError> create(const ICU4XDataProvider& provider);

  /**
   * Performs canonical composition (including Hangul) on a pair of characters
   * or returns NUL if these characters don’t compose. Composition exclusions are taken into account.
   * 
   * See the [Rust documentation for `compose`](https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalComposition.html#method.compose) for more information.
   */
  char32_t compose(char32_t starter, char32_t second) const;
  inline const capi::ICU4XCanonicalComposition* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XCanonicalComposition* AsFFIMut() { return this->inner.get(); }
  inline ICU4XCanonicalComposition(capi::ICU4XCanonicalComposition* i) : inner(i) {}
  ICU4XCanonicalComposition() = default;
  ICU4XCanonicalComposition(ICU4XCanonicalComposition&&) noexcept = default;
  ICU4XCanonicalComposition& operator=(ICU4XCanonicalComposition&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XCanonicalComposition, ICU4XCanonicalCompositionDeleter> inner;
};

#include "ICU4XDataProvider.hpp"

inline diplomat::result<ICU4XCanonicalComposition, ICU4XError> ICU4XCanonicalComposition::create(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XCanonicalComposition_create(provider.AsFFI());
  diplomat::result<ICU4XCanonicalComposition, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XCanonicalComposition>(std::move(ICU4XCanonicalComposition(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline char32_t ICU4XCanonicalComposition::compose(char32_t starter, char32_t second) const {
  return capi::ICU4XCanonicalComposition_compose(this->inner.get(), starter, second);
}
#endif
