#ifndef ICU4XGeneralCategoryNameToMaskMapper_H
#define ICU4XGeneralCategoryNameToMaskMapper_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XGeneralCategoryNameToMaskMapper ICU4XGeneralCategoryNameToMaskMapper;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "diplomat_result_box_ICU4XGeneralCategoryNameToMaskMapper_ICU4XError.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

uint32_t ICU4XGeneralCategoryNameToMaskMapper_get_strict(const ICU4XGeneralCategoryNameToMaskMapper* self, const char* name_data, size_t name_len);

uint32_t ICU4XGeneralCategoryNameToMaskMapper_get_loose(const ICU4XGeneralCategoryNameToMaskMapper* self, const char* name_data, size_t name_len);

diplomat_result_box_ICU4XGeneralCategoryNameToMaskMapper_ICU4XError ICU4XGeneralCategoryNameToMaskMapper_load(const ICU4XDataProvider* provider);
void ICU4XGeneralCategoryNameToMaskMapper_destroy(ICU4XGeneralCategoryNameToMaskMapper* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
