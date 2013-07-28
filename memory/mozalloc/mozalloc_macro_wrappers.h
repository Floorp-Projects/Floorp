/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mozalloc_macro_wrappers_h
#define mozilla_mozalloc_macro_wrappers_h


/*
 * Make libc "allocating functions" never fail (return NULL).
 *
 * FIXME: use infallible allocators by default after 
 *   http://bugzilla.mozilla.org/show_bug.cgi?id=507249
 * lands.
 */
#define free(_) moz_free(_)

#define malloc(_) moz_malloc(_)

#define calloc(_, __) moz_calloc(_, __)

#define realloc(_, __) moz_realloc(_, __)

/*
 * Note: on some platforms, strdup may be a macro instead of a function.
 * So we have to #undef it to avoid build warnings about redefining it.
 */
#undef strdup
#define strdup(_) moz_strdup(_)

#if defined(HAVE_STRNDUP)
#define strndup(_, __) moz_strndup(_, __)
#endif

#if defined(HAVE_POSIX_MEMALIGN)
#define posix_memalign(_, __, ___) moz_posix_memalign(_, __, ___)
#endif

#if defined(HAVE_MEMALIGN)
#define memalign(_, __) moz_memalign(_, __)
#endif

#if defined(HAVE_VALLOC)
#define valloc(_) moz_valloc(_)
#endif


#endif /* ifndef mozilla_mozalloc_macro_wrappers_h */
