/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Toolkit Crash Reporter
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2006-2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Ted Mielczarek <ted.mielczarek@gmail.com>
 *  Dave Camp <dcamp@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "crashreporter.h"

#ifdef _MSC_VER
// Disable exception handler warnings.
# pragma warning( disable : 4530 )
#endif

#include <fstream>
#include <sstream>

using std::string;
using std::istream;
using std::ifstream;
using std::istringstream;
using std::ostringstream;
using std::ostream;
using std::ofstream;
using std::vector;

StringTable  gStrings;
int          gArgc;
const char** gArgv;

static string       gDumpFile;
static string       gExtraFile;
static string       gSettingsPath;

static string kExtraDataExtension = ".extra";

static string Unescape(const string& str)
{
  string ret;
  for (string::const_iterator iter = str.begin();
       iter != str.end();
       iter++) {
    if (*iter == '\\') {
      iter++;
      if (*iter == '\\'){
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

static bool ReadStrings(istream& in, StringTable& strings, bool unescape)
{
  string currentSection;
  while (!in.eof()) {
    string line;
    std::getline(in, line);
    int sep = line.find('=');
    if (sep >= 0) {
      string key, value;
      key = line.substr(0, sep);
      value = line.substr(sep + 1);
      if (unescape)
        value = Unescape(value);
      strings[key] = value;
    }
  }

  return true;
}

static bool ReadStringsFromFile(const string& path,
                                StringTable& strings,
                                bool unescape)
{
  ifstream f(path.c_str(), std::ios::in);
  if (!f.is_open()) return false;

  return ReadStrings(f, strings, unescape);
}

static bool ReadConfig()
{
  string iniPath;
  if (!UIGetIniPath(iniPath))
    return false;

  if (!ReadStringsFromFile(iniPath, gStrings, true))
    return false;

  return true;
}

static string GetExtraDataFilename(const string& dumpfile)
{
  string filename(dumpfile);
  int dot = filename.rfind('.');
  if (dot < 0)
    return "";

  filename.replace(dot, filename.length() - dot, kExtraDataExtension);
  return filename;
}

static string Basename(const string& file)
{
  int slashIndex = file.rfind(UI_DIR_SEPARATOR);
  if (slashIndex >= 0)
    return file.substr(slashIndex + 1);
  else
    return file;
}

static bool MoveCrashData(const string& toDir,
                          string& dumpfile,
                          string& extrafile)
{
  if (!UIEnsurePathExists(toDir)) {
    return false;
  }

  string newDump = toDir + UI_DIR_SEPARATOR + Basename(dumpfile);
  string newExtra = toDir + UI_DIR_SEPARATOR + Basename(extrafile);

  if (!UIMoveFile(dumpfile, newDump) ||
      !UIMoveFile(extrafile, newExtra)) {
    return false;
  }

  dumpfile = newDump;
  extrafile = newExtra;

  return true;
}

static bool AddSubmittedReport(const string& serverResponse)
{
  StringTable responseItems;
  istringstream in(serverResponse);
  ReadStrings(in, responseItems, false);

  if (responseItems.find("CrashID") == responseItems.end())
    return false;

  string submittedDir =
    gSettingsPath + UI_DIR_SEPARATOR + "submitted";
  if (!UIEnsurePathExists(submittedDir)) {
    return false;
  }

  string path = submittedDir + UI_DIR_SEPARATOR +
    responseItems["CrashID"] + ".txt";

  ofstream file(path.c_str());
  if (!file.is_open())
    return false;

  char buf[1024];
  UI_SNPRINTF(buf, 1024,
              gStrings["CrashID"].c_str(),
              responseItems["CrashID"].c_str());
  file << buf << "\n";

  if (responseItems.find("ViewURL") != responseItems.end()) {
    UI_SNPRINTF(buf, 1024,
                gStrings["ViewURL"].c_str(),
                responseItems["ViewURL"].c_str());
    file << buf << "\n";
  }

  file.close();

  return true;

}

bool CrashReporterSendCompleted(bool success,
                                const string& serverResponse)
{
  if (success) {
    const char* noDelete = getenv("MOZ_CRASHREPORTER_NO_DELETE_DUMP");
    if (!noDelete || *noDelete == '\0') {
      if (!gDumpFile.empty())
        UIDeleteFile(gDumpFile);
      if (!gExtraFile.empty())
        UIDeleteFile(gExtraFile);
    }

    return AddSubmittedReport(serverResponse);
  }
  return true;
}

int main(int argc, const char** argv)
{
  gArgc = argc;
  gArgv = argv;

  if (!ReadConfig()) {
    UIError("Couldn't read configuration");
    return 0;
  }

  if (!UIInit())
    return 0;

  if (argc > 1) {
    gDumpFile = argv[1];
  }

  if (gDumpFile.empty()) {
    // no dump file specified, run the default UI
    UIShowDefaultUI();
  } else {
    gExtraFile = GetExtraDataFilename(gDumpFile);
    if (gExtraFile.empty()) {
      UIError("Couldn't get extra data filename");
      return 0;
    }

    StringTable queryParameters;
    if (!ReadStringsFromFile(gExtraFile, queryParameters, true)) {
      UIError("Couldn't read extra data");
      return 0;
    }

    if (queryParameters.find("ProductName") == queryParameters.end()) {
      UIError("No product name specified");
      return 0;
    }

    if (queryParameters.find("ServerURL") == queryParameters.end()) {
      UIError("No server URL specified");
      return 0;
    }

    string product = queryParameters["ProductName"];
    string vendor = queryParameters["Vendor"];
    if (!UIGetSettingsPath(vendor, product, gSettingsPath)) {
      UIError("Couldn't get settings path");
      return 0;
    }

    string pendingDir = gSettingsPath + UI_DIR_SEPARATOR + "pending";
    if (!MoveCrashData(pendingDir, gDumpFile, gExtraFile)) {
      UIError("Couldn't move crash data");
      return 0;
    }

    string sendURL = queryParameters["ServerURL"];
    // we don't need to actually send this
    queryParameters.erase("ServerURL");

    vector<string> restartArgs;

    ostringstream paramName;
    int i = 0;
    paramName << "MOZ_CRASHREPORTER_RESTART_ARG_" << i++;
    const char *param = getenv(paramName.str().c_str());
    while (param && *param) {
      restartArgs.push_back(param);

      paramName << "MOZ_CRASHREPORTER_RESTART_ARG_" << i++;
      param = getenv(paramName.str().c_str());
    };

    // allow override of the server url via environment variable
    //XXX: remove this in the far future when our robot
    // masters force everyone to use XULRunner
    char* urlEnv = getenv("MOZ_CRASHREPORTER_URL");
    if (urlEnv && *urlEnv) {
      sendURL = urlEnv;
    }

    // rewrite some UI strings with the values from the query parameters
    char buf[4096];
    UI_SNPRINTF(buf, sizeof(buf),
                gStrings[ST_RESTART].c_str(),
                product.c_str());
    gStrings[ST_RESTART] = buf;

    UI_SNPRINTF(buf, sizeof(buf),
                gStrings[ST_CRASHREPORTERDESCRIPTION].c_str(),
                product.c_str());
    gStrings[ST_CRASHREPORTERDESCRIPTION] = buf;

    UI_SNPRINTF(buf, sizeof(buf),
                gStrings[ST_CHECKSUBMIT].c_str(),
                vendor.empty() ? "Mozilla" : vendor.c_str());
    gStrings[ST_CHECKSUBMIT] = buf;

    UI_SNPRINTF(buf, sizeof(buf),
                gStrings[ST_CRASHREPORTERTITLE].c_str(),
                vendor.empty() ? "Mozilla" : vendor.c_str());
    gStrings[ST_CRASHREPORTERTITLE] = buf;

    UIShowCrashUI(gDumpFile, queryParameters, sendURL, restartArgs);
  }

  UIShutdown();

  return 0;
}

#if defined(XP_WIN) && !defined(__GNUC__)
// We need WinMain in order to not be a console app.  This function is unused
// if we are a console application.
int WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR args, int )
{
  // Do the real work.
  return main(__argc, (const char**)__argv);
}
#endif
