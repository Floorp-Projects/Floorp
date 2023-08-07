#ifndef ICU4XLocaleDisplayNamesFormatter_H
#define ICU4XLocaleDisplayNamesFormatter_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XLocaleDisplayNamesFormatter ICU4XLocaleDisplayNamesFormatter;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "ICU4XLocale.h"
#include "ICU4XDisplayNamesOptionsV1.h"
#include "diplomat_result_box_ICU4XLocaleDisplayNamesFormatter_ICU4XError.h"
#include "diplomat_result_void_ICU4XError.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XLocaleDisplayNamesFormatter_ICU4XError ICU4XLocaleDisplayNamesFormatter_try_new_unstable(const ICU4XDataProvider* provider, const ICU4XLocale* locale, ICU4XDisplayNamesOptionsV1 options);

diplomat_result_void_ICU4XError ICU4XLocaleDisplayNamesFormatter_of(const ICU4XLocaleDisplayNamesFormatter* self, const ICU4XLocale* locale, DiplomatWriteable* write);
void ICU4XLocaleDisplayNamesFormatter_destroy(ICU4XLocaleDisplayNamesFormatter* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
