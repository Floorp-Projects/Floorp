#ifndef ICU4XCollatorMaxVariable_H
#define ICU4XCollatorMaxVariable_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XCollatorMaxVariable {
  ICU4XCollatorMaxVariable_Auto = 0,
  ICU4XCollatorMaxVariable_Space = 1,
  ICU4XCollatorMaxVariable_Punctuation = 2,
  ICU4XCollatorMaxVariable_Symbol = 3,
  ICU4XCollatorMaxVariable_Currency = 4,
} ICU4XCollatorMaxVariable;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XCollatorMaxVariable_destroy(ICU4XCollatorMaxVariable* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
