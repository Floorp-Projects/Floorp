#ifndef ICU4XGraphemeClusterSegmenter_HPP
#define ICU4XGraphemeClusterSegmenter_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XGraphemeClusterSegmenter.h"

class ICU4XDataProvider;
class ICU4XGraphemeClusterSegmenter;
#include "ICU4XError.hpp"
class ICU4XGraphemeClusterBreakIteratorUtf8;
class ICU4XGraphemeClusterBreakIteratorUtf16;
class ICU4XGraphemeClusterBreakIteratorLatin1;

/**
 * A destruction policy for using ICU4XGraphemeClusterSegmenter with std::unique_ptr.
 */
struct ICU4XGraphemeClusterSegmenterDeleter {
  void operator()(capi::ICU4XGraphemeClusterSegmenter* l) const noexcept {
    capi::ICU4XGraphemeClusterSegmenter_destroy(l);
  }
};

/**
 * An ICU4X grapheme-cluster-break segmenter, capable of finding grapheme cluster breakpoints
 * in strings.
 * 
 * See the [Rust documentation for `GraphemeClusterSegmenter`](https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterSegmenter.html) for more information.
 */
class ICU4XGraphemeClusterSegmenter {
 public:

  /**
   * Construct an [`ICU4XGraphemeClusterSegmenter`].
   * 
   * See the [Rust documentation for `try_new_unstable`](https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterSegmenter.html#method.try_new_unstable) for more information.
   */
  static diplomat::result<ICU4XGraphemeClusterSegmenter, ICU4XError> create(const ICU4XDataProvider& provider);

  /**
   * Segments a (potentially ill-formed) UTF-8 string.
   * 
   * See the [Rust documentation for `segment_utf8`](https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterSegmenter.html#method.segment_utf8) for more information.
   * 
   * Lifetimes: `this`, `input` must live at least as long as the output.
   */
  ICU4XGraphemeClusterBreakIteratorUtf8 segment_utf8(const std::string_view input) const;

  /**
   * Segments a UTF-16 string.
   * 
   * See the [Rust documentation for `segment_utf16`](https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterSegmenter.html#method.segment_utf16) for more information.
   * 
   * Lifetimes: `this`, `input` must live at least as long as the output.
   */
  ICU4XGraphemeClusterBreakIteratorUtf16 segment_utf16(const diplomat::span<const uint16_t> input) const;

  /**
   * Segments a Latin-1 string.
   * 
   * See the [Rust documentation for `segment_latin1`](https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterSegmenter.html#method.segment_latin1) for more information.
   * 
   * Lifetimes: `this`, `input` must live at least as long as the output.
   */
  ICU4XGraphemeClusterBreakIteratorLatin1 segment_latin1(const diplomat::span<const uint8_t> input) const;
  inline const capi::ICU4XGraphemeClusterSegmenter* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XGraphemeClusterSegmenter* AsFFIMut() { return this->inner.get(); }
  inline ICU4XGraphemeClusterSegmenter(capi::ICU4XGraphemeClusterSegmenter* i) : inner(i) {}
  ICU4XGraphemeClusterSegmenter() = default;
  ICU4XGraphemeClusterSegmenter(ICU4XGraphemeClusterSegmenter&&) noexcept = default;
  ICU4XGraphemeClusterSegmenter& operator=(ICU4XGraphemeClusterSegmenter&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XGraphemeClusterSegmenter, ICU4XGraphemeClusterSegmenterDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XGraphemeClusterBreakIteratorUtf8.hpp"
#include "ICU4XGraphemeClusterBreakIteratorUtf16.hpp"
#include "ICU4XGraphemeClusterBreakIteratorLatin1.hpp"

inline diplomat::result<ICU4XGraphemeClusterSegmenter, ICU4XError> ICU4XGraphemeClusterSegmenter::create(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XGraphemeClusterSegmenter_create(provider.AsFFI());
  diplomat::result<ICU4XGraphemeClusterSegmenter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XGraphemeClusterSegmenter>(std::move(ICU4XGraphemeClusterSegmenter(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline ICU4XGraphemeClusterBreakIteratorUtf8 ICU4XGraphemeClusterSegmenter::segment_utf8(const std::string_view input) const {
  return ICU4XGraphemeClusterBreakIteratorUtf8(capi::ICU4XGraphemeClusterSegmenter_segment_utf8(this->inner.get(), input.data(), input.size()));
}
inline ICU4XGraphemeClusterBreakIteratorUtf16 ICU4XGraphemeClusterSegmenter::segment_utf16(const diplomat::span<const uint16_t> input) const {
  return ICU4XGraphemeClusterBreakIteratorUtf16(capi::ICU4XGraphemeClusterSegmenter_segment_utf16(this->inner.get(), input.data(), input.size()));
}
inline ICU4XGraphemeClusterBreakIteratorLatin1 ICU4XGraphemeClusterSegmenter::segment_latin1(const diplomat::span<const uint8_t> input) const {
  return ICU4XGraphemeClusterBreakIteratorLatin1(capi::ICU4XGraphemeClusterSegmenter_segment_latin1(this->inner.get(), input.data(), input.size()));
}
#endif
