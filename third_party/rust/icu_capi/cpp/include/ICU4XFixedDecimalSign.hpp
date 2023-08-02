#ifndef ICU4XFixedDecimalSign_HPP
#define ICU4XFixedDecimalSign_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XFixedDecimalSign.h"



/**
 * The sign of a FixedDecimal, as shown in formatting.
 * 
 * See the [Rust documentation for `Sign`](https://docs.rs/fixed_decimal/latest/fixed_decimal/enum.Sign.html) for more information.
 */
enum struct ICU4XFixedDecimalSign {

  /**
   * No sign (implicitly positive, e.g., 1729).
   */
  None = 0,

  /**
   * A negative sign, e.g., -1729.
   */
  Negative = 1,

  /**
   * An explicit positive sign, e.g., +1729.
   */
  Positive = 2,
};

#endif
