#ifndef ICU4XTimeLength_H
#define ICU4XTimeLength_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XTimeLength {
  ICU4XTimeLength_Full = 0,
  ICU4XTimeLength_Long = 1,
  ICU4XTimeLength_Medium = 2,
  ICU4XTimeLength_Short = 3,
} ICU4XTimeLength;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XTimeLength_destroy(ICU4XTimeLength* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
