#ifndef ICU4XWordBreakIteratorUtf8_HPP
#define ICU4XWordBreakIteratorUtf8_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XWordBreakIteratorUtf8.h"

#include "ICU4XSegmenterWordType.hpp"

/**
 * A destruction policy for using ICU4XWordBreakIteratorUtf8 with std::unique_ptr.
 */
struct ICU4XWordBreakIteratorUtf8Deleter {
  void operator()(capi::ICU4XWordBreakIteratorUtf8* l) const noexcept {
    capi::ICU4XWordBreakIteratorUtf8_destroy(l);
  }
};

/**
 * 
 * 
 * See the [Rust documentation for `WordBreakIterator`](https://docs.rs/icu/latest/icu/segmenter/struct.WordBreakIterator.html) for more information.
 */
class ICU4XWordBreakIteratorUtf8 {
 public:

  /**
   * Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
   * out of range of a 32-bit signed integer.
   * 
   * See the [Rust documentation for `next`](https://docs.rs/icu/latest/icu/segmenter/struct.WordBreakIterator.html#method.next) for more information.
   */
  int32_t next();

  /**
   * Return the status value of break boundary.
   * 
   * See the [Rust documentation for `word_type`](https://docs.rs/icu/latest/icu/segmenter/struct.WordBreakIterator.html#method.word_type) for more information.
   */
  ICU4XSegmenterWordType word_type() const;

  /**
   * Return true when break boundary is word-like such as letter/number/CJK
   * 
   * See the [Rust documentation for `is_word_like`](https://docs.rs/icu/latest/icu/segmenter/struct.WordBreakIterator.html#method.is_word_like) for more information.
   */
  bool is_word_like() const;
  inline const capi::ICU4XWordBreakIteratorUtf8* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XWordBreakIteratorUtf8* AsFFIMut() { return this->inner.get(); }
  inline ICU4XWordBreakIteratorUtf8(capi::ICU4XWordBreakIteratorUtf8* i) : inner(i) {}
  ICU4XWordBreakIteratorUtf8() = default;
  ICU4XWordBreakIteratorUtf8(ICU4XWordBreakIteratorUtf8&&) noexcept = default;
  ICU4XWordBreakIteratorUtf8& operator=(ICU4XWordBreakIteratorUtf8&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XWordBreakIteratorUtf8, ICU4XWordBreakIteratorUtf8Deleter> inner;
};


inline int32_t ICU4XWordBreakIteratorUtf8::next() {
  return capi::ICU4XWordBreakIteratorUtf8_next(this->inner.get());
}
inline ICU4XSegmenterWordType ICU4XWordBreakIteratorUtf8::word_type() const {
  return static_cast<ICU4XSegmenterWordType>(capi::ICU4XWordBreakIteratorUtf8_word_type(this->inner.get()));
}
inline bool ICU4XWordBreakIteratorUtf8::is_word_like() const {
  return capi::ICU4XWordBreakIteratorUtf8_is_word_like(this->inner.get());
}
#endif
