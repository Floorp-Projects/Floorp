#ifndef ICU4XWordSegmenter_HPP
#define ICU4XWordSegmenter_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XWordSegmenter.h"

class ICU4XDataProvider;
class ICU4XWordSegmenter;
#include "ICU4XError.hpp"
class ICU4XWordBreakIteratorUtf8;
class ICU4XWordBreakIteratorUtf16;
class ICU4XWordBreakIteratorLatin1;

/**
 * A destruction policy for using ICU4XWordSegmenter with std::unique_ptr.
 */
struct ICU4XWordSegmenterDeleter {
  void operator()(capi::ICU4XWordSegmenter* l) const noexcept {
    capi::ICU4XWordSegmenter_destroy(l);
  }
};

/**
 * An ICU4X word-break segmenter, capable of finding word breakpoints in strings.
 * 
 * See the [Rust documentation for `WordSegmenter`](https://docs.rs/icu/latest/icu/segmenter/struct.WordSegmenter.html) for more information.
 */
class ICU4XWordSegmenter {
 public:

  /**
   * Construct an [`ICU4XWordSegmenter`] with automatically selecting the best available LSTM
   * or dictionary payload data.
   * 
   * Note: currently, it uses dictionary for Chinese and Japanese, and LSTM for Burmese,
   * Khmer, Lao, and Thai.
   * 
   * See the [Rust documentation for `try_new_auto_unstable`](https://docs.rs/icu/latest/icu/segmenter/struct.WordSegmenter.html#method.try_new_auto_unstable) for more information.
   */
  static diplomat::result<ICU4XWordSegmenter, ICU4XError> create_auto(const ICU4XDataProvider& provider);

  /**
   * Construct an [`ICU4XWordSegmenter`] with LSTM payload data for Burmese, Khmer, Lao, and
   * Thai.
   * 
   * Warning: [`ICU4XWordSegmenter`] created by this function doesn't handle Chinese or
   * Japanese.
   * 
   * See the [Rust documentation for `try_new_lstm_unstable`](https://docs.rs/icu/latest/icu/segmenter/struct.WordSegmenter.html#method.try_new_lstm_unstable) for more information.
   */
  static diplomat::result<ICU4XWordSegmenter, ICU4XError> create_lstm(const ICU4XDataProvider& provider);

  /**
   * Construct an [`ICU4XWordSegmenter`] with dictionary payload data for Chinese, Japanese,
   * Burmese, Khmer, Lao, and Thai.
   * 
   * See the [Rust documentation for `try_new_dictionary_unstable`](https://docs.rs/icu/latest/icu/segmenter/struct.WordSegmenter.html#method.try_new_dictionary_unstable) for more information.
   */
  static diplomat::result<ICU4XWordSegmenter, ICU4XError> create_dictionary(const ICU4XDataProvider& provider);

  /**
   * Segments a (potentially ill-formed) UTF-8 string.
   * 
   * See the [Rust documentation for `segment_utf8`](https://docs.rs/icu/latest/icu/segmenter/struct.WordSegmenter.html#method.segment_utf8) for more information.
   * 
   * Lifetimes: `this`, `input` must live at least as long as the output.
   */
  ICU4XWordBreakIteratorUtf8 segment_utf8(const std::string_view input) const;

  /**
   * Segments a UTF-16 string.
   * 
   * See the [Rust documentation for `segment_utf16`](https://docs.rs/icu/latest/icu/segmenter/struct.WordSegmenter.html#method.segment_utf16) for more information.
   * 
   * Lifetimes: `this`, `input` must live at least as long as the output.
   */
  ICU4XWordBreakIteratorUtf16 segment_utf16(const diplomat::span<const uint16_t> input) const;

  /**
   * Segments a Latin-1 string.
   * 
   * See the [Rust documentation for `segment_latin1`](https://docs.rs/icu/latest/icu/segmenter/struct.WordSegmenter.html#method.segment_latin1) for more information.
   * 
   * Lifetimes: `this`, `input` must live at least as long as the output.
   */
  ICU4XWordBreakIteratorLatin1 segment_latin1(const diplomat::span<const uint8_t> input) const;
  inline const capi::ICU4XWordSegmenter* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XWordSegmenter* AsFFIMut() { return this->inner.get(); }
  inline ICU4XWordSegmenter(capi::ICU4XWordSegmenter* i) : inner(i) {}
  ICU4XWordSegmenter() = default;
  ICU4XWordSegmenter(ICU4XWordSegmenter&&) noexcept = default;
  ICU4XWordSegmenter& operator=(ICU4XWordSegmenter&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XWordSegmenter, ICU4XWordSegmenterDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XWordBreakIteratorUtf8.hpp"
#include "ICU4XWordBreakIteratorUtf16.hpp"
#include "ICU4XWordBreakIteratorLatin1.hpp"

inline diplomat::result<ICU4XWordSegmenter, ICU4XError> ICU4XWordSegmenter::create_auto(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XWordSegmenter_create_auto(provider.AsFFI());
  diplomat::result<ICU4XWordSegmenter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XWordSegmenter>(std::move(ICU4XWordSegmenter(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XWordSegmenter, ICU4XError> ICU4XWordSegmenter::create_lstm(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XWordSegmenter_create_lstm(provider.AsFFI());
  diplomat::result<ICU4XWordSegmenter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XWordSegmenter>(std::move(ICU4XWordSegmenter(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XWordSegmenter, ICU4XError> ICU4XWordSegmenter::create_dictionary(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XWordSegmenter_create_dictionary(provider.AsFFI());
  diplomat::result<ICU4XWordSegmenter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XWordSegmenter>(std::move(ICU4XWordSegmenter(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline ICU4XWordBreakIteratorUtf8 ICU4XWordSegmenter::segment_utf8(const std::string_view input) const {
  return ICU4XWordBreakIteratorUtf8(capi::ICU4XWordSegmenter_segment_utf8(this->inner.get(), input.data(), input.size()));
}
inline ICU4XWordBreakIteratorUtf16 ICU4XWordSegmenter::segment_utf16(const diplomat::span<const uint16_t> input) const {
  return ICU4XWordBreakIteratorUtf16(capi::ICU4XWordSegmenter_segment_utf16(this->inner.get(), input.data(), input.size()));
}
inline ICU4XWordBreakIteratorLatin1 ICU4XWordSegmenter::segment_latin1(const diplomat::span<const uint8_t> input) const {
  return ICU4XWordBreakIteratorLatin1(capi::ICU4XWordSegmenter_segment_latin1(this->inner.get(), input.data(), input.size()));
}
#endif
