#ifndef ICU4XAnyCalendarKind_HPP
#define ICU4XAnyCalendarKind_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XAnyCalendarKind.h"



/**
 * The various calendar types currently supported by [`ICU4XCalendar`]
 * 
 * See the [Rust documentation for `AnyCalendarKind`](https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendarKind.html) for more information.
 */
enum struct ICU4XAnyCalendarKind {

  /**
   * The kind of an Iso calendar
   */
  Iso = 0,

  /**
   * The kind of a Gregorian calendar
   */
  Gregorian = 1,

  /**
   * The kind of a Buddhist calendar
   */
  Buddhist = 2,

  /**
   * The kind of a Japanese calendar with modern eras
   */
  Japanese = 3,

  /**
   * The kind of a Japanese calendar with modern and historic eras
   */
  JapaneseExtended = 4,

  /**
   * The kind of an Ethiopian calendar, with Amete Mihret era
   */
  Ethiopian = 5,

  /**
   * The kind of an Ethiopian calendar, with Amete Alem era
   */
  EthiopianAmeteAlem = 6,

  /**
   * The kind of a Indian calendar
   */
  Indian = 7,

  /**
   * The kind of a Coptic calendar
   */
  Coptic = 8,
};
class ICU4XLocale;
#include "ICU4XError.hpp"

#include "ICU4XLocale.hpp"
#endif
