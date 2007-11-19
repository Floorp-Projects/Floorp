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

#if defined(XP_WIN32)

#include <windows.h>

#define UI_SNPRINTF _snprintf
#define UI_DIR_SEPARATOR "\\"

std::string WideToUTF8(const std::wstring& wide, bool* success = 0);

#else

#define UI_SNPRINTF snprintf
#define UI_DIR_SEPARATOR "/"

#endif

typedef std::map<std::string, std::string> StringTable;

#define ST_CRASHREPORTERTITLE        "CrashReporterTitle"
#define ST_CRASHREPORTERVENDORTITLE  "CrashReporterVendorTitle"
#define ST_CRASHREPORTERERROR        "CrashReporterError"
#define ST_CRASHREPORTERPRODUCTERROR "CrashReporterProductError"
#define ST_CRASHREPORTERHEADER       "CrashReporterHeader"
#define ST_CRASHREPORTERDESCRIPTION  "CrashReporterDescription"
#define ST_CRASHREPORTERDEFAULT      "CrashReporterDefault"
#define ST_VIEWREPORT                "ViewReport"
#define ST_EXTRAREPORTINFO           "ExtraReportInfo"
#define ST_CHECKSUBMIT               "CheckSubmit"
#define ST_CHECKEMAIL                "CheckEmail"
#define ST_CLOSE                     "Close"
#define ST_RESTART                   "Restart"
#define ST_SUBMITFAILED              "SubmitFailed"

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

//=============================================================================
// implemented in crashreporter.cpp
//=============================================================================

namespace CrashReporter {
  extern StringTable  gStrings;
  extern std::string  gSettingsPath;
  extern int          gArgc;
  extern char**       gArgv;

  void UIError(const std::string& message);

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
  void LogMessage(const std::string& message);
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

void UIError_impl(const std::string& message);

bool UIGetIniPath(std::string& path);
bool UIGetSettingsPath(const std::string& vendor,
                       const std::string& product,
                       std::string& settingsPath);
bool UIEnsurePathExists(const std::string& path);
bool UIFileExists(const std::string& path);
bool UIMoveFile(const std::string& oldfile, const std::string& newfile);
bool UIDeleteFile(const std::string& oldfile);
std::ifstream* UIOpenRead(const std::string& filename);
std::ofstream* UIOpenWrite(const std::string& filename, bool append=false);

#ifdef _MSC_VER
# pragma warning( pop )
#endif

#endif
