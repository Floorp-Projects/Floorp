#ifndef ICU4XCollatorStrength_HPP
#define ICU4XCollatorStrength_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XCollatorStrength.h"



/**
 * 
 * 
 * See the [Rust documentation for `Strength`](https://docs.rs/icu/latest/icu/collator/enum.Strength.html) for more information.
 */
enum struct ICU4XCollatorStrength {
  Auto = 0,
  Primary = 1,
  Secondary = 2,
  Tertiary = 3,
  Quaternary = 4,
  Identical = 5,
};

#endif
