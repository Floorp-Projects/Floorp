#ifndef ICU4XIsoWeekday_H
#define ICU4XIsoWeekday_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XIsoWeekday {
  ICU4XIsoWeekday_Monday = 1,
  ICU4XIsoWeekday_Tuesday = 2,
  ICU4XIsoWeekday_Wednesday = 3,
  ICU4XIsoWeekday_Thursday = 4,
  ICU4XIsoWeekday_Friday = 5,
  ICU4XIsoWeekday_Saturday = 6,
  ICU4XIsoWeekday_Sunday = 7,
} ICU4XIsoWeekday;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XIsoWeekday_destroy(ICU4XIsoWeekday* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
