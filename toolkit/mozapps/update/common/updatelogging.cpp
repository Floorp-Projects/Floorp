/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(XP_WIN)
#include <windows.h>
#endif


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "updatelogging.h"

UpdateLog::UpdateLog() : logFP(nullptr)
{
}

void UpdateLog::Init(NS_tchar* sourcePath,
                     const NS_tchar* fileName,
                     const NS_tchar* alternateFileName,
                     bool append)
{
  if (logFP)
    return;

  this->sourcePath = sourcePath;
  NS_tchar logFile[MAXPATHLEN];
  NS_tsnprintf(logFile, sizeof(logFile)/sizeof(logFile[0]),
    NS_T("%s/%s"), sourcePath, fileName);

  if (alternateFileName && NS_taccess(logFile, F_OK)) {
    NS_tsnprintf(logFile, sizeof(logFile)/sizeof(logFile[0]),
      NS_T("%s/%s"), sourcePath, alternateFileName);
  }

  logFP = NS_tfopen(logFile, append ? NS_T("a") : NS_T("w"));
}

void UpdateLog::Finish()
{
  if (!logFP)
    return;

  fclose(logFP);
  logFP = nullptr;
}

void UpdateLog::Flush()
{
  if (!logFP)
    return;

  fflush(logFP);
}

void UpdateLog::Printf(const char *fmt, ... )
{
  if (!logFP)
    return;

  va_list ap;
  va_start(ap, fmt);
  vfprintf(logFP, fmt, ap);
  fprintf(logFP, "\n");
  va_end(ap);
}

void UpdateLog::WarnPrintf(const char *fmt, ... )
{
  if (!logFP)
    return;

  va_list ap;
  va_start(ap, fmt);
  fprintf(logFP, "*** Warning: ");
  vfprintf(logFP, fmt, ap);
  fprintf(logFP, "***\n");
  va_end(ap);
}
