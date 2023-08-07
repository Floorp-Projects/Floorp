#ifndef ICU4XListLength_HPP
#define ICU4XListLength_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XListLength.h"



/**
 * 
 * 
 * See the [Rust documentation for `ListLength`](https://docs.rs/icu/latest/icu/list/enum.ListLength.html) for more information.
 */
enum struct ICU4XListLength {
  Wide = 0,
  Short = 1,
  Narrow = 2,
};

#endif
