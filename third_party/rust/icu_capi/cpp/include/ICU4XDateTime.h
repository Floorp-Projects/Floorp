#ifndef ICU4XDateTime_H
#define ICU4XDateTime_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XDateTime ICU4XDateTime;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XCalendar.h"
#include "diplomat_result_box_ICU4XDateTime_ICU4XError.h"
#include "ICU4XDate.h"
#include "ICU4XTime.h"
#include "ICU4XIsoDateTime.h"
#include "ICU4XIsoWeekday.h"
#include "ICU4XWeekCalculator.h"
#include "diplomat_result_ICU4XWeekOf_ICU4XError.h"
#include "diplomat_result_void_ICU4XError.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XDateTime_ICU4XError ICU4XDateTime_create_from_iso_in_calendar(int32_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond, const ICU4XCalendar* calendar);

diplomat_result_box_ICU4XDateTime_ICU4XError ICU4XDateTime_create_from_codes_in_calendar(const char* era_code_data, size_t era_code_len, int32_t year, const char* month_code_data, size_t month_code_len, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond, const ICU4XCalendar* calendar);

ICU4XDateTime* ICU4XDateTime_create_from_date_and_time(const ICU4XDate* date, const ICU4XTime* time);

ICU4XDate* ICU4XDateTime_date(const ICU4XDateTime* self);

ICU4XTime* ICU4XDateTime_time(const ICU4XDateTime* self);

ICU4XIsoDateTime* ICU4XDateTime_to_iso(const ICU4XDateTime* self);

ICU4XDateTime* ICU4XDateTime_to_calendar(const ICU4XDateTime* self, const ICU4XCalendar* calendar);

uint8_t ICU4XDateTime_hour(const ICU4XDateTime* self);

uint8_t ICU4XDateTime_minute(const ICU4XDateTime* self);

uint8_t ICU4XDateTime_second(const ICU4XDateTime* self);

uint32_t ICU4XDateTime_nanosecond(const ICU4XDateTime* self);

uint32_t ICU4XDateTime_day_of_month(const ICU4XDateTime* self);

ICU4XIsoWeekday ICU4XDateTime_day_of_week(const ICU4XDateTime* self);

uint32_t ICU4XDateTime_week_of_month(const ICU4XDateTime* self, ICU4XIsoWeekday first_weekday);

diplomat_result_ICU4XWeekOf_ICU4XError ICU4XDateTime_week_of_year(const ICU4XDateTime* self, const ICU4XWeekCalculator* calculator);

uint32_t ICU4XDateTime_ordinal_month(const ICU4XDateTime* self);

diplomat_result_void_ICU4XError ICU4XDateTime_month_code(const ICU4XDateTime* self, DiplomatWriteable* write);

int32_t ICU4XDateTime_year_in_era(const ICU4XDateTime* self);

diplomat_result_void_ICU4XError ICU4XDateTime_era(const ICU4XDateTime* self, DiplomatWriteable* write);

uint8_t ICU4XDateTime_months_in_year(const ICU4XDateTime* self);

uint8_t ICU4XDateTime_days_in_month(const ICU4XDateTime* self);

uint32_t ICU4XDateTime_days_in_year(const ICU4XDateTime* self);

ICU4XCalendar* ICU4XDateTime_calendar(const ICU4XDateTime* self);
void ICU4XDateTime_destroy(ICU4XDateTime* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
