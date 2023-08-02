#ifndef ICU4XTime_HPP
#define ICU4XTime_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XTime.h"

class ICU4XTime;
#include "ICU4XError.hpp"

/**
 * A destruction policy for using ICU4XTime with std::unique_ptr.
 */
struct ICU4XTimeDeleter {
  void operator()(capi::ICU4XTime* l) const noexcept {
    capi::ICU4XTime_destroy(l);
  }
};

/**
 * An ICU4X Time object representing a time in terms of hour, minute, second, nanosecond
 * 
 * See the [Rust documentation for `Time`](https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html) for more information.
 */
class ICU4XTime {
 public:

  /**
   * Creates a new [`ICU4XTime`] given field values
   * 
   * See the [Rust documentation for `Time`](https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html) for more information.
   */
  static diplomat::result<ICU4XTime, ICU4XError> create(uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond);

  /**
   * Returns the hour in this time
   * 
   * See the [Rust documentation for `hour`](https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html#structfield.hour) for more information.
   */
  uint8_t hour() const;

  /**
   * Returns the minute in this time
   * 
   * See the [Rust documentation for `minute`](https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html#structfield.minute) for more information.
   */
  uint8_t minute() const;

  /**
   * Returns the second in this time
   * 
   * See the [Rust documentation for `second`](https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html#structfield.second) for more information.
   */
  uint8_t second() const;

  /**
   * Returns the nanosecond in this time
   * 
   * See the [Rust documentation for `nanosecond`](https://docs.rs/icu/latest/icu/calendar/types/struct.Time.html#structfield.nanosecond) for more information.
   */
  uint32_t nanosecond() const;
  inline const capi::ICU4XTime* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XTime* AsFFIMut() { return this->inner.get(); }
  inline ICU4XTime(capi::ICU4XTime* i) : inner(i) {}
  ICU4XTime() = default;
  ICU4XTime(ICU4XTime&&) noexcept = default;
  ICU4XTime& operator=(ICU4XTime&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XTime, ICU4XTimeDeleter> inner;
};


inline diplomat::result<ICU4XTime, ICU4XError> ICU4XTime::create(uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond) {
  auto diplomat_result_raw_out_value = capi::ICU4XTime_create(hour, minute, second, nanosecond);
  diplomat::result<ICU4XTime, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XTime>(std::move(ICU4XTime(diplomat_result_raw_out_value.ok)));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(std::move(static_cast<ICU4XError>(diplomat_result_raw_out_value.err)));
  }
  return diplomat_result_out_value;
}
inline uint8_t ICU4XTime::hour() const {
  return capi::ICU4XTime_hour(this->inner.get());
}
inline uint8_t ICU4XTime::minute() const {
  return capi::ICU4XTime_minute(this->inner.get());
}
inline uint8_t ICU4XTime::second() const {
  return capi::ICU4XTime_second(this->inner.get());
}
inline uint32_t ICU4XTime::nanosecond() const {
  return capi::ICU4XTime_nanosecond(this->inner.get());
}
#endif
