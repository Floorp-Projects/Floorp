/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRtcLog.h"

#include "mozilla/Logging.h"
#include "mozilla/StaticPtr.h"
#include "prenv.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/common_types.h"
#include "webrtc/base/logging.h"

#include "nscore.h"
#include "nsString.h"
#include "nsXULAppAPI.h"
#include "mozilla/Preferences.h"

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

static mozilla::LazyLogModule sWebRtcLog("webrtc_trace");
static mozilla::LazyLogModule sLogAEC("AEC");

class WebRtcTraceCallback: public webrtc::TraceCallback
{
public:
  void Print(webrtc::TraceLevel level, const char* message, int length) override
  {
    MOZ_LOG(sWebRtcLog, LogLevel::Debug, ("%s", message));
  }
};

class LogSinkImpl : public rtc::LogSink
{
public:
  LogSinkImpl() {}

private:
  void OnLogMessage(const std::string& message) override {
    MOZ_LOG(sWebRtcLog, LogLevel::Debug, ("%s", message.data()));
  }
};

// For WEBRTC_TRACE()
static WebRtcTraceCallback gWebRtcCallback;
// For LOG()
static mozilla::StaticAutoPtr<LogSinkImpl> sSink;

void
GetWebRtcLogPrefs(uint32_t *aTraceMask, nsACString& aLogFile,
                  bool *aMultiLog)
{
  *aMultiLog = mozilla::Preferences::GetBool("media.webrtc.debug.multi_log");
  *aTraceMask = mozilla::Preferences::GetUint("media.webrtc.debug.trace_mask");
  mozilla::Preferences::GetCString("media.webrtc.debug.log_file", aLogFile);
  webrtc::Trace::set_aec_debug_size(mozilla::Preferences::GetUint("media.webrtc.debug.aec_dump_max_size"));
}

mozilla::LogLevel
CheckOverrides(uint32_t *aTraceMask, nsACString *aLogFile, bool *aMultiLog)
{
  mozilla::LogModule *log_info = sWebRtcLog;
  mozilla::LogLevel log_level = log_info->Level();

  if (!aTraceMask || !aLogFile || !aMultiLog) {
    return log_level;
  }

  // Override or fill in attributes from the environment if possible.
  switch (log_level) {
    case mozilla::LogLevel::Verbose:
      *aTraceMask = webrtc::TraceLevel::kTraceAll;
      break;
    case mozilla::LogLevel::Debug:
      *aTraceMask = 0x1fff; // kTraceInfo and below
      break;
    case mozilla::LogLevel::Info:
      *aTraceMask = 0x07ff; // kTraceStream and below;
      break;
    case mozilla::LogLevel::Warning:
      *aTraceMask = webrtc::TraceLevel::kTraceDefault; // ktraceModule and below
      break;
    case mozilla::LogLevel::Error:
      *aTraceMask = webrtc::TraceLevel::kTraceWarning |
                    webrtc::TraceLevel::kTraceError |
                    webrtc::TraceLevel::kTraceStateInfo;
      break;
    case mozilla::LogLevel::Disabled:
    default:
      *aTraceMask = 0;
  }

  // Allow it to be overridden
  char *trace_level = getenv("WEBRTC_TRACE_LEVEL");
  if (trace_level && *trace_level) {
    *aTraceMask = atoi(trace_level);
  }

  log_info = sLogAEC;
  if (sLogAEC && (log_info->Level() != mozilla::LogLevel::Disabled)) {
    webrtc::Trace::set_aec_debug(true);
  }

  const char *file_name = PR_GetEnv("WEBRTC_TRACE_FILE");
  if (file_name) {
    aLogFile->Assign(file_name);
  }
  return log_level;
}

void ConfigWebRtcLog(mozilla::LogLevel level, uint32_t trace_mask,
                     nsCString &aLogFile, bool multi_log)
{
  if (gWebRtcTraceLoggingOn) {
    return;
  }

#if defined(ANDROID)
  // Special case: use callback to pipe to NSPR logging.
  aLogFile.Assign(default_log_name);
#else

  rtc::LoggingSeverity log_level;
  switch (level) {
    case mozilla::LogLevel::Verbose:
      log_level = rtc::LoggingSeverity::LS_VERBOSE;
      break;
    case mozilla::LogLevel::Debug:
    case mozilla::LogLevel::Info:
      log_level = rtc::LoggingSeverity::LS_INFO;
      break;
    case mozilla::LogLevel::Warning:
      log_level = rtc::LoggingSeverity::LS_WARNING;
      break;
    case mozilla::LogLevel::Error:
      log_level = rtc::LoggingSeverity::LS_ERROR;
      break;
    case mozilla::LogLevel::Disabled:
      log_level = rtc::LoggingSeverity::LS_NONE;
      break;
    default:
      MOZ_ASSERT(false);
      break;
  }
  rtc::LogMessage::LogToDebug(log_level);
  if (level != mozilla::LogLevel::Disabled) {
    // always capture LOG(...) << ... logging in webrtc.org code to nspr logs
    if (!sSink) {
      sSink = new LogSinkImpl();
      rtc::LogMessage::AddLogToStream(sSink, log_level);
      // it's ok if this leaks to program end
    }
  } else if (sSink) {
    rtc::LogMessage::RemoveLogToStream(sSink);
    sSink = nullptr;
  }

  webrtc::Trace::set_level_filter(trace_mask);
  if (trace_mask != 0) {
    // default WEBRTC_TRACE logs to a rotating file, but allow redirecting to nspr
    // XXX always redirect in e10s if the sandbox blocks file access, or somehow proxy
    if (aLogFile.EqualsLiteral("nspr") || aLogFile.EqualsLiteral("moz_log")) {
      rtc::LogMessage::SetLogToStderr(false);
      webrtc::Trace::SetTraceCallback(&gWebRtcCallback);
    } else {
      rtc::LogMessage::SetLogToStderr(true);
      webrtc::Trace::SetTraceFile(aLogFile.get(), multi_log);
    }
  } else {
    rtc::LogMessage::SetLogToStderr(false);
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

  if (XRE_IsParentProcess()) {
    // Capture the final choice for the trace setting.
    mozilla::Preferences::SetCString("media.webrtc.debug.log_file", aLogFile);
  }
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

  GetWebRtcLogPrefs(&trace_mask, log_file, &multi_log);
  mozilla::LogLevel level = CheckOverrides(&trace_mask, &log_file, &multi_log);

  if (trace_mask == 0) {
    trace_mask = log_level;
  }

  ConfigWebRtcLog(level, trace_mask, log_file, multi_log);

}

void EnableWebRtcLog()
{
  if (gWebRtcTraceLoggingOn) {
    return;
  }

  uint32_t trace_mask = 0;
  bool multi_log = false;
  nsAutoCString log_file;

  GetWebRtcLogPrefs(&trace_mask, log_file, &multi_log);
  mozilla::LogLevel level = CheckOverrides(&trace_mask, &log_file, &multi_log);
  ConfigWebRtcLog(level, trace_mask, log_file, multi_log);
}

// Called when we destroy the singletons from PeerConnectionCtx or if the
// user changes logging in about:webrtc
void StopWebRtcLog()
{
  // TODO(NG) strip/fix gWebRtcTraceLoggingOn which is never set to true
  webrtc::Trace::set_level_filter(webrtc::kTraceNone);
  webrtc::Trace::SetTraceCallback(nullptr);
  webrtc::Trace::SetTraceFile(nullptr);
  if (sSink) {
    rtc::LogMessage::RemoveLogToStream(sSink);
    sSink = nullptr;
  }
}

nsCString ConfigAecLog() {
  nsCString aecLogDir;
  if (webrtc::Trace::aec_debug()) {
    return EmptyCString();
  }
#if defined(ANDROID)
  aecLogDir.Assign(default_tmp_dir);
#else
  nsCOMPtr<nsIFile> tempDir;
  nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tempDir));
  if (NS_SUCCEEDED(rv)) {
    tempDir->GetNativePath(aecLogDir);
  }
#endif
  webrtc::Trace::set_aec_debug_filename(aecLogDir.get());

  return aecLogDir;
}

nsCString StartAecLog()
{
  nsCString aecLogDir;
  if (webrtc::Trace::aec_debug()) {
    return EmptyCString();
  }
  uint32_t trace_mask = 0;
  bool multi_log = false;
  nsAutoCString log_file;

  GetWebRtcLogPrefs(&trace_mask, log_file, &multi_log);
  CheckOverrides(&trace_mask, &log_file, &multi_log);
  aecLogDir = ConfigAecLog();

  webrtc::Trace::set_aec_debug(true);

  return aecLogDir;
}

void StopAecLog()
{
  webrtc::Trace::set_aec_debug(false);
}
