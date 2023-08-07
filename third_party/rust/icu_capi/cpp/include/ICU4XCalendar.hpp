#ifndef ICU4XCalendar_HPP
#define ICU4XCalendar_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XCalendar.h"

class ICU4XDataProvider;
class ICU4XLocale;
class ICU4XCalendar;
#include "ICU4XError.hpp"
#include "ICU4XAnyCalendarKind.hpp"

/**
 * A destruction policy for using ICU4XCalendar with std::unique_ptr.
 */
struct ICU4XCalendarDeleter {
  void operator()(capi::ICU4XCalendar* l) const noexcept {
    capi::ICU4XCalendar_destroy(l);
  }
};

/**
 * 
 * 
 * See the [Rust documentation for `AnyCalendar`](https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendar.html) for more information.
 */
class ICU4XCalendar {
 public:

  /**
   * Creates a new [`ICU4XCalendar`] from the specified date and time.
   * 
   * See the [Rust documentation for `try_new_for_locale_unstable`](https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendar.html#method.try_new_for_locale_unstable) for more information.
   */
  static diplomat::result<ICU4XCalendar, ICU4XError> create_for_locale(const ICU4XDataProvider& provider, const ICU4XLocale& locale);

  /**
   * Creates a new [`ICU4XCalendar`] from the specified date and time.
   * 
   * See the [Rust documentation for `try_new_unstable`](https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendar.html#method.try_new_unstable) for more information.
   */
  static diplomat::result<ICU4XCalendar, ICU4XError> create_for_kind(const ICU4XDataProvider& provider, ICU4XAnyCalendarKind kind);

  /**
   * Returns the kind of this calendar
   * 
   * See the [Rust documentation for `kind`](https://docs.rs/icu/latest/icu/calendar/enum.AnyCalendar.html#method.kind) for more information.
   */
  ICU4XAnyCalendarKind kind() const;
  inline const capi::ICU4XCalendar* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XCalendar* AsFFIMut() { return this->inner.get(); }
  inline ICU4XCalendar(capi::ICU4XCalendar* i) : inner(i) {}
  ICU4XCalendar() = default;
  ICU4XCalendar(ICU4XCalendar&&) noexcept = default;
  ICU4XCalendar& operator=(ICU4XCalendar&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XCalendar, ICU4XCalendarDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XLocale.hpp"

inline diplomat::result<ICU4XCalendar, ICU4XError> ICU4XCalendar::create_for_locale(const ICU4XDataProvider& provider, const ICU4XLocale& locale) {
  auto diplomat_result_raw_out_value = capi::ICU4XCalendar_create_for_locale(provider.AsFFI(), locale.AsFFI());
  diplomat::result<ICU4XCalendar, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XCalendar>(std::move(ICU4XCalendar(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<ICU4XCalendar, ICU4XError> ICU4XCalendar::create_for_kind(const ICU4XDataProvider& provider, ICU4XAnyCalendarKind kind) {
  auto diplomat_result_raw_out_value = capi::ICU4XCalendar_create_for_kind(provider.AsFFI(), static_cast<capi::ICU4XAnyCalendarKind>(kind));
  diplomat::result<ICU4XCalendar, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XCalendar>(std::move(ICU4XCalendar(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline ICU4XAnyCalendarKind ICU4XCalendar::kind() const {
  return static_cast<ICU4XAnyCalendarKind>(capi::ICU4XCalendar_kind(this->inner.get()));
}
#endif
