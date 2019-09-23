/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DAV1D_COMMON_MEM_H
#define DAV1D_COMMON_MEM_H

#include <stdlib.h>

#if defined(HAVE_ALIGNED_MALLOC) || defined(HAVE_MEMALIGN)
#include <malloc.h>
#endif

#include "common/attributes.h"

/*
 * Allocate 32-byte aligned memory. The return value can be released
 * by calling the standard free() function.
 */
static inline void *dav1d_alloc_aligned(size_t sz, size_t align) {
#ifdef HAVE_POSIX_MEMALIGN
    void *ptr;
    assert(!(align & (align - 1)));
    if (posix_memalign(&ptr, align, sz)) return NULL;
    return ptr;
#elif defined(HAVE_ALIGNED_MALLOC)
    return _aligned_malloc(sz, align);
#elif defined(HAVE_MEMALIGN)
    return memalign(align, sz);
#else
#error Missing aligned alloc implementation
#endif
}

static inline void dav1d_free_aligned(void* ptr) {
#ifdef HAVE_POSIX_MEMALIGN
    free(ptr);
#elif defined(HAVE_ALIGNED_MALLOC)
    _aligned_free(ptr);
#elif defined(HAVE_MEMALIGN)
    free(ptr);
#endif
}

static inline void dav1d_freep_aligned(void* ptr) {
    void **mem = (void **) ptr;
    if (*mem) {
        dav1d_free_aligned(*mem);
        *mem = NULL;
    }
}

static inline void freep(void *ptr) {
    void **mem = (void **) ptr;
    if (*mem) {
        free(*mem);
        *mem = NULL;
    }
}

#endif /* DAV1D_COMMON_MEM_H */
