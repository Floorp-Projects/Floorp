#ifndef ICU4XTransformResult_HPP
#define ICU4XTransformResult_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XTransformResult.h"



/**
 * FFI version of `TransformResult`.
 * 
 * See the [Rust documentation for `TransformResult`](https://docs.rs/icu/latest/icu/locid_transform/enum.TransformResult.html) for more information.
 */
enum struct ICU4XTransformResult {
  Modified = 0,
  Unmodified = 1,
};

#endif
