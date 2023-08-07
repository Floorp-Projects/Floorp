#ifndef ICU4XBidiDirection_HPP
#define ICU4XBidiDirection_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XBidiDirection.h"


enum struct ICU4XBidiDirection {
  Ltr = 0,
  Rtl = 1,
  Mixed = 2,
};

#endif
