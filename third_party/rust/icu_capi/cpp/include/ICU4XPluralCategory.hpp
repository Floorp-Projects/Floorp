#ifndef ICU4XPluralCategory_HPP
#define ICU4XPluralCategory_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XPluralCategory.h"



/**
 * FFI version of `PluralCategory`.
 * 
 * See the [Rust documentation for `PluralCategory`](https://docs.rs/icu/latest/icu/plurals/enum.PluralCategory.html) for more information.
 */
enum struct ICU4XPluralCategory {
  Zero = 0,
  One = 1,
  Two = 2,
  Few = 3,
  Many = 4,
  Other = 5,
};

#endif
