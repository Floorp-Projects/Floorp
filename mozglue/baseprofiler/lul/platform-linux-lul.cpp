/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "AutoObjectMapper.h"
#include "BaseProfiler.h"
#include "BaseProfilerSharedLibraries.h"
#include "platform.h"
#include "PlatformMacros.h"
#include "LulMain.h"

// Contains miscellaneous helpers that are used to connect the Gecko Profiler
// and LUL.

// Find out, in a platform-dependent way, where the code modules got
// mapped in the process' virtual address space, and get |aLUL| to
// load unwind info for them.
void read_procmaps(lul::LUL* aLUL) {
  MOZ_ASSERT(aLUL->CountMappings() == 0);

#if defined(GP_OS_linux) || defined(GP_OS_android) || defined(GP_OS_freebsd)
  SharedLibraryInfo info = SharedLibraryInfo::GetInfoForSelf();

  for (size_t i = 0; i < info.GetSize(); i++) {
    const SharedLibrary& lib = info.GetEntry(i);

    std::string nativePath = lib.GetDebugPath();

#  if defined(MOZ_LINKER)
    // We're using faulty.lib.  Use a special-case object mapper.
    AutoObjectMapperFaultyLib mapper(aLUL->mLog);
#  else
    // We can use the standard POSIX-based mapper.
    AutoObjectMapperPOSIX mapper(aLUL->mLog);
#  endif

    // Ask |mapper| to map the object.  Then hand its mapped address
    // to NotifyAfterMap().
    void* image = nullptr;
    size_t size = 0;
    bool ok = mapper.Map(&image, &size, nativePath);
    if (ok && image && size > 0) {
      aLUL->NotifyAfterMap(lib.GetStart(), lib.GetEnd() - lib.GetStart(),
                           nativePath.c_str(), image);
    } else if (!ok && lib.GetDebugName().empty()) {
      // The object has no name and (as a consequence) the mapper failed to map
      // it.  This happens on Linux, where GetInfoForSelf() produces such a
      // mapping for the VDSO.  This is a problem on x86-{linux,android} because
      // lack of knowledge about the mapped area inhibits LUL's special
      // __kernel_syscall handling.  Hence notify |aLUL| at least of the
      // mapping, even though it can't read any unwind information for the area.
      aLUL->NotifyExecutableArea(lib.GetStart(), lib.GetEnd() - lib.GetStart());
    }

    // |mapper| goes out of scope at this point and so its destructor
    // unmaps the object.
  }

#else
#  error "Unknown platform"
#endif
}

// LUL needs a callback for its logging sink.
void logging_sink_for_LUL(const char* str) {
  // These are only printed when Verbose logging is enabled (e.g. with
  // MOZ_BASE_PROFILER_VERBOSE_LOGGING=1). This is because LUL's logging is much
  // more verbose than the rest of the profiler's logging, which occurs at the
  // Info (3) and Debug (4) levels.
  // FIXME: This causes a build failure in memory/replace/dmd/test/SmokeDMD (!)
  // and other places, because it doesn't link the implementation in
  // platform.cpp.
  // VERBOSE_LOG("[%d] %s", profiler_current_process_id(), str);
}
