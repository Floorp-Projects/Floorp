#ifndef ICU4XLineBreakIteratorLatin1_HPP
#define ICU4XLineBreakIteratorLatin1_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XLineBreakIteratorLatin1.h"


/**
 * A destruction policy for using ICU4XLineBreakIteratorLatin1 with std::unique_ptr.
 */
struct ICU4XLineBreakIteratorLatin1Deleter {
  void operator()(capi::ICU4XLineBreakIteratorLatin1* l) const noexcept {
    capi::ICU4XLineBreakIteratorLatin1_destroy(l);
  }
};

/**
 * 
 * 
 * See the [Rust documentation for `LineBreakIterator`](https://docs.rs/icu/latest/icu/segmenter/struct.LineBreakIterator.html) for more information.
 * 
 *  Additional information: [1](https://docs.rs/icu/latest/icu/segmenter/type.LineBreakIteratorLatin1.html)
 */
class ICU4XLineBreakIteratorLatin1 {
 public:

  /**
   * Finds the next breakpoint. Returns -1 if at the end of the string or if the index is
   * out of range of a 32-bit signed integer.
   * 
   * See the [Rust documentation for `next`](https://docs.rs/icu/latest/icu/segmenter/struct.LineBreakIterator.html#method.next) for more information.
   */
  int32_t next();
  inline const capi::ICU4XLineBreakIteratorLatin1* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XLineBreakIteratorLatin1* AsFFIMut() { return this->inner.get(); }
  inline ICU4XLineBreakIteratorLatin1(capi::ICU4XLineBreakIteratorLatin1* i) : inner(i) {}
  ICU4XLineBreakIteratorLatin1() = default;
  ICU4XLineBreakIteratorLatin1(ICU4XLineBreakIteratorLatin1&&) noexcept = default;
  ICU4XLineBreakIteratorLatin1& operator=(ICU4XLineBreakIteratorLatin1&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XLineBreakIteratorLatin1, ICU4XLineBreakIteratorLatin1Deleter> inner;
};


inline int32_t ICU4XLineBreakIteratorLatin1::next() {
  return capi::ICU4XLineBreakIteratorLatin1_next(this->inner.get());
}
#endif
