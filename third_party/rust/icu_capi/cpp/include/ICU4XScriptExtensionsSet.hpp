#ifndef ICU4XScriptExtensionsSet_HPP
#define ICU4XScriptExtensionsSet_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XScriptExtensionsSet.h"


/**
 * A destruction policy for using ICU4XScriptExtensionsSet with std::unique_ptr.
 */
struct ICU4XScriptExtensionsSetDeleter {
  void operator()(capi::ICU4XScriptExtensionsSet* l) const noexcept {
    capi::ICU4XScriptExtensionsSet_destroy(l);
  }
};

/**
 * An object that represents the Script_Extensions property for a single character
 * 
 * See the [Rust documentation for `ScriptExtensionsSet`](https://docs.rs/icu/latest/icu/properties/script/struct.ScriptExtensionsSet.html) for more information.
 */
class ICU4XScriptExtensionsSet {
 public:

  /**
   * Check if the Script_Extensions property of the given code point covers the given script
   * 
   * See the [Rust documentation for `contains`](https://docs.rs/icu/latest/icu/properties/script/struct.ScriptExtensionsSet.html#method.contains) for more information.
   */
  bool contains(uint16_t script) const;

  /**
   * Get the number of scripts contained in here
   * 
   * See the [Rust documentation for `iter`](https://docs.rs/icu/latest/icu/properties/script/struct.ScriptExtensionsSet.html#method.iter) for more information.
   */
  size_t count() const;

  /**
   * Get script at index, returning an error if out of bounds
   * 
   * See the [Rust documentation for `iter`](https://docs.rs/icu/latest/icu/properties/script/struct.ScriptExtensionsSet.html#method.iter) for more information.
   */
  diplomat::result<uint16_t, std::monostate> script_at(size_t index) const;
  inline const capi::ICU4XScriptExtensionsSet* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XScriptExtensionsSet* AsFFIMut() { return this->inner.get(); }
  inline ICU4XScriptExtensionsSet(capi::ICU4XScriptExtensionsSet* i) : inner(i) {}
  ICU4XScriptExtensionsSet() = default;
  ICU4XScriptExtensionsSet(ICU4XScriptExtensionsSet&&) noexcept = default;
  ICU4XScriptExtensionsSet& operator=(ICU4XScriptExtensionsSet&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XScriptExtensionsSet, ICU4XScriptExtensionsSetDeleter> inner;
};


inline bool ICU4XScriptExtensionsSet::contains(uint16_t script) const {
  return capi::ICU4XScriptExtensionsSet_contains(this->inner.get(), script);
}
inline size_t ICU4XScriptExtensionsSet::count() const {
  return capi::ICU4XScriptExtensionsSet_count(this->inner.get());
}
inline diplomat::result<uint16_t, std::monostate> ICU4XScriptExtensionsSet::script_at(size_t index) const {
  auto diplomat_result_raw_out_value = capi::ICU4XScriptExtensionsSet_script_at(this->inner.get(), index);
  diplomat::result<uint16_t, std::monostate> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<uint16_t>(std::move(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err(std::monostate());
  }
  return diplomat_result_out_value;
}
#endif
