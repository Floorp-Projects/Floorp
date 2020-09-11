/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This is a mirror of CSFLog. The implementations of SDPLog, SDPLogV, and
   SDPLogTestLevel must be provided by the user. */

#ifndef SDPLOG_H
#define SDPLOG_H

#include <stdarg.h>

typedef enum {
  SDP_LOG_ERROR = 1,
  SDP_LOG_WARNING,
  SDP_LOG_INFO,
  SDP_LOG_DEBUG,
  SDP_LOG_VERBOSE,
} SDPLogLevel;

#define SDPLogError(tag, format, ...) \
  SDPLog(SDP_LOG_ERROR, __FILE__, __LINE__, tag, format, ##__VA_ARGS__)
#define SDPLogErrorV(tag, format, va_list_arg) \
  SDPLogV(SDP_LOG_ERROR, __FILE__, __LINE__, tag, format, va_list_arg)
#define SDPLogWarn(tag, format, ...) \
  SDPLog(SDP_LOG_WARNING, __FILE__, __LINE__, tag, format, ##__VA_ARGS__)
#define SDPLogWarnV(tag, format, va_list_arg) \
  SDPLogV(SDP_LOG_WARNING, __FILE__, __LINE__, tag, format, va_list_arg)
#define SDPLogInfo(tag, format, ...) \
  SDPLog(SDP_LOG_INFO, __FILE__, __LINE__, tag, format, ##__VA_ARGS__)
#define SDPLogInfoV(tag, format, va_list_arg) \
  SDPLogV(SDP_LOG_INFO, __FILE__, __LINE__, tag, format, va_list_arg)
#define SDPLogDebug(tag, format, ...) \
  SDPLog(SDP_LOG_DEBUG, __FILE__, __LINE__, tag, format, ##__VA_ARGS__)
#define SDPLogDebugV(tag, format, va_list_arg) \
  SDPLogV(SDP_LOG_DEBUG, __FILE__, __LINE__, tag, format, va_list_arg)
#define SDPLogVerbose(tag, format, ...) \
  SDPLog(SDP_LOG_VERBOSE, __FILE__, __LINE__, tag, format, ##__VA_ARGS__)
#define SDPLogVerboseV(tag, format, va_list_arg) \
  SDPLogV(SDP_LOG_VERBOSE, __FILE__, __LINE__, tag, format, va_list_arg)

#ifdef __cplusplus
extern "C" {
#endif
void SDPLog(SDPLogLevel priority, const char* sourceFile, int sourceLine,
            const char* tag, const char* format, ...)

#ifdef __GNUC__
    __attribute__((format(printf, 5, 6)))
#endif
    ;

void SDPLogV(SDPLogLevel priority, const char* sourceFile, int sourceLine,
             const char* tag, const char* format, va_list args);

int SDPLogTestLevel(SDPLogLevel priority);

#ifdef __cplusplus
}
#endif

#endif
