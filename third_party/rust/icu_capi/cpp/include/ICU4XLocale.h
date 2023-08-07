#ifndef ICU4XLocale_H
#define ICU4XLocale_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XLocale ICU4XLocale;
#ifdef __cplusplus
} // namespace capi
#endif
#include "diplomat_result_box_ICU4XLocale_ICU4XError.h"
#include "diplomat_result_void_ICU4XError.h"
#include "ICU4XOrdering.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XLocale_ICU4XError ICU4XLocale_create_from_string(const char* name_data, size_t name_len);

ICU4XLocale* ICU4XLocale_create_und();

ICU4XLocale* ICU4XLocale_clone(const ICU4XLocale* self);

diplomat_result_void_ICU4XError ICU4XLocale_basename(const ICU4XLocale* self, DiplomatWriteable* write);

diplomat_result_void_ICU4XError ICU4XLocale_get_unicode_extension(const ICU4XLocale* self, const char* bytes_data, size_t bytes_len, DiplomatWriteable* write);

diplomat_result_void_ICU4XError ICU4XLocale_language(const ICU4XLocale* self, DiplomatWriteable* write);

diplomat_result_void_ICU4XError ICU4XLocale_set_language(ICU4XLocale* self, const char* bytes_data, size_t bytes_len);

diplomat_result_void_ICU4XError ICU4XLocale_region(const ICU4XLocale* self, DiplomatWriteable* write);

diplomat_result_void_ICU4XError ICU4XLocale_set_region(ICU4XLocale* self, const char* bytes_data, size_t bytes_len);

diplomat_result_void_ICU4XError ICU4XLocale_script(const ICU4XLocale* self, DiplomatWriteable* write);

diplomat_result_void_ICU4XError ICU4XLocale_set_script(ICU4XLocale* self, const char* bytes_data, size_t bytes_len);

diplomat_result_void_ICU4XError ICU4XLocale_canonicalize(const char* bytes_data, size_t bytes_len, DiplomatWriteable* write);

diplomat_result_void_ICU4XError ICU4XLocale_to_string(const ICU4XLocale* self, DiplomatWriteable* write);

bool ICU4XLocale_normalizing_eq(const ICU4XLocale* self, const char* other_data, size_t other_len);

ICU4XOrdering ICU4XLocale_strict_cmp(const ICU4XLocale* self, const char* other_data, size_t other_len);

ICU4XLocale* ICU4XLocale_create_en();

ICU4XLocale* ICU4XLocale_create_bn();
void ICU4XLocale_destroy(ICU4XLocale* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
