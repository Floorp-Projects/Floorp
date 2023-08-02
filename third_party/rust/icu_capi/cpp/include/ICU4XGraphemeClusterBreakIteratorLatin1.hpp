#ifndef ICU4XGraphemeClusterBreakIteratorLatin1_HPP
#define ICU4XGraphemeClusterBreakIteratorLatin1_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XGraphemeClusterBreakIteratorLatin1.h"


/**
 * A destruction policy for using ICU4XGraphemeClusterBreakIteratorLatin1 with std::unique_ptr.
 */
struct ICU4XGraphemeClusterBreakIteratorLatin1Deleter {
  void operator()(capi::ICU4XGraphemeClusterBreakIteratorLatin1* l) const noexcept {
    capi::ICU4XGraphemeClusterBreakIteratorLatin1_destroy(l);
  }
};

/**
 * 
 * 
 * See the [Rust documentation for `GraphemeClusterBreakIterator`](https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterBreakIterator.html) for more information.
 */
class ICU4XGraphemeClusterBreakIteratorLatin1 {
 public:

  /**
   * Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
   * out of range of a 32-bit signed integer.
   * 
   * See the [Rust documentation for `next`](https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterBreakIterator.html#method.next) for more information.
   */
  int32_t next();
  inline const capi::ICU4XGraphemeClusterBreakIteratorLatin1* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XGraphemeClusterBreakIteratorLatin1* AsFFIMut() { return this->inner.get(); }
  inline ICU4XGraphemeClusterBreakIteratorLatin1(capi::ICU4XGraphemeClusterBreakIteratorLatin1* i) : inner(i) {}
  ICU4XGraphemeClusterBreakIteratorLatin1() = default;
  ICU4XGraphemeClusterBreakIteratorLatin1(ICU4XGraphemeClusterBreakIteratorLatin1&&) noexcept = default;
  ICU4XGraphemeClusterBreakIteratorLatin1& operator=(ICU4XGraphemeClusterBreakIteratorLatin1&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XGraphemeClusterBreakIteratorLatin1, ICU4XGraphemeClusterBreakIteratorLatin1Deleter> inner;
};


inline int32_t ICU4XGraphemeClusterBreakIteratorLatin1::next() {
  return capi::ICU4XGraphemeClusterBreakIteratorLatin1_next(this->inner.get());
}
#endif
