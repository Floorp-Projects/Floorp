#ifndef ICU4XLineBreakStrictness_H
#define ICU4XLineBreakStrictness_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XLineBreakStrictness {
  ICU4XLineBreakStrictness_Loose = 0,
  ICU4XLineBreakStrictness_Normal = 1,
  ICU4XLineBreakStrictness_Strict = 2,
  ICU4XLineBreakStrictness_Anywhere = 3,
} ICU4XLineBreakStrictness;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XLineBreakStrictness_destroy(ICU4XLineBreakStrictness* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
