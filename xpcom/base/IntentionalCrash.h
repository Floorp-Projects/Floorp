/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_IntentionalCrash_h
#define mozilla_IntentionalCrash_h

#include <string>
#include <sstream>
#include <stdlib.h>
#include <stdio.h>

#ifdef XP_WIN
#  include <process.h>
#  define getpid _getpid
#else
#  include <unistd.h>
#endif

namespace mozilla {

inline void NoteIntentionalCrash(const char* aProcessType, uint32_t aPid = 0) {
// In opt builds we don't actually have the leak checking enabled, and the
// sandbox doesn't allow writing to this path, so we just disable this
// function's behaviour.
#ifdef MOZ_DEBUG
  char* f = getenv("XPCOM_MEM_BLOAT_LOG");
  if (!f) {
    return;
  }

  fprintf(stderr, "XPCOM_MEM_BLOAT_LOG: %s\n", f);

  uint32_t processPid = aPid == 0 ? getpid() : aPid;

  std::ostringstream bloatName;
  std::string processType(aProcessType);
  if (!processType.compare("default")) {
    bloatName << f;
  } else {
    std::string bloatLog(f);

    bool hasExt = false;
    if (bloatLog.size() >= 4 &&
        bloatLog.compare(bloatLog.size() - 4, 4, ".log", 4) == 0) {
      hasExt = true;
      bloatLog.erase(bloatLog.size() - 4, 4);
    }

    bloatName << bloatLog << "_" << processType << "_pid" << processPid;
    if (hasExt) {
      bloatName << ".log";
    }
  }

  fprintf(stderr, "Writing to log: %s\n", bloatName.str().c_str());

  FILE* processfd = fopen(bloatName.str().c_str(), "a");
  if (processfd) {
    fprintf(processfd, "\n==> process %d will purposefully crash\n",
            processPid);
    fclose(processfd);
  }
#endif
}

}  // namespace mozilla

#endif  // mozilla_IntentionalCrash_h
