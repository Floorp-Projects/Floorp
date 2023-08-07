#ifndef ICU4XWordBreakIteratorLatin1_HPP
#define ICU4XWordBreakIteratorLatin1_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XWordBreakIteratorLatin1.h"

#include "ICU4XSegmenterWordType.hpp"

/**
 * A destruction policy for using ICU4XWordBreakIteratorLatin1 with std::unique_ptr.
 */
struct ICU4XWordBreakIteratorLatin1Deleter {
  void operator()(capi::ICU4XWordBreakIteratorLatin1* l) const noexcept {
    capi::ICU4XWordBreakIteratorLatin1_destroy(l);
  }
};

/**
 * 
 * 
 * See the [Rust documentation for `WordBreakIterator`](https://docs.rs/icu/latest/icu/segmenter/struct.WordBreakIterator.html) for more information.
 */
class ICU4XWordBreakIteratorLatin1 {
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
  inline const capi::ICU4XWordBreakIteratorLatin1* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XWordBreakIteratorLatin1* AsFFIMut() { return this->inner.get(); }
  inline ICU4XWordBreakIteratorLatin1(capi::ICU4XWordBreakIteratorLatin1* i) : inner(i) {}
  ICU4XWordBreakIteratorLatin1() = default;
  ICU4XWordBreakIteratorLatin1(ICU4XWordBreakIteratorLatin1&&) noexcept = default;
  ICU4XWordBreakIteratorLatin1& operator=(ICU4XWordBreakIteratorLatin1&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XWordBreakIteratorLatin1, ICU4XWordBreakIteratorLatin1Deleter> inner;
};


inline int32_t ICU4XWordBreakIteratorLatin1::next() {
  return capi::ICU4XWordBreakIteratorLatin1_next(this->inner.get());
}
inline ICU4XSegmenterWordType ICU4XWordBreakIteratorLatin1::word_type() const {
  return static_cast<ICU4XSegmenterWordType>(capi::ICU4XWordBreakIteratorLatin1_word_type(this->inner.get()));
}
inline bool ICU4XWordBreakIteratorLatin1::is_word_like() const {
  return capi::ICU4XWordBreakIteratorLatin1_is_word_like(this->inner.get());
}
#endif
