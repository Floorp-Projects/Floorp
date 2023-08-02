#ifndef ICU4XDecomposed_H
#define ICU4XDecomposed_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XDecomposed {
    char32_t first;
    char32_t second;
} ICU4XDecomposed;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XDecomposed_destroy(ICU4XDecomposed* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
