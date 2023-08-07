#ifndef ICU4XDisplayNamesFallback_H
#define ICU4XDisplayNamesFallback_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XDisplayNamesFallback {
  ICU4XDisplayNamesFallback_Code = 0,
  ICU4XDisplayNamesFallback_None = 1,
} ICU4XDisplayNamesFallback;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XDisplayNamesFallback_destroy(ICU4XDisplayNamesFallback* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
