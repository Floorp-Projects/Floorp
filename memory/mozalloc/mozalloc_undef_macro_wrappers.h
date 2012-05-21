/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This header is the "anti-header" for mozalloc_macro_wrappers.h.
 * Including it undefines all the macros defined by
 * mozalloc_macro_wrappers.h.
 */

#ifndef mozilla_mozalloc_macro_wrappers_h
#  error "mozalloc macro wrappers haven't been defined"
#endif

/*
 * This allows the wrappers to be redefined by including
 * mozalloc_macro_wrappers.h again 
 */
#undef mozilla_mozalloc_macro_wrappers_h

#undef free
#undef malloc
#undef calloc
#undef realloc
#undef strdup

#if defined(HAVE_STRNDUP)
#  undef strndup
#endif

#if defined(HAVE_POSIX_MEMALIGN)
#  undef posix_memalign
#endif

#if defined(HAVE_MEMALIGN)
#  undef memalign
#endif

#if defined(HAVE_VALLOC)
#  undef valloc
#endif
