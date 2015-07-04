/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "platform.h"
#include "PlatformMacros.h"
#include "LulMain.h"
#include "shared-libraries.h"
#include "AutoObjectMapper.h"

// Contains miscellaneous helpers that are used to connect SPS and LUL.


// Find out, in a platform-dependent way, where the code modules got
// mapped in the process' virtual address space, and get |aLUL| to
// load unwind info for them.
void
read_procmaps(lul::LUL* aLUL)
{
  MOZ_ASSERT(aLUL->CountMappings() == 0);

# if defined(SPS_OS_linux) || defined(SPS_OS_android) || defined(SPS_OS_darwin)
  SharedLibraryInfo info = SharedLibraryInfo::GetInfoForSelf();

  for (size_t i = 0; i < info.GetSize(); i++) {
    const SharedLibrary& lib = info.GetEntry(i);

#   if defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
    // We're using faulty.lib.  Use a special-case object mapper.
    AutoObjectMapperFaultyLib mapper(aLUL->mLog);
#   else
    // We can use the standard POSIX-based mapper.
    AutoObjectMapperPOSIX mapper(aLUL->mLog);
#   endif

    // Ask |mapper| to map the object.  Then hand its mapped address
    // to NotifyAfterMap().
    void*  image = nullptr;
    size_t size  = 0;
    bool ok = mapper.Map(&image, &size, lib.GetName());
    if (ok && image && size > 0) {
      aLUL->NotifyAfterMap(lib.GetStart(), lib.GetEnd()-lib.GetStart(),
                           lib.GetName().c_str(), image);
    } else if (!ok && lib.GetName() == "") {
      // The object has no name and (as a consequence) the mapper
      // failed to map it.  This happens on Linux, where
      // GetInfoForSelf() produces two such mappings: one for the
      // executable and one for the VDSO.  The executable one isn't a
      // big deal since there's not much interesting code in there,
      // but the VDSO one is a problem on x86-{linux,android} because
      // lack of knowledge about the mapped area inhibits LUL's
      // special __kernel_syscall handling.  Hence notify |aLUL| at
      // least of the mapping, even though it can't read any unwind
      // information for the area.
      aLUL->NotifyExecutableArea(lib.GetStart(), lib.GetEnd()-lib.GetStart());
    }

    // |mapper| goes out of scope at this point and so its destructor
    // unmaps the object.
  }

# else
#  error "Unknown platform"
# endif
}


// LUL needs a callback for its logging sink.
void
logging_sink_for_LUL(const char* str) {
  // Ignore any trailing \n, since LOG will add one anyway.
  size_t n = strlen(str);
  if (n > 0 && str[n-1] == '\n') {
    char* tmp = strdup(str);
    tmp[n-1] = 0;
    LOG(tmp);
    free(tmp);
  } else {
    LOG(str);
  }
}
