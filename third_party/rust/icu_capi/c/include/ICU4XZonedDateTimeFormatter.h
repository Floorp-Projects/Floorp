#ifndef ICU4XZonedDateTimeFormatter_H
#define ICU4XZonedDateTimeFormatter_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XZonedDateTimeFormatter ICU4XZonedDateTimeFormatter;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "ICU4XLocale.h"
#include "ICU4XDateLength.h"
#include "ICU4XTimeLength.h"
#include "diplomat_result_box_ICU4XZonedDateTimeFormatter_ICU4XError.h"
#include "ICU4XIsoTimeZoneOptions.h"
#include "ICU4XDateTime.h"
#include "ICU4XCustomTimeZone.h"
#include "diplomat_result_void_ICU4XError.h"
#include "ICU4XIsoDateTime.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XZonedDateTimeFormatter_ICU4XError ICU4XZonedDateTimeFormatter_create_with_lengths(const ICU4XDataProvider* provider, const ICU4XLocale* locale, ICU4XDateLength date_length, ICU4XTimeLength time_length);

diplomat_result_box_ICU4XZonedDateTimeFormatter_ICU4XError ICU4XZonedDateTimeFormatter_create_with_lengths_and_iso_8601_time_zone_fallback(const ICU4XDataProvider* provider, const ICU4XLocale* locale, ICU4XDateLength date_length, ICU4XTimeLength time_length, ICU4XIsoTimeZoneOptions zone_options);

diplomat_result_void_ICU4XError ICU4XZonedDateTimeFormatter_format_datetime_with_custom_time_zone(const ICU4XZonedDateTimeFormatter* self, const ICU4XDateTime* datetime, const ICU4XCustomTimeZone* time_zone, DiplomatWriteable* write);

diplomat_result_void_ICU4XError ICU4XZonedDateTimeFormatter_format_iso_datetime_with_custom_time_zone(const ICU4XZonedDateTimeFormatter* self, const ICU4XIsoDateTime* datetime, const ICU4XCustomTimeZone* time_zone, DiplomatWriteable* write);
void ICU4XZonedDateTimeFormatter_destroy(ICU4XZonedDateTimeFormatter* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
