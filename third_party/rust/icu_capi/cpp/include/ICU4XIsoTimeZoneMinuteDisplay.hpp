#ifndef ICU4XIsoTimeZoneMinuteDisplay_HPP
#define ICU4XIsoTimeZoneMinuteDisplay_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XIsoTimeZoneMinuteDisplay.h"



/**
 * 
 * 
 * See the [Rust documentation for `IsoMinutes`](https://docs.rs/icu/latest/icu/datetime/time_zone/enum.IsoMinutes.html) for more information.
 */
enum struct ICU4XIsoTimeZoneMinuteDisplay {
  Required = 0,
  Optional = 1,
};

#endif
