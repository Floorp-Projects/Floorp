/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include "mozilla/Types.h"

#ifdef ANDROID
#define wrap(a) __wrap_ ## a
#elif defined(XP_WIN) || defined(XP_MACOSX)
#define wrap(a) je_ ## a
#endif

#ifdef wrap
void *wrap(malloc)(size_t);
void wrap(free)(void *);
#endif

#ifdef ANDROID
/* operator new(unsigned int) */
MOZ_EXPORT_API(void *)
wrap(_Znwj)(unsigned int size)
{
  return wrap(malloc)(size);
}
/* operator new[](unsigned int) */
MOZ_EXPORT_API(void *)
wrap(_Znaj)(unsigned int size)
{
  return wrap(malloc)(size);
}
/* operator delete(void*) */
MOZ_EXPORT_API(void)
wrap(_ZdlPv)(void *ptr)
{
  wrap(free)(ptr);
}
/* operator delete[](void*) */
MOZ_EXPORT_API(void)
wrap(_ZdaPv)(void *ptr)
{
  wrap(free)(ptr);
}
#endif

#ifdef wrap
MOZ_EXPORT_API(char *)
wrap(strndup)(const char *src, size_t len)
{
  char* dst = (char*) wrap(malloc)(len + 1);
  if (dst)
    strncpy(dst, src, len + 1);
  return dst; 
}

MOZ_EXPORT_API(char *)
wrap(strdup)(const char *src)
{
  size_t len = strlen(src);
  return wrap(strndup)(src, len);
}

#ifdef XP_WIN
/*
 *  There's a fun allocator mismatch in (at least) the VS 2010 CRT
 *  (see the giant comment in this directory's Makefile.in
 *  that gets redirected here to avoid a crash on shutdown.
 */
void
wrap(dumb_free_thunk)(void *ptr)
{
  return; /* shutdown leaks that we don't care about */
}

#include <wchar.h>

/*
 *  We also need to provide our own impl of wcsdup so that we don't ask
 *  the CRT for memory from its heap (which will then be unfreeable).
 */
wchar_t *
wrap(wcsdup)(const wchar_t *src)
{
  size_t len = wcslen(src);
  wchar_t *dst = (wchar_t*) wrap(malloc)((len + 1) * sizeof(wchar_t));
  if (dst)
    wcsncpy(dst, src, len + 1);
  return dst;
}
#endif /* XP_WIN */

#endif

#ifdef MOZ_JEMALLOC
/* Override some jemalloc defaults */
const char *je_malloc_conf = "narenas:1,lg_chunk:20";

#ifdef ANDROID
#include <android/log.h>

static void
_je_malloc_message(void *cbopaque, const char *s)
{
  __android_log_print(ANDROID_LOG_INFO, "GeckoJemalloc", "%s", s);
}

void (*je_malloc_message)(void *, const char *s) = _je_malloc_message;
#endif
#endif /* MOZ_JEMALLOC */

#include <mozilla/Assertions.h>

void moz_abort() {
  MOZ_CRASH();
}
