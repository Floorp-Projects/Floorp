/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Linker_h
#define Linker_h

#ifdef MOZ_LINKER
#  include "ElfLoader.h"
#  define __wrap_sigaction SEGVHandler::__wrap_sigaction
#else
#  include <dlfcn.h>
#  include <link.h>
#  include <signal.h>
#  define __wrap_sigaction sigaction
#  define __wrap_dlopen dlopen
#  define __wrap_dlerror dlerror
#  define __wrap_dlsym dlsym
#  define __wrap_dlclose dlclose
#  define __wrap_dladdr dladdr
#  define __wrap_dl_iterate_phdr dl_iterate_phdr
#endif

#endif
