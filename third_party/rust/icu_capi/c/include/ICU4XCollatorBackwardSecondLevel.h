#ifndef ICU4XCollatorBackwardSecondLevel_H
#define ICU4XCollatorBackwardSecondLevel_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XCollatorBackwardSecondLevel {
  ICU4XCollatorBackwardSecondLevel_Auto = 0,
  ICU4XCollatorBackwardSecondLevel_Off = 1,
  ICU4XCollatorBackwardSecondLevel_On = 2,
} ICU4XCollatorBackwardSecondLevel;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XCollatorBackwardSecondLevel_destroy(ICU4XCollatorBackwardSecondLevel* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
