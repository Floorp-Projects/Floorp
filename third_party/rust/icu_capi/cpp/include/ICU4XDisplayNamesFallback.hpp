#ifndef ICU4XDisplayNamesFallback_HPP
#define ICU4XDisplayNamesFallback_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XDisplayNamesFallback.h"



/**
 * 
 * 
 * See the [Rust documentation for `Fallback`](https://docs.rs/icu/latest/icu/displaynames/options/enum.Fallback.html) for more information.
 */
enum struct ICU4XDisplayNamesFallback {
  Code = 0,
  None = 1,
};

#endif
