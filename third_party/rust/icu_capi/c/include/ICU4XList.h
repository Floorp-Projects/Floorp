#ifndef ICU4XList_H
#define ICU4XList_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XList ICU4XList;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

ICU4XList* ICU4XList_create();

ICU4XList* ICU4XList_create_with_capacity(size_t capacity);

void ICU4XList_push(ICU4XList* self, const char* val_data, size_t val_len);

size_t ICU4XList_len(const ICU4XList* self);
void ICU4XList_destroy(ICU4XList* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
