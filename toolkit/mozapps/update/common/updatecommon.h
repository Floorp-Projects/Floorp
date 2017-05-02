/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef UPDATECOMMON_H
#define UPDATECOMMON_H

#include "updatedefines.h"
#include <stdio.h>
#include "mozilla/Attributes.h"

class UpdateLog
{
public:
  static UpdateLog & GetPrimaryLog() 
  {
    static UpdateLog primaryLog;
    return primaryLog;
  }

  void Init(NS_tchar* sourcePath, const NS_tchar* fileName);
  void Finish();
  void Flush();
  void Printf(const char *fmt, ... ) MOZ_FORMAT_PRINTF(2, 3);
  void WarnPrintf(const char *fmt, ... ) MOZ_FORMAT_PRINTF(2, 3);

  ~UpdateLog()
  {
    Finish();
  }

protected:
  UpdateLog();
  FILE *logFP;
  NS_tchar mTmpFilePath[MAXPATHLEN];
  NS_tchar mDstFilePath[MAXPATHLEN];
};

bool IsValidFullPath(NS_tchar* fullPath);

#define LOG_WARN(args) UpdateLog::GetPrimaryLog().WarnPrintf args
#define LOG(args) UpdateLog::GetPrimaryLog().Printf args
#define LogInit(PATHNAME_, FILENAME_) \
  UpdateLog::GetPrimaryLog().Init(PATHNAME_, FILENAME_)
#define LogFinish() UpdateLog::GetPrimaryLog().Finish()
#define LogFlush() UpdateLog::GetPrimaryLog().Flush()

#endif
