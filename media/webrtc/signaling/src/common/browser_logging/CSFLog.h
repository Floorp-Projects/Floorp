/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CSFLOG_H
#define CSFLOG_H

// In order for FORCE_PR_LOG below to work, we have to define it before the
// first time prlog is #included.
#if defined(PR_LOG)
#error "Must #include CSFLog.h before before any IPDL-generated files or other files that #include prlog.h."
#endif

#if defined(MOZ_LOGGING)
#define FORCE_PR_LOG
#endif

#include <stdarg.h>
#include "prlog.h"

typedef enum{
    CSF_LOG_CRITICAL =1,
    CSF_LOG_ERROR,
    CSF_LOG_WARNING,
    CSF_LOG_NOTICE,
    CSF_LOG_INFO,
    CSF_LOG_DEBUG,
    CSF_LOG_OBNOXIOUS
} CSFLogLevel;



#define CSFLogError(tag , format, ...) CSFLog( CSF_LOG_ERROR, __FILE__ , __LINE__ , tag , format , ## __VA_ARGS__ )
#define CSFLogErrorV(tag , format, va_list_arg) CSFLogV(CSF_LOG_ERROR, __FILE__ , __LINE__ , tag , format , va_list_arg )
#define CSFLogWarn(tag , format, ...) CSFLog( CSF_LOG_WARNING, __FILE__ , __LINE__ , tag , format , ## __VA_ARGS__ )
#define CSFLogWarnV(tag , format, va_list_arg) CSFLogV(CSF_LOG_WARNING, __FILE__ , __LINE__ , tag , format , va_list_arg )
#define CSFLogNotice(tag , format, ...) CSFLog( CSF_LOG_NOTICE, __FILE__ , __LINE__ , tag , format , ## __VA_ARGS__ )
#define CSFLogNoticeV(tag , format, va_list_arg) CSFLogV(CSF_LOG_NOTICE, __FILE__ , __LINE__ , tag , format , va_list_arg )
#define CSFLogInfo(tag , format, ...) CSFLog( CSF_LOG_INFO, __FILE__ , __LINE__ , tag , format , ## __VA_ARGS__ )
#define CSFLogInfoV(tag , format, va_list_arg) CSFLogV(CSF_LOG_INFO, __FILE__ , __LINE__ , tag , format , va_list_arg )
#define CSFLogDebug(tag , format, ...) CSFLog(CSF_LOG_DEBUG, __FILE__ , __LINE__ , tag , format , ## __VA_ARGS__ )
#define CSFLogDebugV(tag , format, va_list_arg) CSFLogV(CSF_LOG_DEBUG, __FILE__ , __LINE__ , tag , format , va_list_arg )
#define CSFLogObnoxious(tag , format, ...) CSFLog(CSF_LOG_OBNOXIOUS, __FILE__ , __LINE__ , tag , format , ## __VA_ARGS__ )
#define CSFLogObnoxiousV(tag , format, va_list_arg) CSFLogV(CSF_LOG_OBNOXIOUS, __FILE__ , __LINE__ , tag , format , va_list_arg )

#ifdef __cplusplus
extern "C"
{
#endif
void CSFLog( CSFLogLevel priority, const char* sourceFile, int sourceLine, const char* tag , const char* format, ...)
#ifdef __GNUC__
  __attribute__ ((format (printf, 5, 6)))
#endif
;
void CSFLogV( CSFLogLevel priority, const char* sourceFile, int sourceLine, const char* tag , const char* format, va_list args);

PRLogModuleInfo *GetSignalingLogInfo();
PRLogModuleInfo *GetWebRTCLogInfo();
extern int gWebrtcTraceLoggingOn;

#ifdef __cplusplus
}
#endif

#endif

