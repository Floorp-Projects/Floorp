#ifndef ICU4XTime_H
#define ICU4XTime_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XTime ICU4XTime;
#ifdef __cplusplus
} // namespace capi
#endif
#include "diplomat_result_box_ICU4XTime_ICU4XError.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XTime_ICU4XError ICU4XTime_create(uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond);

uint8_t ICU4XTime_hour(const ICU4XTime* self);

uint8_t ICU4XTime_minute(const ICU4XTime* self);

uint8_t ICU4XTime_second(const ICU4XTime* self);

uint32_t ICU4XTime_nanosecond(const ICU4XTime* self);
void ICU4XTime_destroy(ICU4XTime* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
