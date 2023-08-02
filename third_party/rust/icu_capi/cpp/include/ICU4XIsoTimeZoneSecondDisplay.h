#ifndef ICU4XIsoTimeZoneSecondDisplay_H
#define ICU4XIsoTimeZoneSecondDisplay_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XIsoTimeZoneSecondDisplay {
  ICU4XIsoTimeZoneSecondDisplay_Optional = 0,
  ICU4XIsoTimeZoneSecondDisplay_Never = 1,
} ICU4XIsoTimeZoneSecondDisplay;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XIsoTimeZoneSecondDisplay_destroy(ICU4XIsoTimeZoneSecondDisplay* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
