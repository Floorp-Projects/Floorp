#ifndef ICU4XGraphemeClusterBreakIteratorUtf16_HPP
#define ICU4XGraphemeClusterBreakIteratorUtf16_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XGraphemeClusterBreakIteratorUtf16.h"


/**
 * A destruction policy for using ICU4XGraphemeClusterBreakIteratorUtf16 with std::unique_ptr.
 */
struct ICU4XGraphemeClusterBreakIteratorUtf16Deleter {
  void operator()(capi::ICU4XGraphemeClusterBreakIteratorUtf16* l) const noexcept {
    capi::ICU4XGraphemeClusterBreakIteratorUtf16_destroy(l);
  }
};

/**
 * 
 * 
 * See the [Rust documentation for `GraphemeClusterBreakIterator`](https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterBreakIterator.html) for more information.
 */
class ICU4XGraphemeClusterBreakIteratorUtf16 {
 public:

  /**
   * Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
   * out of range of a 32-bit signed integer.
   * 
   * See the [Rust documentation for `next`](https://docs.rs/icu/latest/icu/segmenter/struct.GraphemeClusterBreakIterator.html#method.next) for more information.
   */
  int32_t next();
  inline const capi::ICU4XGraphemeClusterBreakIteratorUtf16* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XGraphemeClusterBreakIteratorUtf16* AsFFIMut() { return this->inner.get(); }
  inline ICU4XGraphemeClusterBreakIteratorUtf16(capi::ICU4XGraphemeClusterBreakIteratorUtf16* i) : inner(i) {}
  ICU4XGraphemeClusterBreakIteratorUtf16() = default;
  ICU4XGraphemeClusterBreakIteratorUtf16(ICU4XGraphemeClusterBreakIteratorUtf16&&) noexcept = default;
  ICU4XGraphemeClusterBreakIteratorUtf16& operator=(ICU4XGraphemeClusterBreakIteratorUtf16&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XGraphemeClusterBreakIteratorUtf16, ICU4XGraphemeClusterBreakIteratorUtf16Deleter> inner;
};


inline int32_t ICU4XGraphemeClusterBreakIteratorUtf16::next() {
  return capi::ICU4XGraphemeClusterBreakIteratorUtf16_next(this->inner.get());
}
#endif
