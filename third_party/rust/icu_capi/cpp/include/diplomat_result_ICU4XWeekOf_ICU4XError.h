#ifndef diplomat_result_ICU4XWeekOf_ICU4XError_H
#define diplomat_result_ICU4XWeekOf_ICU4XError_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "ICU4XWeekOf.h"
#include "ICU4XError.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif
typedef struct diplomat_result_ICU4XWeekOf_ICU4XError {
    union {
        ICU4XWeekOf ok;
        ICU4XError err;
    };
    bool is_ok;
} diplomat_result_ICU4XWeekOf_ICU4XError;
#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
