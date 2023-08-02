#ifndef ICU4XLocaleFallbacker_H
#define ICU4XLocaleFallbacker_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XLocaleFallbacker ICU4XLocaleFallbacker;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "diplomat_result_box_ICU4XLocaleFallbacker_ICU4XError.h"
#include "ICU4XLocaleFallbackConfig.h"
#include "diplomat_result_box_ICU4XLocaleFallbackerWithConfig_ICU4XError.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XLocaleFallbacker_ICU4XError ICU4XLocaleFallbacker_create(const ICU4XDataProvider* provider);

ICU4XLocaleFallbacker* ICU4XLocaleFallbacker_create_without_data();

diplomat_result_box_ICU4XLocaleFallbackerWithConfig_ICU4XError ICU4XLocaleFallbacker_for_config(const ICU4XLocaleFallbacker* self, ICU4XLocaleFallbackConfig config);
void ICU4XLocaleFallbacker_destroy(ICU4XLocaleFallbacker* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
