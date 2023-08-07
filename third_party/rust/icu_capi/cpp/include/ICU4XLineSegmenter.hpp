#ifndef ICU4XLineSegmenter_HPP
#define ICU4XLineSegmenter_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XLineSegmenter.h"

class ICU4XDataProvider;
class ICU4XLineSegmenter;
#include "ICU4XError.hpp"
struct ICU4XLineBreakOptionsV1;
class ICU4XLineBreakIteratorUtf8;
class ICU4XLineBreakIteratorUtf16;
class ICU4XLineBreakIteratorLatin1;

/**
 * A destruction policy for using ICU4XLineSegmenter with std::unique_ptr.
 */
struct ICU4XLineSegmenterDeleter {
  void operator()(capi::ICU4XLineSegmenter* l) const noexcept {
    capi::ICU4XLineSegmenter_destroy(l);
  }
};

/**
 * An ICU4X line-break segmenter, capable of finding breakpoints in strings.
 * 
 * See the [Rust documentation for `LineSegmenter`](https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html) for more information.
 */
class ICU4XLineSegmenter {
 public:

  /**
   * Construct a [`ICU4XLineSegmenter`] with default options. It automatically loads the best
   * available payload data for Burmese, Khmer, Lao, and Thai.
   * 
   * See the [Rust documentation for `try_new_auto_unstable`](https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.try_new_auto_unstable) for more information.
   */
  static diplomat::result<ICU4XLineSegmenter, ICU4XError> create_auto(const ICU4XDataProvider& provider);

  /**
   * Construct a [`ICU4XLineSegmenter`] with default options and LSTM payload data for
   * Burmese, Khmer, Lao, and Thai.
   * 
   * See the [Rust documentation for `try_new_lstm_unstable`](https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.try_new_lstm_unstable) for more information.
   */
  static diplomat::result<ICU4XLineSegmenter, ICU4XError> create_lstm(const ICU4XDataProvider& provider);

  /**
   * Construct a [`ICU4XLineSegmenter`] with default options and dictionary payload data for
   * Burmese, Khmer, Lao, and Thai..
   * 
   * See the [Rust documentation for `try_new_dictionary_unstable`](https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.try_new_dictionary_unstable) for more information.
   */
  static diplomat::result<ICU4XLineSegmenter, ICU4XError> create_dictionary(const ICU4XDataProvider& provider);

  /**
   * Construct a [`ICU4XLineSegmenter`] with custom options. It automatically loads the best
   * available payload data for Burmese, Khmer, Lao, and Thai.
   * 
   * See the [Rust documentation for `try_new_auto_with_options_unstable`](https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.try_new_auto_with_options_unstable) for more information.
   */
  static diplomat::result<ICU4XLineSegmenter, ICU4XError> create_auto_with_options_v1(const ICU4XDataProvider& provider, ICU4XLineBreakOptionsV1 options);

  /**
   * Construct a [`ICU4XLineSegmenter`] with custom options and LSTM payload data for
   * Burmese, Khmer, Lao, and Thai.
   * 
   * See the [Rust documentation for `try_new_lstm_with_options_unstable`](https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.try_new_lstm_with_options_unstable) for more information.
   */
  static diplomat::result<ICU4XLineSegmenter, ICU4XError> create_lstm_with_options_v1(const ICU4XDataProvider& provider, ICU4XLineBreakOptionsV1 options);

  /**
   * Construct a [`ICU4XLineSegmenter`] with custom options and dictionary payload data for
   * Burmese, Khmer, Lao, and Thai.
   * 
   * See the [Rust documentation for `try_new_dictionary_with_options_unstable`](https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.try_new_dictionary_with_options_unstable) for more information.
   */
  static diplomat::result<ICU4XLineSegmenter, ICU4XError> create_dictionary_with_options_v1(const ICU4XDataProvider& provider, ICU4XLineBreakOptionsV1 options);

  /**
   * Segments a (potentially ill-formed) UTF-8 string.
   * 
   * See the [Rust documentation for `segment_utf8`](https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.segment_utf8) for more information.
   * 
   * Lifetimes: `this`, `input` must live at least as long as the output.
   */
  ICU4XLineBreakIteratorUtf8 segment_utf8(const std::string_view input) const;

  /**
   * Segments a UTF-16 string.
   * 
   * See the [Rust documentation for `segment_utf16`](https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.segment_utf16) for more information.
   * 
   * Lifetimes: `this`, `input` must live at least as long as the output.
   */
  ICU4XLineBreakIteratorUtf16 segment_utf16(const diplomat::span<const uint16_t> input) const;

  /**
   * Segments a Latin-1 string.
   * 
   * See the [Rust documentation for `segment_latin1`](https://docs.rs/icu/latest/icu/segmenter/struct.LineSegmenter.html#method.segment_latin1) for more information.
   * 
   * Lifetimes: `this`, `input` must live at least as long as the output.
   */
  ICU4XLineBreakIteratorLatin1 segment_latin1(const diplomat::span<const uint8_t> input) const;
  inline const capi::ICU4XLineSegmenter* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XLineSegmenter* AsFFIMut() { return this->inner.get(); }
  inline ICU4XLineSegmenter(capi::ICU4XLineSegmenter* i) : inner(i) {}
  ICU4XLineSegmenter() = default;
  ICU4XLineSegmenter(ICU4XLineSegmenter&&) noexcept = default;
  ICU4XLineSegmenter& operator=(ICU4XLineSegmenter&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XLineSegmenter, ICU4XLineSegmenterDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XLineBreakOptionsV1.hpp"
#include "ICU4XLineBreakIteratorUtf8.hpp"
#include "ICU4XLineBreakIteratorUtf16.hpp"
#include "ICU4XLineBreakIteratorLatin1.hpp"

inline diplomat::result<ICU4XLineSegmenter, ICU4XError> ICU4XLineSegmenter::create_auto(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XLineSegmenter_create_auto(provider.AsFFI());
  diplomat::result<ICU4XLineSegmenter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XLineSegmenter>(std::move(ICU4XLineSegmenter(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XLineSegmenter, ICU4XError> ICU4XLineSegmenter::create_lstm(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XLineSegmenter_create_lstm(provider.AsFFI());
  diplomat::result<ICU4XLineSegmenter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XLineSegmenter>(std::move(ICU4XLineSegmenter(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XLineSegmenter, ICU4XError> ICU4XLineSegmenter::create_dictionary(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XLineSegmenter_create_dictionary(provider.AsFFI());
  diplomat::result<ICU4XLineSegmenter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XLineSegmenter>(std::move(ICU4XLineSegmenter(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XLineSegmenter, ICU4XError> ICU4XLineSegmenter::create_auto_with_options_v1(const ICU4XDataProvider& provider, ICU4XLineBreakOptionsV1 options) {
  ICU4XLineBreakOptionsV1 diplomat_wrapped_struct_options = options;
  auto diplomat_result_raw_out_value = capi::ICU4XLineSegmenter_create_auto_with_options_v1(provider.AsFFI(), capi::ICU4XLineBreakOptionsV1{ .strictness = static_cast<capi::ICU4XLineBreakStrictness>(diplomat_wrapped_struct_options.strictness), .word_option = static_cast<capi::ICU4XLineBreakWordOption>(diplomat_wrapped_struct_options.word_option), .ja_zh = diplomat_wrapped_struct_options.ja_zh });
  diplomat::result<ICU4XLineSegmenter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XLineSegmenter>(std::move(ICU4XLineSegmenter(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XLineSegmenter, ICU4XError> ICU4XLineSegmenter::create_lstm_with_options_v1(const ICU4XDataProvider& provider, ICU4XLineBreakOptionsV1 options) {
  ICU4XLineBreakOptionsV1 diplomat_wrapped_struct_options = options;
  auto diplomat_result_raw_out_value = capi::ICU4XLineSegmenter_create_lstm_with_options_v1(provider.AsFFI(), capi::ICU4XLineBreakOptionsV1{ .strictness = static_cast<capi::ICU4XLineBreakStrictness>(diplomat_wrapped_struct_options.strictness), .word_option = static_cast<capi::ICU4XLineBreakWordOption>(diplomat_wrapped_struct_options.word_option), .ja_zh = diplomat_wrapped_struct_options.ja_zh });
  diplomat::result<ICU4XLineSegmenter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XLineSegmenter>(std::move(ICU4XLineSegmenter(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XLineSegmenter, ICU4XError> ICU4XLineSegmenter::create_dictionary_with_options_v1(const ICU4XDataProvider& provider, ICU4XLineBreakOptionsV1 options) {
  ICU4XLineBreakOptionsV1 diplomat_wrapped_struct_options = options;
  auto diplomat_result_raw_out_value = capi::ICU4XLineSegmenter_create_dictionary_with_options_v1(provider.AsFFI(), capi::ICU4XLineBreakOptionsV1{ .strictness = static_cast<capi::ICU4XLineBreakStrictness>(diplomat_wrapped_struct_options.strictness), .word_option = static_cast<capi::ICU4XLineBreakWordOption>(diplomat_wrapped_struct_options.word_option), .ja_zh = diplomat_wrapped_struct_options.ja_zh });
  diplomat::result<ICU4XLineSegmenter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XLineSegmenter>(std::move(ICU4XLineSegmenter(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline ICU4XLineBreakIteratorUtf8 ICU4XLineSegmenter::segment_utf8(const std::string_view input) const {
  return ICU4XLineBreakIteratorUtf8(capi::ICU4XLineSegmenter_segment_utf8(this->inner.get(), input.data(), input.size()));
}
inline ICU4XLineBreakIteratorUtf16 ICU4XLineSegmenter::segment_utf16(const diplomat::span<const uint16_t> input) const {
  return ICU4XLineBreakIteratorUtf16(capi::ICU4XLineSegmenter_segment_utf16(this->inner.get(), input.data(), input.size()));
}
inline ICU4XLineBreakIteratorLatin1 ICU4XLineSegmenter::segment_latin1(const diplomat::span<const uint8_t> input) const {
  return ICU4XLineBreakIteratorLatin1(capi::ICU4XLineSegmenter_segment_latin1(this->inner.get(), input.data(), input.size()));
}
#endif
