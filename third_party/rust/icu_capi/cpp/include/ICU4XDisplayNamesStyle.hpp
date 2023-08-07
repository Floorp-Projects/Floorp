#ifndef ICU4XDisplayNamesStyle_HPP
#define ICU4XDisplayNamesStyle_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XDisplayNamesStyle.h"



/**
 * 
 * 
 * See the [Rust documentation for `Style`](https://docs.rs/icu/latest/icu/displaynames/options/enum.Style.html) for more information.
 */
enum struct ICU4XDisplayNamesStyle {
  Auto = 0,
  Narrow = 1,
  Short = 2,
  Long = 3,
  Menu = 4,
};

#endif
