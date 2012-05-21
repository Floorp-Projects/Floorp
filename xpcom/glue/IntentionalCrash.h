/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>
#include <sstream>
#include <stdlib.h>
#include <stdio.h>

#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

#ifndef mozilla_IntentionalCrash_h
#define mozilla_IntentionalCrash_h

namespace mozilla {

inline void
NoteIntentionalCrash(const char* processType)
{
  char* f = getenv("XPCOM_MEM_BLOAT_LOG");
  fprintf(stderr, "XPCOM_MEM_BLOAT_LOG: %s\n", f);
  if (!f)
    return;

  std::string bloatLog(f);
  
  bool hasExt = false;
  if (bloatLog.size() >= 4 &&
      0 == bloatLog.compare(bloatLog.size() - 4, 4, ".log", 4)) {
    hasExt = true;
    bloatLog.erase(bloatLog.size() - 4, 4);
  }

  std::ostringstream bloatName;
  bloatName << bloatLog << "_" << processType << "_pid" << getpid();
  if (hasExt)
    bloatName << ".log";

  fprintf(stderr, "Writing to log: %s\n", bloatName.str().c_str());

  FILE* processfd = fopen(bloatName.str().c_str(), "a");
  fprintf(processfd, "==> process %d will purposefully crash\n", getpid());
  fclose(processfd);
}

} // namespace mozilla

#endif // mozilla_IntentionalCrash_h
