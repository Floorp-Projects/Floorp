#ifndef ICU4XSentenceBreakIteratorLatin1_HPP
#define ICU4XSentenceBreakIteratorLatin1_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XSentenceBreakIteratorLatin1.h"


/**
 * A destruction policy for using ICU4XSentenceBreakIteratorLatin1 with std::unique_ptr.
 */
struct ICU4XSentenceBreakIteratorLatin1Deleter {
  void operator()(capi::ICU4XSentenceBreakIteratorLatin1* l) const noexcept {
    capi::ICU4XSentenceBreakIteratorLatin1_destroy(l);
  }
};

/**
 * 
 * 
 * See the [Rust documentation for `SentenceBreakIterator`](https://docs.rs/icu/latest/icu/segmenter/struct.SentenceBreakIterator.html) for more information.
 */
class ICU4XSentenceBreakIteratorLatin1 {
 public:

  /**
   * Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
   * out of range of a 32-bit signed integer.
   * 
   * See the [Rust documentation for `next`](https://docs.rs/icu/latest/icu/segmenter/struct.SentenceBreakIterator.html#method.next) for more information.
   */
  int32_t next();
  inline const capi::ICU4XSentenceBreakIteratorLatin1* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XSentenceBreakIteratorLatin1* AsFFIMut() { return this->inner.get(); }
  inline ICU4XSentenceBreakIteratorLatin1(capi::ICU4XSentenceBreakIteratorLatin1* i) : inner(i) {}
  ICU4XSentenceBreakIteratorLatin1() = default;
  ICU4XSentenceBreakIteratorLatin1(ICU4XSentenceBreakIteratorLatin1&&) noexcept = default;
  ICU4XSentenceBreakIteratorLatin1& operator=(ICU4XSentenceBreakIteratorLatin1&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XSentenceBreakIteratorLatin1, ICU4XSentenceBreakIteratorLatin1Deleter> inner;
};


inline int32_t ICU4XSentenceBreakIteratorLatin1::next() {
  return capi::ICU4XSentenceBreakIteratorLatin1_next(this->inner.get());
}
#endif
