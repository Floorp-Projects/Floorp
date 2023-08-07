#ifndef ICU4XSentenceSegmenter_HPP
#define ICU4XSentenceSegmenter_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XSentenceSegmenter.h"

class ICU4XDataProvider;
class ICU4XSentenceSegmenter;
#include "ICU4XError.hpp"
class ICU4XSentenceBreakIteratorUtf8;
class ICU4XSentenceBreakIteratorUtf16;
class ICU4XSentenceBreakIteratorLatin1;

/**
 * A destruction policy for using ICU4XSentenceSegmenter with std::unique_ptr.
 */
struct ICU4XSentenceSegmenterDeleter {
  void operator()(capi::ICU4XSentenceSegmenter* l) const noexcept {
    capi::ICU4XSentenceSegmenter_destroy(l);
  }
};

/**
 * An ICU4X sentence-break segmenter, capable of finding sentence breakpoints in strings.
 * 
 * See the [Rust documentation for `SentenceSegmenter`](https://docs.rs/icu/latest/icu/segmenter/struct.SentenceSegmenter.html) for more information.
 */
class ICU4XSentenceSegmenter {
 public:

  /**
   * Construct an [`ICU4XSentenceSegmenter`].
   * 
   * See the [Rust documentation for `try_new_unstable`](https://docs.rs/icu/latest/icu/segmenter/struct.SentenceSegmenter.html#method.try_new_unstable) for more information.
   */
  static diplomat::result<ICU4XSentenceSegmenter, ICU4XError> create(const ICU4XDataProvider& provider);

  /**
   * Segments a (potentially ill-formed) UTF-8 string.
   * 
   * See the [Rust documentation for `segment_utf8`](https://docs.rs/icu/latest/icu/segmenter/struct.SentenceSegmenter.html#method.segment_utf8) for more information.
   * 
   * Lifetimes: `this`, `input` must live at least as long as the output.
   */
  ICU4XSentenceBreakIteratorUtf8 segment_utf8(const std::string_view input) const;

  /**
   * Segments a UTF-16 string.
   * 
   * See the [Rust documentation for `segment_utf16`](https://docs.rs/icu/latest/icu/segmenter/struct.SentenceSegmenter.html#method.segment_utf16) for more information.
   * 
   * Lifetimes: `this`, `input` must live at least as long as the output.
   */
  ICU4XSentenceBreakIteratorUtf16 segment_utf16(const diplomat::span<const uint16_t> input) const;

  /**
   * Segments a Latin-1 string.
   * 
   * See the [Rust documentation for `segment_latin1`](https://docs.rs/icu/latest/icu/segmenter/struct.SentenceSegmenter.html#method.segment_latin1) for more information.
   * 
   * Lifetimes: `this`, `input` must live at least as long as the output.
   */
  ICU4XSentenceBreakIteratorLatin1 segment_latin1(const diplomat::span<const uint8_t> input) const;
  inline const capi::ICU4XSentenceSegmenter* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XSentenceSegmenter* AsFFIMut() { return this->inner.get(); }
  inline ICU4XSentenceSegmenter(capi::ICU4XSentenceSegmenter* i) : inner(i) {}
  ICU4XSentenceSegmenter() = default;
  ICU4XSentenceSegmenter(ICU4XSentenceSegmenter&&) noexcept = default;
  ICU4XSentenceSegmenter& operator=(ICU4XSentenceSegmenter&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XSentenceSegmenter, ICU4XSentenceSegmenterDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XSentenceBreakIteratorUtf8.hpp"
#include "ICU4XSentenceBreakIteratorUtf16.hpp"
#include "ICU4XSentenceBreakIteratorLatin1.hpp"

inline diplomat::result<ICU4XSentenceSegmenter, ICU4XError> ICU4XSentenceSegmenter::create(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XSentenceSegmenter_create(provider.AsFFI());
  diplomat::result<ICU4XSentenceSegmenter, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XSentenceSegmenter>(std::move(ICU4XSentenceSegmenter(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline ICU4XSentenceBreakIteratorUtf8 ICU4XSentenceSegmenter::segment_utf8(const std::string_view input) const {
  return ICU4XSentenceBreakIteratorUtf8(capi::ICU4XSentenceSegmenter_segment_utf8(this->inner.get(), input.data(), input.size()));
}
inline ICU4XSentenceBreakIteratorUtf16 ICU4XSentenceSegmenter::segment_utf16(const diplomat::span<const uint16_t> input) const {
  return ICU4XSentenceBreakIteratorUtf16(capi::ICU4XSentenceSegmenter_segment_utf16(this->inner.get(), input.data(), input.size()));
}
inline ICU4XSentenceBreakIteratorLatin1 ICU4XSentenceSegmenter::segment_latin1(const diplomat::span<const uint8_t> input) const {
  return ICU4XSentenceBreakIteratorLatin1(capi::ICU4XSentenceSegmenter_segment_latin1(this->inner.get(), input.data(), input.size()));
}
#endif
