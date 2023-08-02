#ifndef ICU4XIsoTimeZoneFormat_HPP
#define ICU4XIsoTimeZoneFormat_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XIsoTimeZoneFormat.h"



/**
 * 
 * 
 * See the [Rust documentation for `IsoFormat`](https://docs.rs/icu/latest/icu/datetime/time_zone/enum.IsoFormat.html) for more information.
 */
enum struct ICU4XIsoTimeZoneFormat {
  Basic = 0,
  Extended = 1,
  UtcBasic = 2,
  UtcExtended = 3,
};

#endif
