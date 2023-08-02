#ifndef ICU4XCollatorAlternateHandling_HPP
#define ICU4XCollatorAlternateHandling_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XCollatorAlternateHandling.h"



/**
 * 
 * 
 * See the [Rust documentation for `AlternateHandling`](https://docs.rs/icu/latest/icu/collator/enum.AlternateHandling.html) for more information.
 */
enum struct ICU4XCollatorAlternateHandling {
  Auto = 0,
  NonIgnorable = 1,
  Shifted = 2,
};

#endif
