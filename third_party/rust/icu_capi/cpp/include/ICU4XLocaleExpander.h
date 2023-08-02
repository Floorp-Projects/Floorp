#ifndef ICU4XLocaleExpander_H
#define ICU4XLocaleExpander_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XLocaleExpander ICU4XLocaleExpander;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "diplomat_result_box_ICU4XLocaleExpander_ICU4XError.h"
#include "ICU4XLocale.h"
#include "ICU4XTransformResult.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XLocaleExpander_ICU4XError ICU4XLocaleExpander_create(const ICU4XDataProvider* provider);

diplomat_result_box_ICU4XLocaleExpander_ICU4XError ICU4XLocaleExpander_create_extended(const ICU4XDataProvider* provider);

ICU4XTransformResult ICU4XLocaleExpander_maximize(const ICU4XLocaleExpander* self, ICU4XLocale* locale);

ICU4XTransformResult ICU4XLocaleExpander_minimize(const ICU4XLocaleExpander* self, ICU4XLocale* locale);
void ICU4XLocaleExpander_destroy(ICU4XLocaleExpander* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
