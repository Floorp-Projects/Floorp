/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// GTK headers
#include <gtk/gtk.h>

// Linux headers
#include <fcntl.h>
#include <unistd.h>

// Mozilla headers
#include "nsIFile.h"
#include "nsINIParser.h"
#include "nsXPCOMGlue.h"
#include "nsXPCOMPrivate.h"              // for MAXPATHLEN and XPCOM_DLL
#include "nsXULAppAPI.h"
#include "BinaryPath.h"

const char kAPP_INI[] = "application.ini";
const char kWEBAPP_INI[] = "webapp.ini";
const char kWEBAPPRT_INI[] = "webapprt.ini";
const char kWEBAPPRT_PATH[] = "webapprt";
const char kAPP_ENV_VAR[] = "XUL_APP_FILE";
const char kAPP_RT[] = "webapprt-stub";

int* pargc;
char*** pargv;
char profile[MAXPATHLEN];
bool isProfileOverridden = false;

XRE_GetFileFromPathType XRE_GetFileFromPath;
XRE_CreateAppDataType XRE_CreateAppData;
XRE_FreeAppDataType XRE_FreeAppData;
XRE_mainType XRE_main;

const nsDynamicFunctionLoad kXULFuncs[] = {
  { "XRE_GetFileFromPath", (NSFuncPtr*) &XRE_GetFileFromPath },
  { "XRE_CreateAppData", (NSFuncPtr*) &XRE_CreateAppData },
  { "XRE_FreeAppData", (NSFuncPtr*) &XRE_FreeAppData },
  { "XRE_main", (NSFuncPtr*) &XRE_main },
  { nullptr, nullptr }
};

class ScopedLogging
{
  public:
    ScopedLogging() { NS_LogInit(); }
    ~ScopedLogging() { NS_LogTerm(); }
};

// Copied from toolkit/xre/nsAppData.cpp.
void SetAllocatedString(const char *&str, const char *newvalue)
{
  NS_Free(const_cast<char*>(str));
  if (newvalue) {
    str = NS_strdup(newvalue);
  }
  else {
    str = nullptr;
  }
}

// Function to open a dialog box displaying the message provided
void ErrorDialog(const char* message)
{
  gtk_init(pargc, pargv);

  GtkWidget* dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", message);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

// Function to get the parent dir of a file
void GetDirFromPath(char* parentDir, char* filePath)
{
  char* base = strrchr(filePath, '/');

  if (!base) {
    strcpy(parentDir, ".");
  }
  else {
    while (base > filePath && *base == '/')
      base--;

    int len = 1 + base - filePath;
    strncpy(parentDir, filePath, len);
    parentDir[len] = '\0';
  }
}

bool CopyFile(const char* inputFile, const char* outputFile)
{
  // Open input file
  int inputFD = open(inputFile, O_RDONLY);
  if (!inputFD)
    return false;

  // Open output file
  int outputFD = creat(outputFile, S_IRWXU);
  if (!outputFD)
    return false;

  // Copy file
  char buf[BUFSIZ];
  ssize_t bytesRead;

  while ((bytesRead = read(inputFD, buf, BUFSIZ)) > 0) {
    ssize_t bytesWritten = write(outputFD, buf, bytesRead);
    if (bytesWritten < 0) {
      bytesRead = -1;
      break;
    }
  }

  // Close file descriptors
  close(inputFD);
  close(outputFD);

  return (bytesRead >= 0);
}

bool GRELoadAndLaunch(const char* firefoxDir)
{
  char xpcomDllPath[MAXPATHLEN];
  snprintf(xpcomDllPath, MAXPATHLEN, "%s/%s", firefoxDir, XPCOM_DLL);

  if (NS_FAILED(XPCOMGlueStartup(xpcomDllPath))) {
    ErrorDialog("Couldn't load the XPCOM library");
    return false;
  }

  if (NS_FAILED(XPCOMGlueLoadXULFunctions(kXULFuncs))) {
    ErrorDialog("Couldn't load libxul");
    return false;
  }

  if (!isProfileOverridden) {
    // Override the class name part of the WM_CLASS property, so that the
    // DE can match our window to the correct launcher
    char programClass[MAXPATHLEN];
    snprintf(programClass, MAXPATHLEN, "owa-%s", profile);
    g_set_prgname(programClass);
  }

  // NOTE: The GRE has successfully loaded, so we can use XPCOM now
  { // Scope for any XPCOM stuff we create
    ScopedLogging log;

    // Get the path to the runtime
    char rtPath[MAXPATHLEN];
    snprintf(rtPath, MAXPATHLEN, "%s/%s", firefoxDir, kWEBAPPRT_PATH);

    // Get the path to the runtime's INI file
    char rtIniPath[MAXPATHLEN];
    snprintf(rtIniPath, MAXPATHLEN, "%s/%s", rtPath, kWEBAPPRT_INI);

    // Load the runtime's INI from its path
    nsCOMPtr<nsIFile> rtINI;
    if (NS_FAILED(XRE_GetFileFromPath(rtIniPath, getter_AddRefs(rtINI)))) {
      ErrorDialog("Couldn't load the runtime INI");
      return false;
    }

    bool exists;
    nsresult rv = rtINI->Exists(&exists);
    if (NS_FAILED(rv) || !exists) {
      ErrorDialog("The runtime INI doesn't exist");
      return false;
    }

    nsXREAppData *webShellAppData;
    if (NS_FAILED(XRE_CreateAppData(rtINI, &webShellAppData))) {
      ErrorDialog("Couldn't read WebappRT application.ini");
      return false;
    }

    if (!isProfileOverridden) {
      SetAllocatedString(webShellAppData->profile, profile);
      SetAllocatedString(webShellAppData->name, profile);
    }

    nsCOMPtr<nsIFile> directory;
    if (NS_FAILED(XRE_GetFileFromPath(rtPath, getter_AddRefs(directory)))) {
      ErrorDialog("Couldn't open runtime directory");
      return false;
    }

    nsCOMPtr<nsIFile> xreDir;
    if (NS_FAILED(XRE_GetFileFromPath(firefoxDir, getter_AddRefs(xreDir)))) {
      ErrorDialog("Couldn't open XRE directory");
      return false;
    }

    xreDir.forget(&webShellAppData->xreDirectory);
    NS_IF_RELEASE(webShellAppData->directory);
    directory.forget(&webShellAppData->directory);

    XRE_main(*pargc, *pargv, webShellAppData, 0);

    XRE_FreeAppData(webShellAppData);
  }

  return true;
}

void CopyAndRelaunch(const char* firefoxDir, const char* curExePath)
{
  char newExePath[MAXPATHLEN];
  snprintf(newExePath, MAXPATHLEN, "%s/%s", firefoxDir, kAPP_RT);

  if (unlink(curExePath) == -1) {
    ErrorDialog("Couldn't remove the old webapprt-stub executable");
    return;
  }

  if (!CopyFile(newExePath, curExePath)) {
    ErrorDialog("Couldn't copy the new webapprt-stub executable");
    return;
  }

  execv(curExePath, *pargv);

  ErrorDialog("Couldn't execute the new webapprt-stub executable");
}

int main(int argc, char *argv[])
{
  pargc = &argc;
  pargv = &argv;

  // Get current executable path
  char curExePath[MAXPATHLEN];
  if (NS_FAILED(mozilla::BinaryPath::Get(argv[0], curExePath))) {
    ErrorDialog("Couldn't read current executable path");
    return 255;
  }
  char curExeDir[MAXPATHLEN];
  GetDirFromPath(curExeDir, curExePath);

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-profile")) {
      isProfileOverridden = true;
      break;
    }
  }

  char firefoxDir[MAXPATHLEN];

  // Check if Firefox is in the ../../dist/bin directory (relative to the webapp runtime)
  // This is the case for webapprt chrome and content tests.
  snprintf(firefoxDir, MAXPATHLEN, "%s/../../dist/bin", curExeDir);
  if (access(firefoxDir, F_OK) != -1) {
    if (GRELoadAndLaunch(firefoxDir))
      return 0;

    return 255;
  }

  // Set up webAppIniPath with path to webapp.ini
  char webAppIniPath[MAXPATHLEN];
  snprintf(webAppIniPath, MAXPATHLEN, "%s/%s", curExeDir, kWEBAPP_INI);

  // Open webapp.ini as an INI file
  nsINIParser parser;
  if (NS_FAILED(parser.Init(webAppIniPath))) {
    ErrorDialog("Couldn't open webapp.ini");
    return 255;
  }

  // Set up our environment to know where webapp.ini was loaded from
  if (setenv(kAPP_ENV_VAR, webAppIniPath, 1) == -1) {
    ErrorDialog("Couldn't set up app environment");
    return 255;
  }

  // Get profile dir from webapp.ini
  if (NS_FAILED(parser.GetString("Webapp", "Profile", profile, MAXPATHLEN))) {
    ErrorDialog("Couldn't retrieve profile from web app INI file");
    return 255;
  }

  // Get the location of Firefox from our webapp.ini
  if (NS_FAILED(parser.GetString("WebappRT", "InstallDir", firefoxDir, MAXPATHLEN))) {
    ErrorDialog("Couldn't find your Firefox install directory.");
    return 255;
  }

  // Set up appIniPath with path to application.ini.
  // This is in the Firefox installation directory.
  char appIniPath[MAXPATHLEN];
  snprintf(appIniPath, MAXPATHLEN, "%s/%s", firefoxDir, kAPP_INI);

  if (NS_FAILED(parser.Init(appIniPath))) {
    ErrorDialog("This app requires that Firefox version 16 or above is installed."
                " Firefox 16+ has not been detected.");
    return 255;
  }

  // Get buildid of Firefox we're trying to load (MAXPATHLEN only for convenience)
  char buildid[MAXPATHLEN];
  if (NS_FAILED(parser.GetString("App", "BuildID", buildid, MAXPATHLEN))) {
    ErrorDialog("Couldn't read BuildID from Firefox application.ini");
    return 255;
  }

  // If WebAppRT version == Firefox version, load XUL and execute the application
  if (!strcmp(buildid, NS_STRINGIFY(GRE_BUILDID))) {
    if (GRELoadAndLaunch(firefoxDir))
      return 0;
  }
  // Else, copy WebAppRT from Firefox installation and re-execute the process
  else
    CopyAndRelaunch(firefoxDir, curExePath);

  return 255;
}
