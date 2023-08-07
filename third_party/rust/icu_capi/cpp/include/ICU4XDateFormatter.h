#ifndef ICU4XDateFormatter_H
#define ICU4XDateFormatter_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XDateFormatter ICU4XDateFormatter;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "ICU4XLocale.h"
#include "ICU4XDateLength.h"
#include "diplomat_result_box_ICU4XDateFormatter_ICU4XError.h"
#include "ICU4XDate.h"
#include "diplomat_result_void_ICU4XError.h"
#include "ICU4XIsoDate.h"
#include "ICU4XDateTime.h"
#include "ICU4XIsoDateTime.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XDateFormatter_ICU4XError ICU4XDateFormatter_create_with_length(const ICU4XDataProvider* provider, const ICU4XLocale* locale, ICU4XDateLength date_length);

diplomat_result_void_ICU4XError ICU4XDateFormatter_format_date(const ICU4XDateFormatter* self, const ICU4XDate* value, DiplomatWriteable* write);

diplomat_result_void_ICU4XError ICU4XDateFormatter_format_iso_date(const ICU4XDateFormatter* self, const ICU4XIsoDate* value, DiplomatWriteable* write);

diplomat_result_void_ICU4XError ICU4XDateFormatter_format_datetime(const ICU4XDateFormatter* self, const ICU4XDateTime* value, DiplomatWriteable* write);

diplomat_result_void_ICU4XError ICU4XDateFormatter_format_iso_datetime(const ICU4XDateFormatter* self, const ICU4XIsoDateTime* value, DiplomatWriteable* write);
void ICU4XDateFormatter_destroy(ICU4XDateFormatter* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
