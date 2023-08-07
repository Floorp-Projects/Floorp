#ifndef ICU4XRegionDisplayNames_H
#define ICU4XRegionDisplayNames_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XRegionDisplayNames ICU4XRegionDisplayNames;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "ICU4XLocale.h"
#include "diplomat_result_box_ICU4XRegionDisplayNames_ICU4XError.h"
#include "diplomat_result_void_ICU4XError.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XRegionDisplayNames_ICU4XError ICU4XRegionDisplayNames_try_new_unstable(const ICU4XDataProvider* provider, const ICU4XLocale* locale);

diplomat_result_void_ICU4XError ICU4XRegionDisplayNames_of(const ICU4XRegionDisplayNames* self, const char* region_data, size_t region_len, DiplomatWriteable* write);
void ICU4XRegionDisplayNames_destroy(ICU4XRegionDisplayNames* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
