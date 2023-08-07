#ifndef ICU4XPropertyValueNameToEnumMapper_HPP
#define ICU4XPropertyValueNameToEnumMapper_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XPropertyValueNameToEnumMapper.h"

class ICU4XDataProvider;
class ICU4XPropertyValueNameToEnumMapper;
#include "ICU4XError.hpp"

/**
 * A destruction policy for using ICU4XPropertyValueNameToEnumMapper with std::unique_ptr.
 */
struct ICU4XPropertyValueNameToEnumMapperDeleter {
  void operator()(capi::ICU4XPropertyValueNameToEnumMapper* l) const noexcept {
    capi::ICU4XPropertyValueNameToEnumMapper_destroy(l);
  }
};

/**
 * A type capable of looking up a property value from a string name.
 * 
 * See the [Rust documentation for `PropertyValueNameToEnumMapper`](https://docs.rs/icu/latest/icu/properties/names/struct.PropertyValueNameToEnumMapper.html) for more information.
 * 
 * See the [Rust documentation for `PropertyValueNameToEnumMapperBorrowed`](https://docs.rs/icu/latest/icu/properties/names/struct.PropertyValueNameToEnumMapperBorrowed.html) for more information.
 */
class ICU4XPropertyValueNameToEnumMapper {
 public:

  /**
   * Get the property value matching the given name, using strict matching
   * 
   * Returns -1 if the name is unknown for this property
   * 
   * See the [Rust documentation for `get_strict`](https://docs.rs/icu/latest/icu/properties/names/struct.PropertyValueNameToEnumMapperBorrowed.html#method.get_strict) for more information.
   */
  int16_t get_strict(const std::string_view name) const;

  /**
   * Get the property value matching the given name, using loose matching
   * 
   * Returns -1 if the name is unknown for this property
   * 
   * See the [Rust documentation for `get_loose`](https://docs.rs/icu/latest/icu/properties/names/struct.PropertyValueNameToEnumMapperBorrowed.html#method.get_loose) for more information.
   */
  int16_t get_loose(const std::string_view name) const;

  /**
   * 
   * 
   * See the [Rust documentation for `get_name_to_enum_mapper`](https://docs.rs/icu/latest/icu/properties/struct.GeneralCategory.html#method.get_name_to_enum_mapper) for more information.
   */
  static diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> load_general_category(const ICU4XDataProvider& provider);

  /**
   * 
   * 
   * See the [Rust documentation for `get_name_to_enum_mapper`](https://docs.rs/icu/latest/icu/properties/struct.BidiClass.html#method.get_name_to_enum_mapper) for more information.
   */
  static diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> load_bidi_class(const ICU4XDataProvider& provider);

  /**
   * 
   * 
   * See the [Rust documentation for `get_name_to_enum_mapper`](https://docs.rs/icu/latest/icu/properties/struct.EastAsianWidth.html#method.get_name_to_enum_mapper) for more information.
   */
  static diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> load_east_asian_width(const ICU4XDataProvider& provider);

  /**
   * 
   * 
   * See the [Rust documentation for `get_name_to_enum_mapper`](https://docs.rs/icu/latest/icu/properties/struct.LineBreak.html#method.get_name_to_enum_mapper) for more information.
   */
  static diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> load_line_break(const ICU4XDataProvider& provider);

  /**
   * 
   * 
   * See the [Rust documentation for `get_name_to_enum_mapper`](https://docs.rs/icu/latest/icu/properties/struct.GraphemeClusterBreak.html#method.get_name_to_enum_mapper) for more information.
   */
  static diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> load_grapheme_cluster_break(const ICU4XDataProvider& provider);

  /**
   * 
   * 
   * See the [Rust documentation for `get_name_to_enum_mapper`](https://docs.rs/icu/latest/icu/properties/struct.WordBreak.html#method.get_name_to_enum_mapper) for more information.
   */
  static diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> load_word_break(const ICU4XDataProvider& provider);

  /**
   * 
   * 
   * See the [Rust documentation for `get_name_to_enum_mapper`](https://docs.rs/icu/latest/icu/properties/struct.SentenceBreak.html#method.get_name_to_enum_mapper) for more information.
   */
  static diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> load_sentence_break(const ICU4XDataProvider& provider);

  /**
   * 
   * 
   * See the [Rust documentation for `get_name_to_enum_mapper`](https://docs.rs/icu/latest/icu/properties/struct.Script.html#method.get_name_to_enum_mapper) for more information.
   */
  static diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> load_script(const ICU4XDataProvider& provider);
  inline const capi::ICU4XPropertyValueNameToEnumMapper* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XPropertyValueNameToEnumMapper* AsFFIMut() { return this->inner.get(); }
  inline ICU4XPropertyValueNameToEnumMapper(capi::ICU4XPropertyValueNameToEnumMapper* i) : inner(i) {}
  ICU4XPropertyValueNameToEnumMapper() = default;
  ICU4XPropertyValueNameToEnumMapper(ICU4XPropertyValueNameToEnumMapper&&) noexcept = default;
  ICU4XPropertyValueNameToEnumMapper& operator=(ICU4XPropertyValueNameToEnumMapper&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XPropertyValueNameToEnumMapper, ICU4XPropertyValueNameToEnumMapperDeleter> inner;
};

#include "ICU4XDataProvider.hpp"

inline int16_t ICU4XPropertyValueNameToEnumMapper::get_strict(const std::string_view name) const {
  return capi::ICU4XPropertyValueNameToEnumMapper_get_strict(this->inner.get(), name.data(), name.size());
}
inline int16_t ICU4XPropertyValueNameToEnumMapper::get_loose(const std::string_view name) const {
  return capi::ICU4XPropertyValueNameToEnumMapper_get_loose(this->inner.get(), name.data(), name.size());
}
inline diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> ICU4XPropertyValueNameToEnumMapper::load_general_category(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XPropertyValueNameToEnumMapper_load_general_category(provider.AsFFI());
  diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XPropertyValueNameToEnumMapper>(std::move(ICU4XPropertyValueNameToEnumMapper(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> ICU4XPropertyValueNameToEnumMapper::load_bidi_class(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XPropertyValueNameToEnumMapper_load_bidi_class(provider.AsFFI());
  diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XPropertyValueNameToEnumMapper>(std::move(ICU4XPropertyValueNameToEnumMapper(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> ICU4XPropertyValueNameToEnumMapper::load_east_asian_width(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XPropertyValueNameToEnumMapper_load_east_asian_width(provider.AsFFI());
  diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XPropertyValueNameToEnumMapper>(std::move(ICU4XPropertyValueNameToEnumMapper(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> ICU4XPropertyValueNameToEnumMapper::load_line_break(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XPropertyValueNameToEnumMapper_load_line_break(provider.AsFFI());
  diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XPropertyValueNameToEnumMapper>(std::move(ICU4XPropertyValueNameToEnumMapper(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> ICU4XPropertyValueNameToEnumMapper::load_grapheme_cluster_break(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XPropertyValueNameToEnumMapper_load_grapheme_cluster_break(provider.AsFFI());
  diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XPropertyValueNameToEnumMapper>(std::move(ICU4XPropertyValueNameToEnumMapper(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> ICU4XPropertyValueNameToEnumMapper::load_word_break(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XPropertyValueNameToEnumMapper_load_word_break(provider.AsFFI());
  diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XPropertyValueNameToEnumMapper>(std::move(ICU4XPropertyValueNameToEnumMapper(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> ICU4XPropertyValueNameToEnumMapper::load_sentence_break(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XPropertyValueNameToEnumMapper_load_sentence_break(provider.AsFFI());
  diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XPropertyValueNameToEnumMapper>(std::move(ICU4XPropertyValueNameToEnumMapper(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> ICU4XPropertyValueNameToEnumMapper::load_script(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XPropertyValueNameToEnumMapper_load_script(provider.AsFFI());
  diplomat::result<ICU4XPropertyValueNameToEnumMapper, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XPropertyValueNameToEnumMapper>(std::move(ICU4XPropertyValueNameToEnumMapper(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
#endif
