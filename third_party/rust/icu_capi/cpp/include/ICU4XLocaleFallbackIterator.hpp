#ifndef ICU4XLocaleFallbackIterator_HPP
#define ICU4XLocaleFallbackIterator_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XLocaleFallbackIterator.h"

class ICU4XLocale;

/**
 * A destruction policy for using ICU4XLocaleFallbackIterator with std::unique_ptr.
 */
struct ICU4XLocaleFallbackIteratorDeleter {
  void operator()(capi::ICU4XLocaleFallbackIterator* l) const noexcept {
    capi::ICU4XLocaleFallbackIterator_destroy(l);
  }
};

/**
 * An iterator over the locale under fallback.
 * 
 * See the [Rust documentation for `LocaleFallbackIterator`](https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fallback/struct.LocaleFallbackIterator.html) for more information.
 */
class ICU4XLocaleFallbackIterator {
 public:

  /**
   * Gets a snapshot of the current state of the locale.
   * 
   * See the [Rust documentation for `get`](https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fallback/struct.LocaleFallbackIterator.html#method.get) for more information.
   */
  ICU4XLocale get() const;

  /**
   * Performs one step of the fallback algorithm, mutating the locale.
   * 
   * See the [Rust documentation for `step`](https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fallback/struct.LocaleFallbackIterator.html#method.step) for more information.
   */
  void step();
  inline const capi::ICU4XLocaleFallbackIterator* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XLocaleFallbackIterator* AsFFIMut() { return this->inner.get(); }
  inline ICU4XLocaleFallbackIterator(capi::ICU4XLocaleFallbackIterator* i) : inner(i) {}
  ICU4XLocaleFallbackIterator() = default;
  ICU4XLocaleFallbackIterator(ICU4XLocaleFallbackIterator&&) noexcept = default;
  ICU4XLocaleFallbackIterator& operator=(ICU4XLocaleFallbackIterator&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XLocaleFallbackIterator, ICU4XLocaleFallbackIteratorDeleter> inner;
};

#include "ICU4XLocale.hpp"

inline ICU4XLocale ICU4XLocaleFallbackIterator::get() const {
  return ICU4XLocale(capi::ICU4XLocaleFallbackIterator_get(this->inner.get()));
}
inline void ICU4XLocaleFallbackIterator::step() {
  capi::ICU4XLocaleFallbackIterator_step(this->inner.get());
}
#endif
