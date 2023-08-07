#ifndef ICU4XLanguageDisplay_HPP
#define ICU4XLanguageDisplay_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XLanguageDisplay.h"



/**
 * 
 * 
 * See the [Rust documentation for `LanguageDisplay`](https://docs.rs/icu/latest/icu/displaynames/options/enum.LanguageDisplay.html) for more information.
 */
enum struct ICU4XLanguageDisplay {
  Dialect = 0,
  Standard = 1,
};

#endif
