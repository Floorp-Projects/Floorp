#ifndef ICU4XLocaleFallbacker_HPP
#define ICU4XLocaleFallbacker_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XLocaleFallbacker.h"

class ICU4XDataProvider;
class ICU4XLocaleFallbacker;
#include "ICU4XError.hpp"
struct ICU4XLocaleFallbackConfig;
class ICU4XLocaleFallbackerWithConfig;

/**
 * A destruction policy for using ICU4XLocaleFallbacker with std::unique_ptr.
 */
struct ICU4XLocaleFallbackerDeleter {
  void operator()(capi::ICU4XLocaleFallbacker* l) const noexcept {
    capi::ICU4XLocaleFallbacker_destroy(l);
  }
};

/**
 * An object that runs the ICU4X locale fallback algorithm.
 * 
 * See the [Rust documentation for `LocaleFallbacker`](https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fallback/struct.LocaleFallbacker.html) for more information.
 */
class ICU4XLocaleFallbacker {
 public:

  /**
   * Creates a new `ICU4XLocaleFallbacker` from a data provider.
   * 
   * See the [Rust documentation for `try_new_unstable`](https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fallback/struct.LocaleFallbacker.html#method.try_new_unstable) for more information.
   */
  static diplomat::result<ICU4XLocaleFallbacker, ICU4XError> create(const ICU4XDataProvider& provider);

  /**
   * Creates a new `ICU4XLocaleFallbacker` without data for limited functionality.
   * 
   * See the [Rust documentation for `new_without_data`](https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fallback/struct.LocaleFallbacker.html#method.new_without_data) for more information.
   */
  static ICU4XLocaleFallbacker create_without_data();

  /**
   * Associates this `ICU4XLocaleFallbacker` with configuration options.
   * 
   * See the [Rust documentation for `for_config`](https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fallback/struct.LocaleFallbacker.html#method.for_config) for more information.
   * 
   * Lifetimes: `this` must live at least as long as the output.
   */
  diplomat::result<ICU4XLocaleFallbackerWithConfig, ICU4XError> for_config(ICU4XLocaleFallbackConfig config) const;
  inline const capi::ICU4XLocaleFallbacker* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XLocaleFallbacker* AsFFIMut() { return this->inner.get(); }
  inline ICU4XLocaleFallbacker(capi::ICU4XLocaleFallbacker* i) : inner(i) {}
  ICU4XLocaleFallbacker() = default;
  ICU4XLocaleFallbacker(ICU4XLocaleFallbacker&&) noexcept = default;
  ICU4XLocaleFallbacker& operator=(ICU4XLocaleFallbacker&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XLocaleFallbacker, ICU4XLocaleFallbackerDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XLocaleFallbackConfig.hpp"
#include "ICU4XLocaleFallbackerWithConfig.hpp"

inline diplomat::result<ICU4XLocaleFallbacker, ICU4XError> ICU4XLocaleFallbacker::create(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XLocaleFallbacker_create(provider.AsFFI());
  diplomat::result<ICU4XLocaleFallbacker, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XLocaleFallbacker>(std::move(ICU4XLocaleFallbacker(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline ICU4XLocaleFallbacker ICU4XLocaleFallbacker::create_without_data() {
  return ICU4XLocaleFallbacker(capi::ICU4XLocaleFallbacker_create_without_data());
}
inline diplomat::result<ICU4XLocaleFallbackerWithConfig, ICU4XError> ICU4XLocaleFallbacker::for_config(ICU4XLocaleFallbackConfig config) const {
  ICU4XLocaleFallbackConfig diplomat_wrapped_struct_config = config;
  auto diplomat_result_raw_out_value = capi::ICU4XLocaleFallbacker_for_config(this->inner.get(), capi::ICU4XLocaleFallbackConfig{ .priority = static_cast<capi::ICU4XLocaleFallbackPriority>(diplomat_wrapped_struct_config.priority), .extension_key = { diplomat_wrapped_struct_config.extension_key.data(), diplomat_wrapped_struct_config.extension_key.size() } });
  diplomat::result<ICU4XLocaleFallbackerWithConfig, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XLocaleFallbackerWithConfig>(std::move(ICU4XLocaleFallbackerWithConfig(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
#endif
