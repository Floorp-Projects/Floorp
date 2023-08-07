#ifndef ICU4XWeekRelativeUnit_H
#define ICU4XWeekRelativeUnit_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XWeekRelativeUnit {
  ICU4XWeekRelativeUnit_Previous = 0,
  ICU4XWeekRelativeUnit_Current = 1,
  ICU4XWeekRelativeUnit_Next = 2,
} ICU4XWeekRelativeUnit;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XWeekRelativeUnit_destroy(ICU4XWeekRelativeUnit* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
