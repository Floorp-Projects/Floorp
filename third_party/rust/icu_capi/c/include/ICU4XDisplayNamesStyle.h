#ifndef ICU4XDisplayNamesStyle_H
#define ICU4XDisplayNamesStyle_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XDisplayNamesStyle {
  ICU4XDisplayNamesStyle_Auto = 0,
  ICU4XDisplayNamesStyle_Narrow = 1,
  ICU4XDisplayNamesStyle_Short = 2,
  ICU4XDisplayNamesStyle_Long = 3,
  ICU4XDisplayNamesStyle_Menu = 4,
} ICU4XDisplayNamesStyle;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XDisplayNamesStyle_destroy(ICU4XDisplayNamesStyle* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
