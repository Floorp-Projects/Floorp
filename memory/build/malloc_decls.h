/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Helper header to declare all the supported malloc functions.
 * MALLOC_DECL arguments are:
 *   - function name
 *   - return type
 *   - argument types
 */

#ifndef malloc_decls_h
#  define malloc_decls_h

#  ifdef __linux__
typedef void * usable_ptr_t;
#  else
typedef const void * usable_ptr_t;
#  endif
#endif /* malloc_decls_h */

#ifdef MALLOC_DECL
MALLOC_DECL(malloc, void *, size_t)
MALLOC_DECL(posix_memalign, int, void **, size_t, size_t)
MALLOC_DECL(aligned_alloc, void *, size_t, size_t)
MALLOC_DECL(calloc, void *, size_t, size_t)
MALLOC_DECL(realloc, void *, void *, size_t)
MALLOC_DECL(free, void, void *)
MALLOC_DECL(memalign, void *, size_t, size_t)
MALLOC_DECL(valloc, void *, size_t)
MALLOC_DECL(malloc_usable_size, size_t, usable_ptr_t)
MALLOC_DECL(malloc_good_size, size_t, size_t)
#endif /* MALLOC_DECL */

#undef MALLOC_DECL
