#ifndef ICU4XCollatorAlternateHandling_H
#define ICU4XCollatorAlternateHandling_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XCollatorAlternateHandling {
  ICU4XCollatorAlternateHandling_Auto = 0,
  ICU4XCollatorAlternateHandling_NonIgnorable = 1,
  ICU4XCollatorAlternateHandling_Shifted = 2,
} ICU4XCollatorAlternateHandling;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XCollatorAlternateHandling_destroy(ICU4XCollatorAlternateHandling* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
