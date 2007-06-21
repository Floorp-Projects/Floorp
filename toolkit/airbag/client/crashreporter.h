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

#if defined(XP_WIN32)

#include <windows.h>
#define UI_SNPRINTF _snprintf
#define UI_DIR_SEPARATOR "\\"

#else

#define UI_SNPRINTF snprintf
#define UI_DIR_SEPARATOR "/"

#endif

typedef std::map<std::string, std::string> StringTable;

#define ST_CRASHREPORTERTITLE       "CrashReporterTitle"
#define ST_CRASHREPORTERHEADER      "CrashReporterHeader"
#define ST_CRASHREPORTERDESCRIPTION "CrashReporterDescription"
#define ST_CRASHREPORTERDEFAULT     "CrashReporterDefault"
#define ST_VIEWREPORT               "ViewReport"
#define ST_EXTRAREPORTINFO          "ExtraReportInfo"
#define ST_CHECKSUBMIT              "CheckSubmit"
#define ST_CHECKEMAIL               "CheckEmail"
#define ST_CLOSE                    "Close"
#define ST_RESTART                  "Restart"
#define ST_SUBMITFAILED             "SubmitFailed"

//=============================================================================
// implemented in crashreporter.cpp
//=============================================================================

namespace CrashReporter {
  extern StringTable  gStrings;
  extern std::string  gSettingsPath;
  extern int          gArgc;
  extern char**       gArgv;

  // The UI finished sending the report
  bool SendCompleted(bool success, const std::string& serverResponse);

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

}

//=============================================================================
// implemented in the platform-specific files
//=============================================================================

bool UIInit();
void UIShutdown();

// Run the UI for when the app was launched without a dump file
void UIShowDefaultUI();

// Run the UI for when the app was launched with a dump file
void UIShowCrashUI(const std::string& dumpfile,
                   const StringTable& queryParameters,
                   const std::string& sendURL,
                   const std::vector<std::string>& restartArgs);

void UIError(const std::string& message);

bool UIGetIniPath(std::string& path);
bool UIGetSettingsPath(const std::string& vendor,
                       const std::string& product,
                       std::string& settingsPath);
bool UIEnsurePathExists(const std::string& path);
bool UIMoveFile(const std::string& oldfile, const std::string& newfile);
bool UIDeleteFile(const std::string& oldfile);

#ifdef _MSC_VER
# pragma warning( pop )
#endif

#endif
