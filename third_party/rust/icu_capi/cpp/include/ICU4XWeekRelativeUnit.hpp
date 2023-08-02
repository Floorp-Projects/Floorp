#ifndef ICU4XWeekRelativeUnit_HPP
#define ICU4XWeekRelativeUnit_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XWeekRelativeUnit.h"



/**
 * 
 * 
 * See the [Rust documentation for `RelativeUnit`](https://docs.rs/icu/latest/icu/calendar/week/enum.RelativeUnit.html) for more information.
 */
enum struct ICU4XWeekRelativeUnit {
  Previous = 0,
  Current = 1,
  Next = 2,
};

#endif
