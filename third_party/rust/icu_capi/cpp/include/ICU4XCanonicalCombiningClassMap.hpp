#ifndef ICU4XCanonicalCombiningClassMap_HPP
#define ICU4XCanonicalCombiningClassMap_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XCanonicalCombiningClassMap.h"

class ICU4XDataProvider;
class ICU4XCanonicalCombiningClassMap;
#include "ICU4XError.hpp"

/**
 * A destruction policy for using ICU4XCanonicalCombiningClassMap with std::unique_ptr.
 */
struct ICU4XCanonicalCombiningClassMapDeleter {
  void operator()(capi::ICU4XCanonicalCombiningClassMap* l) const noexcept {
    capi::ICU4XCanonicalCombiningClassMap_destroy(l);
  }
};

/**
 * Lookup of the Canonical_Combining_Class Unicode property
 * 
 * See the [Rust documentation for `CanonicalCombiningClassMap`](https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalCombiningClassMap.html) for more information.
 */
class ICU4XCanonicalCombiningClassMap {
 public:

  /**
   * Construct a new ICU4XCanonicalCombiningClassMap instance for NFC
   * 
   * See the [Rust documentation for `try_new_unstable`](https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalCombiningClassMap.html#method.try_new_unstable) for more information.
   */
  static diplomat::result<ICU4XCanonicalCombiningClassMap, ICU4XError> create(const ICU4XDataProvider& provider);

  /**
   * 
   * 
   * See the [Rust documentation for `get`](https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalCombiningClassMap.html#method.get) for more information.
   * 
   *  Additional information: [1](https://docs.rs/icu/latest/icu/properties/properties/struct.CanonicalCombiningClass.html)
   */
  uint8_t get(char32_t ch) const;

  /**
   * 
   * 
   * See the [Rust documentation for `get32`](https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalCombiningClassMap.html#method.get32) for more information.
   * 
   *  Additional information: [1](https://docs.rs/icu/latest/icu/properties/properties/struct.CanonicalCombiningClass.html)
   */
  uint8_t get32(uint32_t ch) const;
  inline const capi::ICU4XCanonicalCombiningClassMap* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XCanonicalCombiningClassMap* AsFFIMut() { return this->inner.get(); }
  inline ICU4XCanonicalCombiningClassMap(capi::ICU4XCanonicalCombiningClassMap* i) : inner(i) {}
  ICU4XCanonicalCombiningClassMap() = default;
  ICU4XCanonicalCombiningClassMap(ICU4XCanonicalCombiningClassMap&&) noexcept = default;
  ICU4XCanonicalCombiningClassMap& operator=(ICU4XCanonicalCombiningClassMap&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XCanonicalCombiningClassMap, ICU4XCanonicalCombiningClassMapDeleter> inner;
};

#include "ICU4XDataProvider.hpp"

inline diplomat::result<ICU4XCanonicalCombiningClassMap, ICU4XError> ICU4XCanonicalCombiningClassMap::create(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XCanonicalCombiningClassMap_create(provider.AsFFI());
  diplomat::result<ICU4XCanonicalCombiningClassMap, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XCanonicalCombiningClassMap>(std::move(ICU4XCanonicalCombiningClassMap(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline uint8_t ICU4XCanonicalCombiningClassMap::get(char32_t ch) const {
  return capi::ICU4XCanonicalCombiningClassMap_get(this->inner.get(), ch);
}
inline uint8_t ICU4XCanonicalCombiningClassMap::get32(uint32_t ch) const {
  return capi::ICU4XCanonicalCombiningClassMap_get32(this->inner.get(), ch);
}
#endif
