/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// GTK headers
#include <gtk/gtk.h>

// Linux headers
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>

// Mozilla headers
#include "nsIFile.h"
#include "nsINIParser.h"
#include "nsXPCOMGlue.h"
#include "nsXPCOMPrivate.h"              // for MAXPATHLEN and XPCOM_DLL
#include "nsXULAppAPI.h"
#include "BinaryPath.h"

const char kAPP_INI[] = "application.ini";
const char kWEBAPP_INI[] = "webapp.ini";
const char kWEBAPP_JSON[] = "webapp.json";
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
  gtk_window_set_title(GTK_WINDOW(dialog), "Error launching webapp");
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), false);
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

bool GRELoadAndLaunch(const char* firefoxDir, bool silentFail)
{
  char xpcomDllPath[MAXPATHLEN];
  snprintf(xpcomDllPath, MAXPATHLEN, "%s/%s", firefoxDir, XPCOM_DLL);

  if (silentFail && access(xpcomDllPath, F_OK) != 0)
    return false;

  if (NS_FAILED(XPCOMGlueStartup(xpcomDllPath))) {
    ErrorDialog("Couldn't load the XPCOM library");
    return false;
  }

  if (NS_FAILED(XPCOMGlueLoadXULFunctions(kXULFuncs))) {
    ErrorDialog("Couldn't load libxul");
    return false;
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
      // nsXREAppData::name is used for the class name part of the WM_CLASS
      // property. Set it so that the DE can match our window to the correct
      // launcher.
      char programClass[MAXPATHLEN];
      snprintf(programClass, MAXPATHLEN, "owa-%s", profile);
      SetAllocatedString(webShellAppData->name, programClass);
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

void RemoveApplication(nsINIParser& parser, const char* curExeDir, const char* profile)  {
  if (!isProfileOverridden) {
    // Remove the desktop entry file.
    char desktopEntryFilePath[MAXPATHLEN];

    char* dataDir = getenv("XDG_DATA_HOME");

    if (dataDir && *dataDir) {
      snprintf(desktopEntryFilePath, MAXPATHLEN, "%s/applications/owa-%s.desktop", dataDir, profile);
    } else {
      char* home = getenv("HOME");
      snprintf(desktopEntryFilePath, MAXPATHLEN, "%s/.local/share/applications/owa-%s.desktop", home, profile);
    }

    unlink(desktopEntryFilePath);
  }

  // Remove the files from the installation directory.
  char webAppIniPath[MAXPATHLEN];
  snprintf(webAppIniPath, MAXPATHLEN, "%s/%s", curExeDir, kWEBAPP_INI);
  unlink(webAppIniPath);

  char curExePath[MAXPATHLEN];
  snprintf(curExePath, MAXPATHLEN, "%s/%s", curExeDir, kAPP_RT);
  unlink(curExePath);

  char webAppJsonPath[MAXPATHLEN];
  snprintf(webAppJsonPath, MAXPATHLEN, "%s/%s", curExeDir, kWEBAPP_JSON);
  unlink(webAppJsonPath);

  char iconPath[MAXPATHLEN];
  snprintf(iconPath, MAXPATHLEN, "%s/icon.png", curExeDir);
  unlink(iconPath);

  char appName[MAXPATHLEN];
  if (NS_FAILED(parser.GetString("Webapp", "Name", appName, MAXPATHLEN))) {
    strcpy(appName, profile);
  }

  char uninstallMsg[MAXPATHLEN];
  if (NS_SUCCEEDED(parser.GetString("Webapp", "UninstallMsg", uninstallMsg, MAXPATHLEN))) {
    /**
     * The only difference between libnotify.so.4 and libnotify.so.1 for these symbols
     * is that notify_notification_new takes three arguments in libnotify.so.4 and
     * four in libnotify.so.1.
     * Passing the fourth argument as NULL is binary compatible.
     */
    typedef void  (*notify_init_t)(const char*);
    typedef void* (*notify_notification_new_t)(const char*, const char*, const char*, const char*);
    typedef void  (*notify_notification_show_t)(void*, void**);

    void *handle = dlopen("libnotify.so.4", RTLD_LAZY);
    if (!handle) {
      handle = dlopen("libnotify.so.1", RTLD_LAZY);
      if (!handle)
        return;
    }

    notify_init_t nn_init = (notify_init_t)(uintptr_t)dlsym(handle, "notify_init");
    notify_notification_new_t nn_new = (notify_notification_new_t)(uintptr_t)dlsym(handle, "notify_notification_new");
    notify_notification_show_t nn_show = (notify_notification_show_t)(uintptr_t)dlsym(handle, "notify_notification_show");
    if (!nn_init || !nn_new || !nn_show) {
      dlclose(handle);
      return;
    }

    nn_init(appName);

    void* n = nn_new(uninstallMsg, NULL, "dialog-information", NULL);

    nn_show(n, NULL);

    dlclose(handle);
  }
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

  bool removeApp = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-profile")) {
      isProfileOverridden = true;
    }
    else if (!strcmp(argv[i], "-remove")) {
      removeApp = true;
    }
  }

  char firefoxDir[MAXPATHLEN];

  // Check if Firefox is in the same directory as the webapp runtime.
  // This is the case for webapprt chrome and content tests.
  if (GRELoadAndLaunch(curExeDir, true)) {
    return 0;
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

  if (removeApp) {
    RemoveApplication(parser, curExeDir, profile);
    return 0;
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
    if (GRELoadAndLaunch(firefoxDir, false))
      return 0;
  }
  // Else, copy WebAppRT from Firefox installation and re-execute the process
  else
    CopyAndRelaunch(firefoxDir, curExePath);

  return 255;
}
