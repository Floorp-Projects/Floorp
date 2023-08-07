#ifndef ICU4XWeekOf_H
#define ICU4XWeekOf_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "ICU4XWeekRelativeUnit.h"
#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XWeekOf {
    uint16_t week;
    ICU4XWeekRelativeUnit unit;
} ICU4XWeekOf;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XWeekRelativeUnit.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XWeekOf_destroy(ICU4XWeekOf* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
