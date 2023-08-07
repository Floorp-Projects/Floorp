#ifndef ICU4XDataProvider_HPP
#define ICU4XDataProvider_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XDataProvider.h"

class ICU4XDataProvider;
#include "ICU4XError.hpp"
class ICU4XLocaleFallbacker;

/**
 * A destruction policy for using ICU4XDataProvider with std::unique_ptr.
 */
struct ICU4XDataProviderDeleter {
  void operator()(capi::ICU4XDataProvider* l) const noexcept {
    capi::ICU4XDataProvider_destroy(l);
  }
};

/**
 * An ICU4X data provider, capable of loading ICU4X data keys from some source.
 * 
 * See the [Rust documentation for `icu_provider`](https://docs.rs/icu_provider/latest/icu_provider/index.html) for more information.
 */
class ICU4XDataProvider {
 public:

  /**
   * Constructs an `FsDataProvider` and returns it as an [`ICU4XDataProvider`].
   * Requires the `provider_fs` Cargo feature.
   * Not supported in WASM.
   * 
   * See the [Rust documentation for `FsDataProvider`](https://docs.rs/icu_provider_fs/latest/icu_provider_fs/struct.FsDataProvider.html) for more information.
   */
  static diplomat::result<ICU4XDataProvider, ICU4XError> create_fs(const std::string_view path);

  /**
   * Constructs a testdata provider and returns it as an [`ICU4XDataProvider`].
   * Requires the `provider_test` and one of `any_provider` or `buffer_provider` Cargo features.
   * 
   * See the [Rust documentation for `icu_testdata`](https://docs.rs/icu_testdata/latest/icu_testdata/index.html) for more information.
   */
  static ICU4XDataProvider create_test();

  /**
   * Constructs a `BlobDataProvider` and returns it as an [`ICU4XDataProvider`].
   * 
   * See the [Rust documentation for `BlobDataProvider`](https://docs.rs/icu_provider_blob/latest/icu_provider_blob/struct.BlobDataProvider.html) for more information.
   * 
   * Lifetimes: `blob` must live for the duration of the program.
   */
  static diplomat::result<ICU4XDataProvider, ICU4XError> create_from_byte_slice(const diplomat::span<const uint8_t> blob);

  /**
   * Constructs an empty [`ICU4XDataProvider`].
   * 
   * See the [Rust documentation for `EmptyDataProvider`](https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/empty/struct.EmptyDataProvider.html) for more information.
   */
  static ICU4XDataProvider create_empty();

  /**
   * Creates a provider that tries the current provider and then, if the current provider
   * doesn't support the data key, another provider `other`.
   * 
   * This takes ownership of the `other` provider, leaving an empty provider in its place.
   * 
   * The providers must be the same type (Any or Buffer). This condition is satisfied if
   * both providers originate from the same constructor, such as `create_from_byte_slice`
   * or `create_fs`. If the condition is not upheld, a runtime error occurs.
   * 
   * See the [Rust documentation for `ForkByKeyProvider`](https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fork/type.ForkByKeyProvider.html) for more information.
   */
  diplomat::result<std::monostate, ICU4XError> fork_by_key(ICU4XDataProvider& other);

  /**
   * Same as `fork_by_key` but forks by locale instead of key.
   * 
   * See the [Rust documentation for `MissingLocalePredicate`](https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fork/predicates/struct.MissingLocalePredicate.html) for more information.
   */
  diplomat::result<std::monostate, ICU4XError> fork_by_locale(ICU4XDataProvider& other);

  /**
   * Enables locale fallbacking for data requests made to this provider.
   * 
   * Note that the test provider (from `create_test`) already has fallbacking enabled.
   * 
   * See the [Rust documentation for `try_new_unstable`](https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fallback/struct.LocaleFallbackProvider.html#method.try_new_unstable) for more information.
   * 
   *  Additional information: [1](https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fallback/struct.LocaleFallbackProvider.html)
   */
  diplomat::result<std::monostate, ICU4XError> enable_locale_fallback();

  /**
   * 
   * 
   * See the [Rust documentation for `new_with_fallbacker`](https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fallback/struct.LocaleFallbackProvider.html#method.new_with_fallbacker) for more information.
   * 
   *  Additional information: [1](https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fallback/struct.LocaleFallbackProvider.html)
   */
  diplomat::result<std::monostate, ICU4XError> enable_locale_fallback_with(const ICU4XLocaleFallbacker& fallbacker);
  inline const capi::ICU4XDataProvider* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XDataProvider* AsFFIMut() { return this->inner.get(); }
  inline ICU4XDataProvider(capi::ICU4XDataProvider* i) : inner(i) {}
  ICU4XDataProvider() = default;
  ICU4XDataProvider(ICU4XDataProvider&&) noexcept = default;
  ICU4XDataProvider& operator=(ICU4XDataProvider&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XDataProvider, ICU4XDataProviderDeleter> inner;
};

#include "ICU4XLocaleFallbacker.hpp"

inline diplomat::result<ICU4XDataProvider, ICU4XError> ICU4XDataProvider::create_fs(const std::string_view path) {
  auto diplomat_result_raw_out_value = capi::ICU4XDataProvider_create_fs(path.data(), path.size());
  diplomat::result<ICU4XDataProvider, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XDataProvider>(std::move(ICU4XDataProvider(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline ICU4XDataProvider ICU4XDataProvider::create_test() {
  return ICU4XDataProvider(capi::ICU4XDataProvider_create_test());
}
inline diplomat::result<ICU4XDataProvider, ICU4XError> ICU4XDataProvider::create_from_byte_slice(const diplomat::span<const uint8_t> blob) {
  auto diplomat_result_raw_out_value = capi::ICU4XDataProvider_create_from_byte_slice(blob.data(), blob.size());
  diplomat::result<ICU4XDataProvider, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XDataProvider>(std::move(ICU4XDataProvider(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline ICU4XDataProvider ICU4XDataProvider::create_empty() {
  return ICU4XDataProvider(capi::ICU4XDataProvider_create_empty());
}
inline diplomat::result<std::monostate, ICU4XError> ICU4XDataProvider::fork_by_key(ICU4XDataProvider& other) {
  auto diplomat_result_raw_out_value = capi::ICU4XDataProvider_fork_by_key(this->inner.get(), other.AsFFIMut());
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::monostate, ICU4XError> ICU4XDataProvider::fork_by_locale(ICU4XDataProvider& other) {
  auto diplomat_result_raw_out_value = capi::ICU4XDataProvider_fork_by_locale(this->inner.get(), other.AsFFIMut());
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::monostate, ICU4XError> ICU4XDataProvider::enable_locale_fallback() {
  auto diplomat_result_raw_out_value = capi::ICU4XDataProvider_enable_locale_fallback(this->inner.get());
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::monostate, ICU4XError> ICU4XDataProvider::enable_locale_fallback_with(const ICU4XLocaleFallbacker& fallbacker) {
  auto diplomat_result_raw_out_value = capi::ICU4XDataProvider_enable_locale_fallback_with(this->inner.get(), fallbacker.AsFFI());
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
#endif
