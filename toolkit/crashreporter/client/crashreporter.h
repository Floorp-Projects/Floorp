/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CRASHREPORTER_H__
#define CRASHREPORTER_H__

#ifdef _MSC_VER
# pragma warning( push )
// Disable exception handler warnings.
# pragma warning( disable : 4530 )
#endif

#include <string>
#include <map>
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>

#define MAX_COMMENT_LENGTH   500

#if defined(XP_WIN32)

#include <windows.h>

#define UI_SNPRINTF _snprintf
#define UI_DIR_SEPARATOR "\\"

std::string WideToUTF8(const std::wstring& wide, bool* success = 0);

#else

#define UI_SNPRINTF snprintf
#define UI_DIR_SEPARATOR "/"

#endif

#define UI_CRASH_REPORTER_FILENAME "crashreporter"
#define UI_MINIDUMP_ANALYZER_FILENAME "minidump-analyzer"
#ifndef XP_MACOSX
#define UI_PING_SENDER_FILENAME "pingsender"
#else
#define UI_PING_SENDER_FILENAME "../../../pingsender"
#endif

typedef std::map<std::string, std::string> StringTable;

#define ST_CRASHREPORTERTITLE        "CrashReporterTitle"
#define ST_CRASHREPORTERVENDORTITLE  "CrashReporterVendorTitle"
#define ST_CRASHREPORTERERROR        "CrashReporterErrorText"
#define ST_CRASHREPORTERPRODUCTERROR "CrashReporterProductErrorText2"
#define ST_CRASHREPORTERHEADER       "CrashReporterSorry"
#define ST_CRASHREPORTERDESCRIPTION  "CrashReporterDescriptionText2"
#define ST_CRASHREPORTERDEFAULT      "CrashReporterDefault"
#define ST_VIEWREPORT                "Details"
#define ST_VIEWREPORTTITLE           "ViewReportTitle"
#define ST_COMMENTGRAYTEXT           "CommentGrayText"
#define ST_EXTRAREPORTINFO           "ExtraReportInfo"
#define ST_CHECKSUBMIT               "CheckSendReport"
#define ST_CHECKURL                  "CheckIncludeURL"
#define ST_CHECKEMAIL                "CheckAllowEmail"
#define ST_EMAILGRAYTEXT             "EmailGrayText"
#define ST_REPORTPRESUBMIT           "ReportPreSubmit2"
#define ST_REPORTDURINGSUBMIT        "ReportDuringSubmit2"
#define ST_REPORTSUBMITSUCCESS       "ReportSubmitSuccess"
#define ST_SUBMITFAILED              "ReportSubmitFailed"
#define ST_QUIT                      "Quit2"
#define ST_RESTART                   "Restart"
#define ST_OK                        "Ok"
#define ST_CLOSE                     "Close"

#define ST_ERROR_BADARGUMENTS        "ErrorBadArguments"
#define ST_ERROR_EXTRAFILEEXISTS     "ErrorExtraFileExists"
#define ST_ERROR_EXTRAFILEREAD       "ErrorExtraFileRead"
#define ST_ERROR_EXTRAFILEMOVE       "ErrorExtraFileMove"
#define ST_ERROR_DUMPFILEEXISTS      "ErrorDumpFileExists"
#define ST_ERROR_DUMPFILEMOVE        "ErrorDumpFileMove"
#define ST_ERROR_NOPRODUCTNAME       "ErrorNoProductName"
#define ST_ERROR_NOSERVERURL         "ErrorNoServerURL"
#define ST_ERROR_NOSETTINGSPATH      "ErrorNoSettingsPath"
#define ST_ERROR_CREATEDUMPDIR       "ErrorCreateDumpDir"
#define ST_ERROR_ENDOFLIFE           "ErrorEndOfLife"

//=============================================================================
// implemented in crashreporter.cpp and ping.cpp
//=============================================================================

namespace CrashReporter {
  extern StringTable  gStrings;
  extern std::string  gSettingsPath;
  extern std::string  gEventsPath;
  extern int          gArgc;
  extern char**       gArgv;

  void UIError(const std::string& message);

  // The UI finished sending the report
  void SendCompleted(bool success, const std::string& serverResponse);

  bool ReadStrings(std::istream& in,
                   StringTable& strings,
                   bool unescape);
  bool ReadStringsFromFile(const std::string& path,
                           StringTable& strings,
                           bool unescape);
  bool WriteStrings(std::ostream& out,
                    const std::string& header,
                    StringTable& strings,
                    bool escape);
  bool WriteStringsToFile(const std::string& path,
                          const std::string& header,
                          StringTable& strings,
                          bool escape);
  void LogMessage(const std::string& message);
  void DeleteDump();

  // Telemetry ping
  bool SendCrashPing(StringTable& strings, const std::string& hash,
                     std::string& pingUuid, const std::string& pingDir);

  static const unsigned int kSaveCount = 10;
}

//=============================================================================
// implemented in the platform-specific files
//=============================================================================

bool UIInit();
void UIShutdown();

// Run the UI for when the app was launched without a dump file
void UIShowDefaultUI();

// Run the UI for when the app was launched with a dump file
// Return true if the user sent (or tried to send) the crash report,
// false if they chose not to, and it should be deleted.
bool UIShowCrashUI(const StringTable& files,
                   const StringTable& queryParameters,
                   const std::string& sendURL,
                   const std::vector<std::string>& restartArgs);

void UIError_impl(const std::string& message);

bool UIGetIniPath(std::string& path);
bool UIGetSettingsPath(const std::string& vendor,
                       const std::string& product,
                       std::string& settingsPath);
bool UIEnsurePathExists(const std::string& path);
bool UIFileExists(const std::string& path);
bool UIMoveFile(const std::string& oldfile, const std::string& newfile);
bool UIDeleteFile(const std::string& oldfile);
std::ifstream* UIOpenRead(const std::string& filename, bool binary = false);
std::ofstream* UIOpenWrite(const std::string& filename,
                           bool append=false,
                           bool binary=false);
void UIPruneSavedDumps(const std::string& directory);

// Run the program specified by exename, passing it the parameters in arg.
// If wait is true, wait for the program to terminate execution before
// returning. Returns true if the program was launched correctly, false
// otherwise.
bool UIRunProgram(const std::string& exename,
                  const std::vector<std::string>& args,
                  bool wait = false);

// Read the environment variable specified by name
std::string UIGetEnv(const std::string name);

#ifdef _MSC_VER
# pragma warning( pop )
#endif

#endif
