#ifndef ICU4XLogger_H
#define ICU4XLogger_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XLogger ICU4XLogger;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

bool ICU4XLogger_init_simple_logger();
void ICU4XLogger_destroy(ICU4XLogger* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
