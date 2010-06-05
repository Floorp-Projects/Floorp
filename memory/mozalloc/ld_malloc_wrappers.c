/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brad Lassey <blassey@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stddef.h>             // for size_t
#include <stdlib.h>             // for malloc, free
#include "mozalloc.h"
#include <malloc.h>
     
#ifdef __malloc_hook
static void* moz_malloc_hook(size_t size, const void *caller)
{
  return moz_malloc(size);
}

static void* moz_realloc_hook(void *ptr, size_t size, const void *caller)
{
  return moz_realloc(ptr, size);
}

static void moz_free_hook(void *ptr, const void *caller)
{
  moz_free(ptr);
}

static void* moz_memalign_hook(size_t align, size_t size, const void *caller)
{
  return moz_memalign(align, size);
}

static void
moz_malloc_init_hook (void)
{
  __malloc_hook = moz_malloc_hook;
  __realloc_hook = moz_realloc_hook;
  __free_hook = moz_free_hook;
  __memalign_hook = moz_memalign_hook;
}

/* Override initializing hook from the C library. */
void (*__malloc_initialize_hook) (void) = moz_malloc_init_hook;
#endif

inline void  __wrap_free(void* ptr)
{
  moz_free(ptr);
}

inline void* __wrap_malloc(size_t size)
{
  return moz_malloc(size);
}

inline void* __wrap_realloc(void* ptr, size_t size)
{
  return moz_realloc(ptr, size);
}

inline void* __wrap_calloc(size_t num, size_t size)
{
  return moz_calloc(num, size);
}

inline void* __wrap_valloc(size_t size)
{
  return moz_valloc(size);
}

inline void* __wrap_memalign(size_t align, size_t size)
{
  return moz_memalign(align, size);
}

inline char* __wrap_strdup(char* str)
{
  return moz_strdup(str);
}

inline char* __wrap_strndup(const char* str, size_t size) {
  return moz_strndup(str, size);
}


inline void  __wrap_PR_Free(void* ptr)
{
  moz_free(ptr);
}

inline void* __wrap_PR_Malloc(size_t size)
{
  return moz_malloc(size);
}

inline void* __wrap_PR_Realloc(void* ptr, size_t size)
{
  return moz_realloc(ptr, size);
}

inline void* __wrap_PR_Calloc(size_t num, size_t size)
{
  return moz_calloc(num, size);
}

inline int __wrap_posix_memalign(void **memptr, size_t alignment, size_t size)
{
  return moz_posix_memalign(memptr, alignment, size);
}
