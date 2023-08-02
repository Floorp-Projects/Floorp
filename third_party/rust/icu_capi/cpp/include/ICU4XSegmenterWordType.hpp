#ifndef ICU4XSegmenterWordType_HPP
#define ICU4XSegmenterWordType_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XSegmenterWordType.h"



/**
 * 
 * 
 * See the [Rust documentation for `WordType`](https://docs.rs/icu/latest/icu/segmenter/enum.WordType.html) for more information.
 */
enum struct ICU4XSegmenterWordType {
  None = 0,
  Number = 1,
  Letter = 2,
};

#endif
