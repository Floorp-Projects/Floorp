#ifndef ICU4XLocaleFallbackPriority_HPP
#define ICU4XLocaleFallbackPriority_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XLocaleFallbackPriority.h"



/**
 * Priority mode for the ICU4X fallback algorithm.
 * 
 * See the [Rust documentation for `FallbackPriority`](https://docs.rs/icu_provider/latest/icu_provider/enum.FallbackPriority.html) for more information.
 */
enum struct ICU4XLocaleFallbackPriority {
  Language = 0,
  Region = 1,
  Collation = 2,
};

#endif
