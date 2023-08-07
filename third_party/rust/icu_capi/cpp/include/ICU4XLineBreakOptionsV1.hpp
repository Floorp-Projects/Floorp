#ifndef ICU4XLineBreakOptionsV1_HPP
#define ICU4XLineBreakOptionsV1_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XLineBreakOptionsV1.h"

#include "ICU4XLineBreakStrictness.hpp"
#include "ICU4XLineBreakWordOption.hpp"


/**
 * 
 * 
 * See the [Rust documentation for `LineBreakOptions`](https://docs.rs/icu/latest/icu/segmenter/struct.LineBreakOptions.html) for more information.
 */
struct ICU4XLineBreakOptionsV1 {
 public:
  ICU4XLineBreakStrictness strictness;
  ICU4XLineBreakWordOption word_option;
  bool ja_zh;
};


#endif
