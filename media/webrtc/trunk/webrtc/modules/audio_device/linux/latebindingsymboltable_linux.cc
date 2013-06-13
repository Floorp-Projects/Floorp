/*
 * libjingle
 * Copyright 2004--2010, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "latebindingsymboltable_linux.h"

#if defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)
#include <dlfcn.h>
#endif

// TODO(grunell): Either put inside webrtc namespace or use webrtc:: instead.
using namespace webrtc;

namespace webrtc_adm_linux {

inline static const char *GetDllError() {
#if defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)
  char *err = dlerror();
  if (err) {
    return err;
  } else {
    return "No error";
  }
#else
#error Not implemented
#endif
}

DllHandle InternalLoadDll(const char dll_name[]) {
#if defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)
  DllHandle handle = dlopen(dll_name, RTLD_NOW);
#else
#error Not implemented
#endif
  if (handle == kInvalidDllHandle) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, -1,
               "Can't load %s : %d", dll_name, GetDllError());
  }
  return handle;
}

void InternalUnloadDll(DllHandle handle) {
#if defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)
  if (dlclose(handle) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, -1,
               "%d", GetDllError());
  }
#else
#error Not implemented
#endif
}

static bool LoadSymbol(DllHandle handle,
                       const char *symbol_name,
                       void **symbol) {
#if defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)
  *symbol = dlsym(handle, symbol_name);
  char *err = dlerror();
  if (err) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, -1,
               "Error loading symbol %s : %d", symbol_name, err);
    return false;
  } else if (!*symbol) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, -1,
               "Symbol %s is NULL", symbol_name);
    return false;
  }
  return true;
#else
#error Not implemented
#endif
}

// This routine MUST assign SOME value for every symbol, even if that value is
// NULL, or else some symbols may be left with uninitialized data that the
// caller may later interpret as a valid address.
bool InternalLoadSymbols(DllHandle handle,
                         int num_symbols,
                         const char *const symbol_names[],
                         void *symbols[]) {
#if defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)
  // Clear any old errors.
  dlerror();
#endif
  for (int i = 0; i < num_symbols; ++i) {
    if (!LoadSymbol(handle, symbol_names[i], &symbols[i])) {
      return false;
    }
  }
  return true;
}

}  // namespace webrtc_adm_linux
