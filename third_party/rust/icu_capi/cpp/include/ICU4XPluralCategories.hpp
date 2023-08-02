#ifndef ICU4XPluralCategories_HPP
#define ICU4XPluralCategories_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XPluralCategories.h"



/**
 * FFI version of `PluralRules::categories()` data.
 */
struct ICU4XPluralCategories {
 public:
  bool zero;
  bool one;
  bool two;
  bool few;
  bool many;
  bool other;
};


#endif
