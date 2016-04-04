/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRtcLog.h"

#include "mozilla/Logging.h"
#include "prenv.h"
#include "webrtc/system_wrappers/interface/trace.h"

#include "nscore.h"
#ifdef MOZILLA_INTERNAL_API
#include "nsString.h"
#include "nsXULAppAPI.h"
#include "mozilla/Preferences.h"
#else
#include "nsStringAPI.h"
#endif

#include "nsIFile.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"

using mozilla::LogLevel;

static int gWebRtcTraceLoggingOn = 0;


#if defined(ANDROID)
static const char *default_tmp_dir = "/dev/null";
static const char *default_log_name = "nspr";
#else // Assume a POSIX environment
NS_NAMED_LITERAL_CSTRING(default_log_name, "WebRTC.log");
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
    MOZ_LOG(log, LogLevel::Debug, ("%s", message));
  }
};

static WebRtcTraceCallback gWebRtcCallback;

#ifdef MOZILLA_INTERNAL_API
void GetWebRtcLogPrefs(uint32_t *aTraceMask, nsACString* aLogFile, nsACString *aAECLogDir, bool *aMultiLog)
{
  *aMultiLog = mozilla::Preferences::GetBool("media.webrtc.debug.multi_log");
  *aTraceMask = mozilla::Preferences::GetUint("media.webrtc.debug.trace_mask");
  mozilla::Preferences::GetCString("media.webrtc.debug.log_file", aLogFile);
  mozilla::Preferences::GetCString("media.webrtc.debug.aec_log_dir", aAECLogDir);
  webrtc::Trace::set_aec_debug_size(mozilla::Preferences::GetUint("media.webrtc.debug.aec_dump_max_size"));
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

#if defined(ANDROID)
  // Special case: use callback to pipe to NSPR logging.
  aLogFile.Assign(default_log_name);
#else

  webrtc::Trace::set_level_filter(trace_mask);

  if (trace_mask != 0) {
    if (aLogFile.EqualsLiteral("nspr")) {
      webrtc::Trace::SetTraceCallback(&gWebRtcCallback);
    } else {
      webrtc::Trace::SetTraceFile(aLogFile.get(), multi_log);
    }
  }

  if (aLogFile.IsEmpty()) {
    nsCOMPtr<nsIFile> tempDir;
    nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tempDir));
    if (NS_SUCCEEDED(rv)) {
      tempDir->AppendNative(default_log_name);
      tempDir->GetNativePath(aLogFile);
    }
  }
#endif

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  if (XRE_IsParentProcess()) {
    // Capture the final choice for the trace setting.
    mozilla::Preferences::SetCString("media.webrtc.debug.log_file", aLogFile);
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

void StopWebRtcLog()
{
  webrtc::Trace::SetTraceFile(nullptr);
}

void ConfigAecLog(nsCString &aAECLogDir) {
  if (webrtc::Trace::aec_debug()) {
    return;
  }
#if defined(ANDROID)
  // For AEC, do not use a default value: force the user to specify a directory.
  if (aAECLogDir.IsEmpty()) {
    aAECLogDir.Assign(default_tmp_dir);
  }
#else
  if (aAECLogDir.IsEmpty()) {
    nsCOMPtr<nsIFile> tempDir;
    nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tempDir));
    if (NS_SUCCEEDED(rv)) {
      if (aAECLogDir.IsEmpty()) {
        tempDir->GetNativePath(aAECLogDir);
      }
    }
  }
#endif
  webrtc::Trace::set_aec_debug_filename(aAECLogDir.get());
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  if (XRE_IsParentProcess()) {
    // Capture the final choice for the aec_log_dir setting.
    mozilla::Preferences::SetCString("media.webrtc.debug.aec_log_dir", aAECLogDir);
  }
#endif
}

void StartAecLog()
{
  if (webrtc::Trace::aec_debug()) {
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
  ConfigAecLog(aec_log_dir);

  webrtc::Trace::set_aec_debug(true);
}

void StopAecLog()
{
  webrtc::Trace::set_aec_debug(false);
}
