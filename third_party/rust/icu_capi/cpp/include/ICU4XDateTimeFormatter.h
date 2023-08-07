#ifndef ICU4XDateTimeFormatter_H
#define ICU4XDateTimeFormatter_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XDateTimeFormatter ICU4XDateTimeFormatter;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "ICU4XLocale.h"
#include "ICU4XDateLength.h"
#include "ICU4XTimeLength.h"
#include "diplomat_result_box_ICU4XDateTimeFormatter_ICU4XError.h"
#include "ICU4XDateTime.h"
#include "diplomat_result_void_ICU4XError.h"
#include "ICU4XIsoDateTime.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XDateTimeFormatter_ICU4XError ICU4XDateTimeFormatter_create_with_lengths(const ICU4XDataProvider* provider, const ICU4XLocale* locale, ICU4XDateLength date_length, ICU4XTimeLength time_length);

diplomat_result_void_ICU4XError ICU4XDateTimeFormatter_format_datetime(const ICU4XDateTimeFormatter* self, const ICU4XDateTime* value, DiplomatWriteable* write);

diplomat_result_void_ICU4XError ICU4XDateTimeFormatter_format_iso_datetime(const ICU4XDateTimeFormatter* self, const ICU4XIsoDateTime* value, DiplomatWriteable* write);
void ICU4XDateTimeFormatter_destroy(ICU4XDateTimeFormatter* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
