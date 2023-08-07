#ifndef ICU4XDisplayNamesOptionsV1_HPP
#define ICU4XDisplayNamesOptionsV1_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XDisplayNamesOptionsV1.h"

#include "ICU4XDisplayNamesStyle.hpp"
#include "ICU4XDisplayNamesFallback.hpp"
#include "ICU4XLanguageDisplay.hpp"


/**
 * 
 * 
 * See the [Rust documentation for `DisplayNamesOptions`](https://docs.rs/icu/latest/icu/displaynames/options/struct.DisplayNamesOptions.html) for more information.
 */
struct ICU4XDisplayNamesOptionsV1 {
 public:

  /**
   * The optional formatting style to use for display name.
   */
  ICU4XDisplayNamesStyle style;

  /**
   * The fallback return when the system does not have the
   * requested display name, defaults to "code".
   */
  ICU4XDisplayNamesFallback fallback;

  /**
   * The language display kind, defaults to "dialect".
   */
  ICU4XLanguageDisplay language_display;
};


#endif
