#ifndef ICU4XIsoTimeZoneMinuteDisplay_H
#define ICU4XIsoTimeZoneMinuteDisplay_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XIsoTimeZoneMinuteDisplay {
  ICU4XIsoTimeZoneMinuteDisplay_Required = 0,
  ICU4XIsoTimeZoneMinuteDisplay_Optional = 1,
} ICU4XIsoTimeZoneMinuteDisplay;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XIsoTimeZoneMinuteDisplay_destroy(ICU4XIsoTimeZoneMinuteDisplay* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
