#ifndef ICU4XGraphemeClusterBreakIteratorLatin1_H
#define ICU4XGraphemeClusterBreakIteratorLatin1_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XGraphemeClusterBreakIteratorLatin1 ICU4XGraphemeClusterBreakIteratorLatin1;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

int32_t ICU4XGraphemeClusterBreakIteratorLatin1_next(ICU4XGraphemeClusterBreakIteratorLatin1* self);
void ICU4XGraphemeClusterBreakIteratorLatin1_destroy(ICU4XGraphemeClusterBreakIteratorLatin1* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
