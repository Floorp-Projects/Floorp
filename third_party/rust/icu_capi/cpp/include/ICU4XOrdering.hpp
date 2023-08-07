#ifndef ICU4XOrdering_HPP
#define ICU4XOrdering_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XOrdering.h"



/**
 * 
 * 
 * See the [Rust documentation for `Ordering`](https://docs.rs/core/latest/core/cmp/enum.Ordering.html) for more information.
 */
enum struct ICU4XOrdering {
  Less = -1,
  Equal = 0,
  Greater = 1,
};

#endif
