/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRtcLog.h"

#include "prlog.h"
#include "prenv.h"
#include "webrtc/system_wrappers/interface/trace.h"

#include "nscore.h"
#ifdef MOZILLA_INTERNAL_API
#include "nsString.h"
#include "nsXULAppAPI.h"
#if !defined(MOZILLA_XPCOMRT_API)
#include "mozilla/Preferences.h"
#endif // !defined(MOZILLA_XPCOMRT_API)
#else
#include "nsStringAPI.h"
#endif

static int gWebRtcTraceLoggingOn = 0;

#ifndef ANDROID
static const char *default_log = "WebRTC.log";
#endif

static PRLogModuleInfo* GetWebRtcTraceLog()
{
  static PRLogModuleInfo *sLog;
  if (!sLog) {
    sLog = PR_NewLogModule("webrtc_trace");
  }
  return sLog;
}

static PRLogModuleInfo* GetWebRtcAECLog()
{
  static PRLogModuleInfo *sLog;
  if (!sLog) {
    sLog = PR_NewLogModule("AEC");
  }
  return sLog;
}

class WebRtcTraceCallback: public webrtc::TraceCallback
{
public:
  void Print(webrtc::TraceLevel level, const char* message, int length)
  {
    PRLogModuleInfo *log = GetWebRtcTraceLog();
    PR_LOG(log, PR_LOG_DEBUG, ("%s", message));
  }
};

static WebRtcTraceCallback gWebRtcCallback;

#ifdef MOZILLA_INTERNAL_API
void GetWebRtcLogPrefs(uint32_t *aTraceMask, nsACString* aLogFile, nsACString *aAECLogDir, bool *aMultiLog)
{
#if !defined(MOZILLA_XPCOMRT_API)
  *aMultiLog = mozilla::Preferences::GetBool("media.webrtc.debug.multi_log");
  *aTraceMask = mozilla::Preferences::GetUint("media.webrtc.debug.trace_mask");
  mozilla::Preferences::GetCString("media.webrtc.debug.log_file", aLogFile);
  mozilla::Preferences::GetCString("media.webrtc.debug.aec_log_dir", aAECLogDir);
  webrtc::Trace::set_aec_debug_size(mozilla::Preferences::GetUint("media.webrtc.debug.aec_dump_max_size"));
#endif // !defined(MOZILLA_XPCOMRT_API)
}
#endif

void CheckOverrides(uint32_t *aTraceMask, nsACString *aLogFile, bool *aMultiLog)
{
  if (!aTraceMask || !aLogFile || !aMultiLog) {
    return;
  }

  // Override or fill in attributes from the environment if possible.

  PRLogModuleInfo *log_info = GetWebRtcTraceLog();
  /* When webrtc_trace:x is not part of the NSPR_LOG_MODULES var the structure returned from
     the GetWebRTCLogInfo call will be non-null and show a level of 0. This cannot
     be reliably used to turn off the trace and override a log level from about:config as
     there is no way to differentiate between NSPR_LOG_MODULES=webrtc_trace:0 and the complete
     absense of the webrtc_trace in the environment string at all.
  */
  if (log_info && (log_info->level != 0)) {
    *aTraceMask = log_info->level;
  }

  log_info = GetWebRtcAECLog();
  if (log_info && (log_info->level != 0)) {
    webrtc::Trace::set_aec_debug(true);
  }

  const char *file_name = PR_GetEnv("WEBRTC_TRACE_FILE");
  if (file_name) {
    aLogFile->Assign(file_name);
  }
}

void ConfigWebRtcLog(uint32_t trace_mask, nsCString &aLogFile, nsCString &aAECLogDir, bool multi_log)
{
  if (gWebRtcTraceLoggingOn) {
    return;
  }

  nsCString logFile;
  nsCString aecLogDir;
#if defined(XP_WIN)
  // Use the Windows TEMP environment variable as part of the default location.
  const char *temp_dir = PR_GetEnv("TEMP");
  if (!temp_dir) {
    logFile.Assign(default_log);
  } else {
    logFile.Assign(temp_dir);
    logFile.Append('/');
    aecLogDir = logFile;
    logFile.Append(default_log);
  }
#elif defined(ANDROID)
  // Special case: use callback to pipe to NSPR logging.
  logFile.Assign("nspr");
  // for AEC, force the user to specify a directory
  aecLogDir.Assign("/dev/null");
#else
  // UNIX-like place for the others
  logFile.Assign("/tmp/");
  aecLogDir = logFile;
  logFile.Append(default_log);
#endif
  if (aLogFile.IsEmpty()) {
    aLogFile = logFile;
  }
  if (aAECLogDir.IsEmpty()) {
    aAECLogDir = aecLogDir;
  }

  webrtc::Trace::set_level_filter(trace_mask);
  webrtc::Trace::set_aec_debug_filename(aAECLogDir.get());
  if (trace_mask != 0) {
    if (aLogFile.EqualsLiteral("nspr")) {
      webrtc::Trace::SetTraceCallback(&gWebRtcCallback);
    } else {
      webrtc::Trace::SetTraceFile(aLogFile.get(), multi_log);
    }
  }
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    // Capture the final choices for the trace settings.
    mozilla::Preferences::SetCString("media.webrtc.debug.log_file", aLogFile);
    mozilla::Preferences::SetCString("media.webrtc.debug.aec_log_dir", aAECLogDir);
  }
#endif
  return;
}

void StartWebRtcLog(uint32_t log_level)
{
  if (gWebRtcTraceLoggingOn && log_level != 0) {
    return;
  }

  if (log_level == 0) {
    if (gWebRtcTraceLoggingOn) {
      gWebRtcTraceLoggingOn = false;
      webrtc::Trace::set_level_filter(webrtc::kTraceNone);
    }
    return;
  }

  uint32_t trace_mask = 0;
  bool multi_log = false;
  nsAutoCString log_file;
  nsAutoCString aec_log_dir;

#ifdef MOZILLA_INTERNAL_API
  GetWebRtcLogPrefs(&trace_mask, &log_file, &aec_log_dir, &multi_log);
#endif
  CheckOverrides(&trace_mask, &log_file, &multi_log);

  if (trace_mask == 0) {
    trace_mask = log_level;
  }

  ConfigWebRtcLog(trace_mask, log_file, aec_log_dir, multi_log);
  return;

}

void EnableWebRtcLog()
{
  if (gWebRtcTraceLoggingOn) {
    return;
  }

  uint32_t trace_mask = 0;
  bool multi_log = false;
  nsAutoCString log_file;
  nsAutoCString aec_log_dir;

#ifdef MOZILLA_INTERNAL_API
  GetWebRtcLogPrefs(&trace_mask, &log_file, &aec_log_dir, &multi_log);
#endif
  CheckOverrides(&trace_mask, &log_file, &multi_log);
  ConfigWebRtcLog(trace_mask, log_file, aec_log_dir, multi_log);
  return;
}
