/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "crashreporter.h"

#ifdef _MSC_VER
// Disable exception handler warnings.
#  pragma warning(disable : 4530)
#endif

#include <fstream>
#include <iomanip>
#include <sstream>
#include <memory>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

#ifdef XP_LINUX
#  include <dlfcn.h>
#endif

#include "json/json.h"
#include "nss.h"
#include "sechash.h"

using std::ifstream;
using std::ios;
using std::istream;
using std::istringstream;
using std::ofstream;
using std::ostream;
using std::ostringstream;
using std::string;
using std::unique_ptr;
using std::vector;

namespace CrashReporter {

StringTable gStrings;
Json::Value gData;
string gSettingsPath;
string gEventsPath;
string gPingPath;
int gArgc;
char** gArgv;
bool gAutoSubmit;

enum SubmissionResult { Succeeded, Failed };

static unique_ptr<ofstream> gLogStream(nullptr);
static string gReporterDumpFile;
static string gExtraFile;
static string gMemoryFile;

static const char kExtraDataExtension[] = ".extra";
static const char kMemoryReportExtension[] = ".memory.json.gz";

void UIError(const string& message) {
  if (gAutoSubmit) {
    return;
  }

  string errorMessage;
  if (!gStrings[ST_CRASHREPORTERERROR].empty()) {
    char buf[2048];
    UI_SNPRINTF(buf, 2048, gStrings[ST_CRASHREPORTERERROR].c_str(),
                message.c_str());
    errorMessage = buf;
  } else {
    errorMessage = message;
  }

  UIError_impl(errorMessage);
}

static string Unescape(const string& str) {
  string ret;
  for (string::const_iterator iter = str.begin(); iter != str.end(); iter++) {
    if (*iter == '\\') {
      iter++;
      if (*iter == '\\') {
        ret.push_back('\\');
      } else if (*iter == 'n') {
        ret.push_back('\n');
      } else if (*iter == 't') {
        ret.push_back('\t');
      }
    } else {
      ret.push_back(*iter);
    }
  }

  return ret;
}

bool ReadStrings(istream& in, StringTable& strings, bool unescape) {
  string currentSection;
  while (!in.eof()) {
    string line;
    std::getline(in, line);
    int sep = line.find('=');
    if (sep >= 0) {
      string key, value;
      key = line.substr(0, sep);
      value = line.substr(sep + 1);
      if (unescape) value = Unescape(value);
      strings[key] = value;
    }
  }

  return true;
}

bool ReadStringsFromFile(const string& path, StringTable& strings,
                         bool unescape) {
  ifstream* f = UIOpenRead(path, ios::in);
  bool success = false;
  if (f->is_open()) {
    success = ReadStrings(*f, strings, unescape);
    f->close();
  }

  delete f;
  return success;
}

static bool ReadExtraFile(const string& aExtraDataPath, Json::Value& aExtra) {
  bool success = false;
  ifstream* f = UIOpenRead(aExtraDataPath, ios::in);
  if (f->is_open()) {
    Json::CharReaderBuilder builder;
    success = parseFromStream(builder, *f, &aExtra, nullptr);
  }

  delete f;
  return success;
}

static string Basename(const string& file) {
  string::size_type slashIndex = file.rfind(UI_DIR_SEPARATOR);
  if (slashIndex != string::npos) {
    return file.substr(slashIndex + 1);
  }
  return file;
}

static string GetDumpLocalID() {
  string localId = Basename(gReporterDumpFile);
  string::size_type dot = localId.rfind('.');

  if (dot == string::npos) return "";

  return localId.substr(0, dot);
}

static bool ReadEventFile(const string& aPath, string& aEventVersion,
                          string& aTime, string& aUuid, Json::Value& aData) {
  bool res = false;
  ifstream* f = UIOpenRead(aPath, ios::binary);

  if (f->is_open()) {
    std::getline(*f, aEventVersion, '\n');
    res = f->good();
    std::getline(*f, aTime, '\n');
    res &= f->good();
    std::getline(*f, aUuid, '\n');
    res &= f->good();

    if (res) {
      Json::CharReaderBuilder builder;
      res = parseFromStream(builder, *f, &aData, nullptr);
    }
  }

  delete f;
  return res;
}

static void OverwriteEventFile(const string& aPath, const string& aEventVersion,
                               const string& aTime, const string& aUuid,
                               const Json::Value& aData) {
  ofstream* f = UIOpenWrite(aPath, ios::binary | ios::trunc);
  if (f->is_open()) {
    f->write(aEventVersion.c_str(), aEventVersion.length()) << '\n';
    f->write(aTime.c_str(), aTime.length()) << '\n';
    f->write(aUuid.c_str(), aUuid.length()) << '\n';

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(aData, f);
    *f << "\n";
  }

  delete f;
}

static void UpdateEventFile(const Json::Value& aExtraData, const string& aHash,
                            const string& aPingUuid) {
  if (gEventsPath.empty()) {
    // If there is no path for finding the crash event, skip this step.
    return;
  }

  string localId = GetDumpLocalID();
  string path = gEventsPath + UI_DIR_SEPARATOR + localId;
  string eventVersion;
  string crashTime;
  string crashUuid;
  Json::Value eventData;

  if (!ReadEventFile(path, eventVersion, crashTime, crashUuid, eventData)) {
    return;
  }

  if (!aHash.empty()) {
    eventData["MinidumpSha256Hash"] = aHash;
  }

  if (!aPingUuid.empty()) {
    eventData["CrashPingUUID"] = aPingUuid;
  }

  if (aExtraData.isMember("StackTraces")) {
    eventData["StackTraces"] = aExtraData["StackTraces"];
  }

  OverwriteEventFile(path, eventVersion, crashTime, crashUuid, eventData);
}

static void WriteSubmissionEvent(SubmissionResult result,
                                 const string& remoteId) {
  if (gEventsPath.empty()) {
    // If there is no path for writing the submission event, skip it.
    return;
  }

  string localId = GetDumpLocalID();
  string fpath = gEventsPath + UI_DIR_SEPARATOR + localId + "-submission";
  ofstream* f = UIOpenWrite(fpath, ios::binary);
  time_t tm;
  time(&tm);

  if (f->is_open()) {
    *f << "crash.submission.1\n";
    *f << tm << "\n";
    *f << localId << "\n";
    *f << (result == Succeeded ? "true" : "false") << "\n";
    *f << remoteId;

    f->close();
  }

  delete f;
}

void LogMessage(const std::string& message) {
  if (gLogStream.get()) {
    char date[64];
    time_t tm;
    time(&tm);
    if (strftime(date, sizeof(date) - 1, "%c", localtime(&tm)) == 0)
      date[0] = '\0';
    (*gLogStream) << "[" << date << "] " << message << std::endl;
  }
}

static void OpenLogFile() {
  string logPath = gSettingsPath + UI_DIR_SEPARATOR + "submit.log";
  gLogStream.reset(UIOpenWrite(logPath, ios::app));
}

static bool ReadConfig() {
  string iniPath;
  if (!UIGetIniPath(iniPath)) {
    return false;
  }

  if (!ReadStringsFromFile(iniPath, gStrings, true)) return false;

  // See if we have a string override file, if so process it
  char* overrideEnv = getenv("MOZ_CRASHREPORTER_STRINGS_OVERRIDE");
  if (overrideEnv && *overrideEnv && UIFileExists(overrideEnv))
    ReadStringsFromFile(overrideEnv, gStrings, true);

  return true;
}

static string GetAdditionalFilename(const string& dumpfile,
                                    const char* extension) {
  string filename(dumpfile);
  int dot = filename.rfind('.');
  if (dot < 0) return "";

  filename.replace(dot, filename.length() - dot, extension);
  return filename;
}

static bool MoveCrashData(const string& toDir, string& dumpfile,
                          string& extrafile, string& memoryfile) {
  if (!UIEnsurePathExists(toDir)) {
    UIError(gStrings[ST_ERROR_CREATEDUMPDIR]);
    return false;
  }

  string newDump = toDir + UI_DIR_SEPARATOR + Basename(dumpfile);
  string newExtra = toDir + UI_DIR_SEPARATOR + Basename(extrafile);
  string newMemory = toDir + UI_DIR_SEPARATOR + Basename(memoryfile);

  if (!UIMoveFile(dumpfile, newDump)) {
    UIError(gStrings[ST_ERROR_DUMPFILEMOVE]);
    return false;
  }

  if (!UIMoveFile(extrafile, newExtra)) {
    UIError(gStrings[ST_ERROR_EXTRAFILEMOVE]);
    return false;
  }

  if (!memoryfile.empty()) {
    // Ignore errors from moving the memory file
    if (!UIMoveFile(memoryfile, newMemory)) {
      UIDeleteFile(memoryfile);
      newMemory.erase();
    }
    memoryfile = newMemory;
  }

  dumpfile = newDump;
  extrafile = newExtra;

  return true;
}

static bool AddSubmittedReport(const string& serverResponse) {
  StringTable responseItems;
  istringstream in(serverResponse);
  ReadStrings(in, responseItems, false);

  if (responseItems.find("StopSendingReportsFor") != responseItems.end()) {
    // server wants to tell us to stop sending reports for a certain version
    string reportPath = gSettingsPath + UI_DIR_SEPARATOR + "EndOfLife" +
                        responseItems["StopSendingReportsFor"];

    ofstream* reportFile = UIOpenWrite(reportPath, ios::trunc);
    if (reportFile->is_open()) {
      // don't really care about the contents
      *reportFile << 1 << "\n";
      reportFile->close();
    }
    delete reportFile;
  }

  if (responseItems.find("Discarded") != responseItems.end()) {
    // server discarded this report... save it so the user can resubmit it
    // manually
    return false;
  }

  if (responseItems.find("CrashID") == responseItems.end()) return false;

  string submittedDir = gSettingsPath + UI_DIR_SEPARATOR + "submitted";
  if (!UIEnsurePathExists(submittedDir)) {
    return false;
  }

  string path =
      submittedDir + UI_DIR_SEPARATOR + responseItems["CrashID"] + ".txt";

  ofstream* file = UIOpenWrite(path, ios::trunc);
  if (!file->is_open()) {
    delete file;
    return false;
  }

  char buf[1024];
  UI_SNPRINTF(buf, 1024, gStrings["CrashID"].c_str(),
              responseItems["CrashID"].c_str());
  *file << buf << "\n";

  if (responseItems.find("ViewURL") != responseItems.end()) {
    UI_SNPRINTF(buf, 1024, gStrings["CrashDetailsURL"].c_str(),
                responseItems["ViewURL"].c_str());
    *file << buf << "\n";
  }

  file->close();
  delete file;

  WriteSubmissionEvent(Succeeded, responseItems["CrashID"]);
  return true;
}

void DeleteDump() {
  const char* noDelete = getenv("MOZ_CRASHREPORTER_NO_DELETE_DUMP");
  if (!noDelete || *noDelete == '\0') {
    if (!gReporterDumpFile.empty()) UIDeleteFile(gReporterDumpFile);
    if (!gExtraFile.empty()) UIDeleteFile(gExtraFile);
    if (!gMemoryFile.empty()) UIDeleteFile(gMemoryFile);
  }
}

void SendCompleted(bool success, const string& serverResponse) {
  if (success) {
    if (AddSubmittedReport(serverResponse)) {
      DeleteDump();
    } else {
      string directory = gReporterDumpFile;
      int slashpos = directory.find_last_of("/\\");
      if (slashpos < 2) return;
      directory.resize(slashpos);
      UIPruneSavedDumps(directory);
      WriteSubmissionEvent(Failed, "");
    }
  } else {
    WriteSubmissionEvent(Failed, "");
  }
}

static string ComputeDumpHash() {
#ifdef XP_LINUX
  // On Linux we rely on the system-provided libcurl which uses nss so we have
  // to also use the system-provided nss instead of the ones we have bundled.
  const char* libnssNames[] = {
      "libnss3.so",
#  ifndef HAVE_64BIT_BUILD
      // 32-bit versions on 64-bit hosts
      "/usr/lib32/libnss3.so",
#  endif
  };
  void* lib = nullptr;

  for (const char* libname : libnssNames) {
    lib = dlopen(libname, RTLD_NOW);

    if (lib) {
      break;
    }
  }

  if (!lib) {
    return "";
  }

  SECStatus (*NSS_Initialize)(const char*, const char*, const char*,
                              const char*, PRUint32);
  HASHContext* (*HASH_Create)(HASH_HashType);
  void (*HASH_Destroy)(HASHContext*);
  void (*HASH_Begin)(HASHContext*);
  void (*HASH_Update)(HASHContext*, const unsigned char*, unsigned int);
  void (*HASH_End)(HASHContext*, unsigned char*, unsigned int*, unsigned int);

  *(void**)(&NSS_Initialize) = dlsym(lib, "NSS_Initialize");
  *(void**)(&HASH_Create) = dlsym(lib, "HASH_Create");
  *(void**)(&HASH_Destroy) = dlsym(lib, "HASH_Destroy");
  *(void**)(&HASH_Begin) = dlsym(lib, "HASH_Begin");
  *(void**)(&HASH_Update) = dlsym(lib, "HASH_Update");
  *(void**)(&HASH_End) = dlsym(lib, "HASH_End");

  if (!HASH_Create || !HASH_Destroy || !HASH_Begin || !HASH_Update ||
      !HASH_End) {
    return "";
  }
#endif
  // Minimal NSS initialization so we can use the hash functions
  const PRUint32 kNssFlags = NSS_INIT_READONLY | NSS_INIT_NOROOTINIT |
                             NSS_INIT_NOMODDB | NSS_INIT_NOCERTDB;
  if (NSS_Initialize(nullptr, "", "", "", kNssFlags) != SECSuccess) {
    return "";
  }

  HASHContext* hashContext = HASH_Create(HASH_AlgSHA256);

  if (!hashContext) {
    return "";
  }

  HASH_Begin(hashContext);

  ifstream* f = UIOpenRead(gReporterDumpFile, ios::binary);
  bool error = false;

  // Read the minidump contents
  if (f->is_open()) {
    uint8_t buff[4096];

    do {
      f->read((char*)buff, sizeof(buff));

      if (f->bad()) {
        error = true;
        break;
      }

      HASH_Update(hashContext, buff, f->gcount());
    } while (!f->eof());

    f->close();
  } else {
    error = true;
  }

  delete f;

  // Finalize the hash computation
  uint8_t result[SHA256_LENGTH];
  uint32_t resultLen = 0;

  HASH_End(hashContext, result, &resultLen, SHA256_LENGTH);

  if (resultLen != SHA256_LENGTH) {
    error = true;
  }

  HASH_Destroy(hashContext);

  if (!error) {
    ostringstream hash;

    for (size_t i = 0; i < SHA256_LENGTH; i++) {
      hash << std::setw(2) << std::setfill('0') << std::hex
           << static_cast<unsigned int>(result[i]);
    }

    return hash.str();
  }
  return "";  // If we encountered an error, return an empty hash
}

}  // namespace CrashReporter

using namespace CrashReporter;

Json::Value kEmptyJsonString("");

void RewriteStrings(Json::Value& aExtraData) {
  // rewrite some UI strings with the values from the query parameters
  string product = aExtraData.get("ProductName", kEmptyJsonString).asString();
  Json::Value mozilla("Mozilla");
  string vendor = aExtraData.get("Vendor", mozilla).asString();

  char buf[4096];
  UI_SNPRINTF(buf, sizeof(buf), gStrings[ST_CRASHREPORTERVENDORTITLE].c_str(),
              vendor.c_str());
  gStrings[ST_CRASHREPORTERTITLE] = buf;

  string str = gStrings[ST_CRASHREPORTERPRODUCTERROR];
  // Only do the replacement here if the string has two
  // format specifiers to start.  Otherwise
  // we assume it has the product name hardcoded.
  string::size_type pos = str.find("%s");
  if (pos != string::npos) pos = str.find("%s", pos + 2);
  if (pos != string::npos) {
    // Leave a format specifier for UIError to fill in
    UI_SNPRINTF(buf, sizeof(buf),
                gStrings[ST_CRASHREPORTERPRODUCTERROR].c_str(), product.c_str(),
                "%s");
    gStrings[ST_CRASHREPORTERERROR] = buf;
  } else {
    // product name is hardcoded
    gStrings[ST_CRASHREPORTERERROR] = str;
  }

  UI_SNPRINTF(buf, sizeof(buf), gStrings[ST_CRASHREPORTERDESCRIPTION].c_str(),
              product.c_str());
  gStrings[ST_CRASHREPORTERDESCRIPTION] = buf;

  UI_SNPRINTF(buf, sizeof(buf), gStrings[ST_CHECKSUBMIT].c_str(),
              vendor.c_str());
  gStrings[ST_CHECKSUBMIT] = buf;

  UI_SNPRINTF(buf, sizeof(buf), gStrings[ST_RESTART].c_str(), product.c_str());
  gStrings[ST_RESTART] = buf;

  UI_SNPRINTF(buf, sizeof(buf), gStrings[ST_QUIT].c_str(), product.c_str());
  gStrings[ST_QUIT] = buf;

  UI_SNPRINTF(buf, sizeof(buf), gStrings[ST_ERROR_ENDOFLIFE].c_str(),
              product.c_str());
  gStrings[ST_ERROR_ENDOFLIFE] = buf;
}

bool CheckEndOfLifed(const Json::Value& aVersion) {
  if (!aVersion.isString()) {
    return false;
  }

  string reportPath =
      gSettingsPath + UI_DIR_SEPARATOR + "EndOfLife" + aVersion.asString();
  return UIFileExists(reportPath);
}

static string GetProgramPath(const string& exename) {
  string path = gArgv[0];
  size_t pos = path.rfind(UI_CRASH_REPORTER_FILENAME BIN_SUFFIX);
  path.erase(pos);
#ifdef XP_MACOSX
  // On macOS the crash reporter client is shipped as an application bundle
  // contained within Firefox' main application bundle. So when it's invoked
  // its current working directory looks like:
  // Firefox.app/Contents/MacOS/crashreporter.app/Contents/MacOS/
  // The other applications we ship with Firefox are stored in the main bundle
  // (Firefox.app/Contents/MacOS/) so we we need to go back three directories
  // to reach them.
  path.append("../../../");
#endif  // XP_MACOSX
  path.append(exename + BIN_SUFFIX);

  return path;
}

int main(int argc, char** argv) {
  gArgc = argc;
  gArgv = argv;

  string autoSubmitEnv = UIGetEnv("MOZ_CRASHREPORTER_AUTO_SUBMIT");
  gAutoSubmit = !autoSubmitEnv.empty();

  if (!ReadConfig()) {
    UIError("Couldn't read configuration.");
    return 0;
  }

  if (!UIInit()) {
    return 0;
  }

  if (argc > 1) {
    gReporterDumpFile = argv[1];
  }

  if (gReporterDumpFile.empty()) {
    // no dump file specified, run the default UI
    if (!gAutoSubmit) {
      UIShowDefaultUI();
    }
  } else {
    // Start by running minidump analyzer to gather stack traces.
    string reporterDumpFile = gReporterDumpFile;
    vector<string> args = {reporterDumpFile};
    string dumpAllThreadsEnv = UIGetEnv("MOZ_CRASHREPORTER_DUMP_ALL_THREADS");
    if (!dumpAllThreadsEnv.empty()) {
      args.insert(args.begin(), "--full");
    }
    UIRunProgram(GetProgramPath(UI_MINIDUMP_ANALYZER_FILENAME), args,
                 /* wait */ true);

    // go ahead with the crash reporter
    gExtraFile = GetAdditionalFilename(gReporterDumpFile, kExtraDataExtension);
    if (gExtraFile.empty()) {
      UIError(gStrings[ST_ERROR_BADARGUMENTS]);
      return 0;
    }

    if (!UIFileExists(gExtraFile)) {
      UIError(gStrings[ST_ERROR_EXTRAFILEEXISTS]);
      return 0;
    }

    gMemoryFile =
        GetAdditionalFilename(gReporterDumpFile, kMemoryReportExtension);
    if (!UIFileExists(gMemoryFile)) {
      gMemoryFile.erase();
    }

    Json::Value extraData;
    if (!ReadExtraFile(gExtraFile, extraData)) {
      UIError(gStrings[ST_ERROR_EXTRAFILEREAD]);
      return 0;
    }

    if (!extraData.isMember("ProductName")) {
      UIError(gStrings[ST_ERROR_NOPRODUCTNAME]);
      return 0;
    }

    // There is enough information in the extra file to rewrite strings
    // to be product specific
    RewriteStrings(extraData);

    if (!extraData.isMember("ServerURL")) {
      UIError(gStrings[ST_ERROR_NOSERVERURL]);
      return 0;
    }

    // Hopefully the settings path exists in the environment. Try that before
    // asking the platform-specific code to guess.
    gSettingsPath = UIGetEnv("MOZ_CRASHREPORTER_DATA_DIRECTORY");
    if (gSettingsPath.empty()) {
      string product =
          extraData.get("ProductName", kEmptyJsonString).asString();
      string vendor = extraData.get("Vendor", kEmptyJsonString).asString();

      if (!UIGetSettingsPath(vendor, product, gSettingsPath)) {
        gSettingsPath.clear();
      }
    }

    if (gSettingsPath.empty() || !UIEnsurePathExists(gSettingsPath)) {
      UIError(gStrings[ST_ERROR_NOSETTINGSPATH]);
      return 0;
    }

    OpenLogFile();

    gEventsPath = UIGetEnv("MOZ_CRASHREPORTER_EVENTS_DIRECTORY");
    gPingPath = UIGetEnv("MOZ_CRASHREPORTER_PING_DIRECTORY");

    // Assemble and send the crash ping
    string hash = ComputeDumpHash();

    string pingUuid;
    SendCrashPing(extraData, hash, pingUuid, gPingPath);
    UpdateEventFile(extraData, hash, pingUuid);

    if (!UIFileExists(gReporterDumpFile)) {
      UIError(gStrings[ST_ERROR_DUMPFILEEXISTS]);
      return 0;
    }

    string pendingDir = gSettingsPath + UI_DIR_SEPARATOR + "pending";
    if (!MoveCrashData(pendingDir, gReporterDumpFile, gExtraFile,
                       gMemoryFile)) {
      return 0;
    }

    string sendURL = extraData.get("ServerURL", kEmptyJsonString).asString();
    // we don't need to actually send these
    extraData.removeMember("ServerURL");
    extraData.removeMember("StackTraces");

    extraData["Throttleable"] = "1";

    // re-set XUL_APP_FILE for xulrunner wrapped apps
    const char* appfile = getenv("MOZ_CRASHREPORTER_RESTART_XUL_APP_FILE");
    if (appfile && *appfile) {
      const char prefix[] = "XUL_APP_FILE=";
      char* env = (char*)malloc(strlen(appfile) + strlen(prefix) + 1);
      if (!env) {
        UIError("Out of memory");
        return 0;
      }
      strcpy(env, prefix);
      strcat(env, appfile);
      putenv(env);
      free(env);
    }

    vector<string> restartArgs;

    ostringstream paramName;
    int i = 0;
    paramName << "MOZ_CRASHREPORTER_RESTART_ARG_" << i++;
    const char* param = getenv(paramName.str().c_str());
    while (param && *param) {
      restartArgs.push_back(param);

      paramName.str("");
      paramName << "MOZ_CRASHREPORTER_RESTART_ARG_" << i++;
      param = getenv(paramName.str().c_str());
    }

    // allow override of the server url via environment variable
    // XXX: remove this in the far future when our robot
    // masters force everyone to use XULRunner
    char* urlEnv = getenv("MOZ_CRASHREPORTER_URL");
    if (urlEnv && *urlEnv) {
      sendURL = urlEnv;
    }

    // see if this version has been end-of-lifed

    if (extraData.isMember("Version") &&
        CheckEndOfLifed(extraData["Version"])) {
      UIError(gStrings[ST_ERROR_ENDOFLIFE]);
      DeleteDump();
      return 0;
    }

    StringTable files;
    files["upload_file_minidump"] = gReporterDumpFile;
    if (!gMemoryFile.empty()) {
      files["memory_report"] = gMemoryFile;
    }

    if (!UIShowCrashUI(files, extraData, sendURL, restartArgs)) {
      DeleteDump();
    }
  }

  UIShutdown();

  return 0;
}

#if defined(XP_WIN) && !defined(__GNUC__)
#  include <windows.h>

// We need WinMain in order to not be a console app.  This function is unused
// if we are a console application.
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR args, int) {
  // Remove everything except close window from the context menu
  {
    HKEY hkApp;
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\Applications", 0,
                    nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr,
                    &hkApp, nullptr);
    RegCloseKey(hkApp);
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        L"Software\\Classes\\Applications\\crashreporter.exe",
                        0, nullptr, REG_OPTION_VOLATILE, KEY_SET_VALUE, nullptr,
                        &hkApp, nullptr) == ERROR_SUCCESS) {
      RegSetValueExW(hkApp, L"IsHostApp", 0, REG_NONE, 0, 0);
      RegSetValueExW(hkApp, L"NoOpenWith", 0, REG_NONE, 0, 0);
      RegSetValueExW(hkApp, L"NoStartPage", 0, REG_NONE, 0, 0);
      RegCloseKey(hkApp);
    }
  }

  char** argv = static_cast<char**>(malloc(__argc * sizeof(char*)));
  for (int i = 0; i < __argc; i++) {
    argv[i] = strdup(WideToUTF8(__wargv[i]).c_str());
  }

  // Do the real work.
  return main(__argc, argv);
}
#endif
