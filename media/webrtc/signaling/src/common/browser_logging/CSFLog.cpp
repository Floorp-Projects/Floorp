/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "CSFLog.h"
#include "base/basictypes.h"
#include "prtypes.h"

static PRLogModuleInfo *gLogModuleInfo = NULL;

PRLogModuleInfo *GetSignalingLogInfo()
{
  if (gLogModuleInfo == NULL)
    gLogModuleInfo = PR_NewLogModule("signaling");

  return gLogModuleInfo;
}

static PRLogModuleInfo *gWebRTCLogModuleInfo = NULL;
int gWebrtcTraceLoggingOn = 0;

PRLogModuleInfo *GetWebRTCLogInfo()
{
  if (gWebRTCLogModuleInfo == NULL)
    gWebRTCLogModuleInfo = PR_NewLogModule("webrtc_trace");

  return gWebRTCLogModuleInfo;
}


void CSFLogV(CSFLogLevel priority, const char* sourceFile, int sourceLine, const char* tag , const char* format, va_list args)
{
#ifdef STDOUT_LOGGING
  printf("%s\n:",tag);
  vprintf(format, args);
#else

#define MAX_MESSAGE_LENGTH 1024
  char message[MAX_MESSAGE_LENGTH];

  vsnprintf(message, MAX_MESSAGE_LENGTH, format, args);

  GetSignalingLogInfo();

  switch(priority)
  {
    case CSF_LOG_CRITICAL:
    case CSF_LOG_ERROR:
      PR_LOG(gLogModuleInfo, PR_LOG_ERROR, ("%s %s", tag, message));
      break;
    case CSF_LOG_WARNING:
    case CSF_LOG_INFO:
    case CSF_LOG_NOTICE:
      PR_LOG(gLogModuleInfo, PR_LOG_WARNING, ("%s %s", tag, message));
      break;
    case CSF_LOG_DEBUG:
      PR_LOG(gLogModuleInfo, PR_LOG_DEBUG, ("%s %s", tag, message));
      break;
    default:
      PR_LOG(gLogModuleInfo, PR_LOG_ALWAYS, ("%s %s", tag, message));
  }

#endif

}

void CSFLog( CSFLogLevel priority, const char* sourceFile, int sourceLine, const char* tag , const char* format, ...)
{
	va_list ap;
  va_start(ap, format);

  CSFLogV(priority, sourceFile, sourceLine, tag, format, ap);
  va_end(ap);
}

