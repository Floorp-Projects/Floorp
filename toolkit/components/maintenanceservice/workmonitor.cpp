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
 * The Original Code is Maintenance service file system monitoring.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian R. Bondy <netzen@gmail.com>
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

#include <shlobj.h>
#include <shlwapi.h>
#include <wtsapi32.h>
#include <userenv.h>
#include <shellapi.h>

#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "shlwapi.lib")

#include "nsWindowsHelpers.h"
#include "nsAutoPtr.h"

#include "workmonitor.h"
#include "serviceinstall.h"
#include "servicebase.h"
#include "registrycertificates.h"
#include "uachelper.h"
#include "updatehelper.h"

extern BOOL gServiceStopping;

// Wait 15 minutes for an update operation to run at most.
// Updates usually take less than a minute so this seems like a 
// significantly large and safe amount of time to wait.
static const int TIME_TO_WAIT_ON_UPDATER = 15 * 60 * 1000;
PRUnichar* MakeCommandLine(int argc, PRUnichar **argv);
BOOL WriteStatusFailure(LPCWSTR updateDirPath, int errorCode);
BOOL PathGetSiblingFilePath(LPWSTR destinationBuffer,  LPCWSTR siblingFilePath, 
                            LPCWSTR newFileName);

// The error codes start from 16000 since Windows system error 
// codes only go up to 15999
const int SERVICE_UPDATER_COULD_NOT_BE_STARTED = 16000;
const int SERVICE_NOT_ENOUGH_COMMAND_LINE_ARGS = 16001;
const int SERVICE_UPDATER_SIGN_ERROR = 16002;
const int SERVICE_UPDATER_COMPARE_ERROR = 16003;
const int SERVICE_UPDATER_IDENTITY_ERROR = 16004;

/**
 * Runs an update process as the service using the SYSTEM account.
 *
 * @param  updaterPath    The path to the update process to start.
 * @param  workingDir     The working directory to execute the update process in
 * @param  cmdLine        The command line parameters to pass to updater.exe
 * @param  processStarted Set to TRUE if the process was started.
 * @return TRUE if the update process was run had a return code of 0.
 */
BOOL
StartUpdateProcess(LPCWSTR updaterPath, 
                   LPCWSTR workingDir, 
                   int argcTmp,
                   LPWSTR *argvTmp,
                   BOOL &processStarted)
{
  STARTUPINFO si = {0};
  si.cb = sizeof(STARTUPINFO);
  si.lpDesktop = L"winsta0\\Default";
  PROCESS_INFORMATION pi = {0};

  LOG(("Starting update process as the service in session 0."));

  // The updater command line is of the form:
  // updater.exe update-dir apply [wait-pid [callback-dir callback-path args]]
  LPWSTR cmdLine = MakeCommandLine(argcTmp, argvTmp);

  // If we're about to start the update process from session 0,
  // then we should not show a GUI.  This only really needs to be done
  // on Vista and higher, but it's better to keep everything consistent
  // across all OS if it's of no harm.
  if (argcTmp >= 2 ) {
    // Setting the desktop to blank will ensure no GUI is displayed
    si.lpDesktop = L"";
    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
  }

  // We move the updater.ini file out of the way because we will handle 
  // executing PostUpdate through the service.  We handle PostUpdate from
  // the service because there are some per user things that happen that
  // can't run in session 0 which we run updater.exe in.
  // Once we are done running updater.exe we rename updater.ini back so
  // that if there were any errors the next updater.exe will run correctly.
  WCHAR updaterINI[MAX_PATH + 1];
  WCHAR updaterINITemp[MAX_PATH + 1];
  BOOL selfHandlePostUpdate = FALSE;
  // We use the updater.ini from the same directory as the updater.exe
  // because of background updates.
  if (PathGetSiblingFilePath(updaterINI, argvTmp[0], L"updater.ini") &&
      PathGetSiblingFilePath(updaterINITemp, argvTmp[0], L"updater.tmp")) {
    selfHandlePostUpdate = MoveFileEx(updaterINI, updaterINITemp, 
                                      MOVEFILE_REPLACE_EXISTING);
  }

  // Create an environment block for the updater.exe process we're about to 
  // start.  Indicate that MOZ_USING_SERVICE is set so the updater.exe can
  // do anything special that it needs to do for service updates.
  // Search in updater.cpp for more info on MOZ_USING_SERVICE.
  WCHAR envVarString[32];
  wsprintf(envVarString, L"MOZ_USING_SERVICE=1"); 
  _wputenv(envVarString);
  LPVOID environmentBlock = NULL;
  if (!CreateEnvironmentBlock(&environmentBlock, NULL, TRUE)) {
    LOG(("Could not create an environment block, setting it to NULL.\n"));
    environmentBlock = NULL;
  }
  // Empty value on _wputenv is how you remove an env variable in Windows
  _wputenv(L"MOZ_USING_SERVICE=");
  processStarted = CreateProcessW(updaterPath, cmdLine, 
                                  NULL, NULL, FALSE, 
                                  CREATE_DEFAULT_ERROR_MODE | 
                                  CREATE_UNICODE_ENVIRONMENT, 
                                  environmentBlock, 
                                  workingDir, &si, &pi);
  if (environmentBlock) {
    DestroyEnvironmentBlock(environmentBlock);
  }
  BOOL updateWasSuccessful = FALSE;
  if (processStarted) {
    // Wait for the updater process to finish
    LOG(("Process was started... waiting on result.\n")); 
    DWORD waitRes = WaitForSingleObject(pi.hProcess, TIME_TO_WAIT_ON_UPDATER);
    if (WAIT_TIMEOUT == waitRes) {
      // We waited a long period of time for updater.exe and it never finished
      // so kill it.
      TerminateProcess(pi.hProcess, 1);
    } else {
      // Check the return code of updater.exe to make sure we get 0
      DWORD returnCode;
      if (GetExitCodeProcess(pi.hProcess, &returnCode)) {
        LOG(("Process finished with return code %d.\n", returnCode)); 
        // updater returns 0 if successful.
        updateWasSuccessful = (returnCode == 0);
      } else {
        LOG(("Process finished but could not obtain return code.\n")); 
      }
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  } else {
    DWORD lastError = GetLastError();
    LOG(("Could not create process as current user, "
         "updaterPath: %ls; cmdLine: %l.  (%d)\n", 
         updaterPath, cmdLine, lastError));
  }

  // Now that we're done with the update, restore back the updater.ini file
  // We use it ourselves, and also we want it back in case we had any type 
  // of error so that the normal update process can use it.
  if (selfHandlePostUpdate) {
    MoveFileEx(updaterINITemp, updaterINI, MOVEFILE_REPLACE_EXISTING);

    // Only run the PostUpdate if the update was successful
    if (updateWasSuccessful && argcTmp > 2) {
      LPCWSTR installationDir = argvTmp[2];
      LPCWSTR updateInfoDir = argvTmp[1];

      // Launch the PostProcess with admin access in session 0.  This is
      // actually launching the post update process but it takes in the 
      // callback app path to figure out where to apply to.
      // The PostUpdate process with user only access will be done inside
      // the unelevated updater.exe after the update process is complete
      // from the service.  We don't know here which session to start
      // the user PostUpdate process from.
      LOG(("Launching post update process as the service in session 0.\n"));
      if (!LaunchWinPostProcess(installationDir, updateInfoDir, true, NULL)) {
        LOG(("The post update process could not be launched.\n"));
      }
    }
  }

  free(cmdLine);
  return updateWasSuccessful;
}

/**
 * Processes a work item (file by the name of .mz) and executes
 * the command within.
 * The only supported command at this time is running an update.
 *
 * @param  monitoringBasePath The base path that is being monitored.
 * @param  notifyInfo         the notifyInfo struct for the changes.
 * @return TRUE if we want the service to stop.
 */
BOOL
ProcessWorkItem(LPCWSTR monitoringBasePath, 
                FILE_NOTIFY_INFORMATION &notifyInfo)
{
  int filenameLength = notifyInfo.FileNameLength / 
                       sizeof(notifyInfo.FileName[0]); 
  notifyInfo.FileName[filenameLength] = L'\0';

  // When the file is ready for processing it will be renamed 
  // to have a .mz extension
  BOOL haveWorkItem = notifyInfo.Action == FILE_ACTION_RENAMED_NEW_NAME && 
                      notifyInfo.FileNameLength > 3 && 
                      notifyInfo.FileName[filenameLength - 3] == L'.' &&
                      notifyInfo.FileName[filenameLength - 2] == L'm' &&
                      notifyInfo.FileName[filenameLength - 1] == L'z';
  if (!haveWorkItem) {
    // We don't have a work item, keep looking
    return FALSE;
  }

  // Indicate that the service is busy and shouldn't be used by anyone else
  // by opening or creating a named event.  Programs should check if this 
  // event exists before writing a work item out.  It should already be
  // created by updater.exe so CreateEventW will lead to an open named event.
  nsAutoHandle serviceRunningEvent(CreateEventW(NULL, TRUE, 
                                                FALSE, SERVICE_EVENT_NAME));

  LOG(("Processing new command meta file: %ls\n", notifyInfo.FileName));
  WCHAR fullMetaUpdateFilePath[MAX_PATH + 1];
  wcscpy(fullMetaUpdateFilePath, monitoringBasePath);

  // We only support file paths in monitoring directories that are MAX_PATH
  // chars or less.
  if (!PathAppendSafe(fullMetaUpdateFilePath, notifyInfo.FileName)) {
    SetEvent(serviceRunningEvent);
    return TRUE;
  }

  nsAutoHandle metaUpdateFile(CreateFile(fullMetaUpdateFilePath, 
                                         GENERIC_READ, 
                                         FILE_SHARE_READ | 
                                         FILE_SHARE_WRITE | 
                                         FILE_SHARE_DELETE, 
                                         NULL, 
                                         OPEN_EXISTING,
                                         0, NULL));
  if (metaUpdateFile == INVALID_HANDLE_VALUE) {
    LOG(("Could not open command meta file: %ls\n", notifyInfo.FileName));
    SetEvent(serviceRunningEvent);
    return TRUE;
  }

  DWORD fileSize = GetFileSize(metaUpdateFile, NULL);
  DWORD commandID = 0;
  // The file should be in wide characters so if it's of odd size it's
  // an invalid file.
  const int kSanityCheckFileSize = 1024 * 64;
  if (fileSize == INVALID_FILE_SIZE || 
      (fileSize %2) != 0 ||
      fileSize > kSanityCheckFileSize ||
      fileSize < sizeof(DWORD)) {
    LOG(("Could not obtain file size or an improper file size was encountered "
         "for command meta file: %ls\n", 
        notifyInfo.FileName));
    SetEvent(serviceRunningEvent);
    return TRUE;
  }

  // The first 4 bytes are for the command ID.
  // Currently only command ID 1 which is for updates is supported.
  DWORD commandIDCount;
  BOOL result = ReadFile(metaUpdateFile, &commandID, 
                         sizeof(DWORD), &commandIDCount, NULL);
  fileSize -= sizeof(DWORD);

  // The next MAX_PATH wchar's are for the app to start
  WCHAR updaterPath[MAX_PATH + 1];
  ZeroMemory(updaterPath, sizeof(updaterPath));
  DWORD updaterPathCount;
  result |= ReadFile(metaUpdateFile, updaterPath, 
                     MAX_PATH * sizeof(WCHAR), &updaterPathCount, NULL);
  fileSize -= updaterPathCount;

  // The next MAX_PATH wchar's are for the app to start
  WCHAR workingDirectory[MAX_PATH + 1];
  ZeroMemory(workingDirectory, sizeof(workingDirectory));
  DWORD workingDirectoryCount;
  result |= ReadFile(metaUpdateFile, workingDirectory, 
                     MAX_PATH * sizeof(WCHAR), &workingDirectoryCount, NULL);
  fileSize -= workingDirectoryCount;

  // + 2 for wide char termination
  nsAutoArrayPtr<char> cmdlineBuffer = new char[fileSize + 2];
  DWORD cmdLineBufferRead;
  result |= ReadFile(metaUpdateFile, cmdlineBuffer, 
                     fileSize, &cmdLineBufferRead, NULL);
  fileSize -= cmdLineBufferRead;

  // We have all the info we need from the work item file, get rid of it.
  metaUpdateFile.reset();
  if (!DeleteFileW(fullMetaUpdateFilePath)) {
    LOG(("Could not delete work item file: %ls. (%d)\n", 
         fullMetaUpdateFilePath, GetLastError()));
  }

  if (!result ||
      commandID != 1 ||
      commandIDCount != sizeof(DWORD) ||
      updaterPathCount != MAX_PATH * sizeof(WCHAR) ||
      workingDirectoryCount != MAX_PATH * sizeof(WCHAR) ||
      fileSize != 0) {
    LOG(("Could not read command data for command meta file: %ls\n", 
         notifyInfo.FileName));
    SetEvent(serviceRunningEvent);
    return TRUE;
  }
  cmdlineBuffer[cmdLineBufferRead] = '\0';
  cmdlineBuffer[cmdLineBufferRead + 1] = '\0';
  WCHAR *cmdlineBufferWide = reinterpret_cast<WCHAR*>(cmdlineBuffer.get());
  LOG(("An update command was detected and is being processed for command meta "
       "file: %ls\n", notifyInfo.FileName));

  int argcTmp = 0;
  LPWSTR* argvTmp = CommandLineToArgvW(cmdlineBufferWide, &argcTmp);

  // Verify that the updater.exe that we are executing is the same
  // as the one in the installation directory which we are updating.
  // The installation dir that we are installing to is argvTmp[2].
  WCHAR installDirUpdater[MAX_PATH + 1];
  wcsncpy(installDirUpdater, argvTmp[2], MAX_PATH);
  if (!PathAppendSafe(installDirUpdater, L"updater.exe")) {
    LOG(("Install directory updater could not be determined.\n"));
    result = FALSE;
  }

  BOOL updaterIsCorrect;
  if (result && !VerifySameFiles(updaterPath, installDirUpdater, 
                                 updaterIsCorrect)) {
    LOG(("Error checking if the updaters are the same.\n"
         "Path 1: %ls\nPath 2: %ls\n", updaterPath, installDirUpdater));
    result = FALSE;
  }

  if (result && !updaterIsCorrect) {
    LOG(("The updaters do not match, udpater will not run.\n")); 
    result = FALSE;
  }

  if (result) {
    LOG(("updater.exe was compared successfully to the installation directory"
         " updater.exe.\n"));
  } else {
    SetEvent(serviceRunningEvent);
    if (argcTmp < 2 || 
        !WriteStatusFailure(argvTmp[1], 
                            SERVICE_UPDATER_COMPARE_ERROR)) {
      LOG(("Could not write update.status updater compare failure.\n"));
    }
    return TRUE;
  }

  // Check to make sure the udpater.exe module has the unique updater identity.
  // This is a security measure to make sure that the signed executable that
  // we will run is actually an updater.
  HMODULE updaterModule = LoadLibrary(updaterPath);
  if (!updaterModule) {
    LOG(("updater.exe module could not be loaded. (%d)\n", GetLastError()));
    result = FALSE;
  } else {
    char updaterIdentity[64];
    if (!LoadStringA(updaterModule, IDS_UPDATER_IDENTITY, 
                     updaterIdentity, sizeof(updaterIdentity))) {
      LOG(("The updater.exe application does not contain the Mozilla"
           " updater identity.\n"));
      result = FALSE;
    }

    if (strcmp(updaterIdentity, UPDATER_IDENTITY_STRING)) {
      LOG(("The updater.exe identity string is not valid.\n"));
      result = FALSE;
    }
    FreeLibrary(updaterModule);
  }

  if (result) {
    LOG(("The updater.exe application contains the Mozilla"
          " updater identity.\n"));
  } else {
    SetEvent(serviceRunningEvent);
    if (argcTmp < 2 || 
        !WriteStatusFailure(argvTmp[1], 
                            SERVICE_UPDATER_IDENTITY_ERROR)) {
      LOG(("Could not write update.status no updater identity.\n"));
    }
    return TRUE;
  }

  // Check for updater.exe sign problems
  BOOL updaterSignProblem = FALSE;
#ifndef DISABLE_UPDATER_AUTHENTICODE_CHECK
  if (argcTmp > 2) {
    updaterSignProblem = !DoesBinaryMatchAllowedCertificates(argvTmp[2], 
                                                             updaterPath);
  }
#endif

  // In order to proceed with the update we need at least 3 command line
  // parameters and no sign problems.
  if (argcTmp > 2 && !updaterSignProblem) {
    BOOL updateProcessWasStarted = FALSE;
    if (StartUpdateProcess(updaterPath, workingDirectory, 
                           argcTmp, argvTmp,
                           updateProcessWasStarted)) {
      LOG(("updater.exe was launched and run successfully!\n"));
      StartServiceUpdate(argcTmp, argvTmp);
    } else {
      LOG(("Error running update process. Updating update.status"
           " Last error: %d\n", GetLastError()));

      // If the update process was started, then updater.exe is responsible for
      // setting the failure code.  If it could not be started then we do the 
      // work.  We set an error instead of directly setting status pending 
      // so that the app.update.service.errors pref can be updated when 
      // the callback app restarts.
      if (!updateProcessWasStarted) {
        if (!WriteStatusFailure(argvTmp[1], 
                                SERVICE_UPDATER_COULD_NOT_BE_STARTED)) {
          LOG(("Could not write update.status service update failure."
               "Last error: %d\n", GetLastError()));
        }
      }
    }
  } else if (argcTmp <= 2) {
    LOG(("Not enough command line parameters specified. "
         "Updating update.status.\n"));

    // We can only update update.status if argvTmp[1] exists.  argvTmp[1] is
    // the directory where the update.status file exists.
    if (argcTmp != 2 || 
        !WriteStatusFailure(argvTmp[1], 
                            SERVICE_NOT_ENOUGH_COMMAND_LINE_ARGS)) {
      LOG(("Could not write update.status service update failure."
           "Last error: %d\n", GetLastError()));
    }
  } else {
    LOG(("Could not start process due to certificate check error on "
         "updater.exe. Updating update.status.  Last error: %d\n", GetLastError()));

    // When there is a certificate check error on the updater.exe application,
    // we want to write out the error.
    if (!WriteStatusFailure(argvTmp[1], 
                            SERVICE_UPDATER_SIGN_ERROR)) {
      LOG(("Could not write pending state to update.status.  (%d)\n", 
           GetLastError()));
    }
  }
  LocalFree(argvTmp);
  SetEvent(serviceRunningEvent);

  // We processed a work item, whether or not it was successful.
  return TRUE;
}

/**
 * Starts monitoring the update directory for work items.
 *
 * @return FALSE if there was an error starting directory monitoring.
 */
BOOL
StartDirectoryChangeMonitor() 
{
  LOG(("Starting directory change monitor...\n"));

  // Init the update directory path and ensure it exists.
  // Example: C:\programData\Mozilla\Firefox\updates\[channel]
  // The channel is not included here as we want to monitor the base directory
  WCHAR updateData[MAX_PATH + 1];
  if (!GetUpdateDirectoryPath(updateData)) {
    LOG(("Could not obtain update directory path\n"));
    return FALSE;
  }

  // Obtain a directory handle, FILE_FLAG_BACKUP_SEMANTICS is needed for this.
  nsAutoHandle directoryHandle(CreateFile(updateData, 
                                          FILE_LIST_DIRECTORY, 
                                          FILE_SHARE_READ | FILE_SHARE_WRITE, 
                                          NULL, 
                                          OPEN_EXISTING,
                                          FILE_FLAG_BACKUP_SEMANTICS, 
                                          NULL));
  if (directoryHandle == INVALID_HANDLE_VALUE) {
    LOG(("Could not obtain directory handle to monitor\n"));
    return FALSE;
  }

  // Allocate a buffer that is big enough to hold 128 changes
  // Note that FILE_NOTIFY_INFORMATION is a variable length struct
  // so that's why we don't create a simple array directly.
  char fileNotifyInfoBuffer[(sizeof(FILE_NOTIFY_INFORMATION) + 
                            MAX_PATH) * 128];
  ZeroMemory(&fileNotifyInfoBuffer, sizeof(fileNotifyInfoBuffer));
  
  DWORD bytesReturned;
  // Listen for directory changes to the update directory
  while (ReadDirectoryChangesW(directoryHandle, 
                               fileNotifyInfoBuffer, 
                               sizeof(fileNotifyInfoBuffer), 
                               TRUE, FILE_NOTIFY_CHANGE_FILE_NAME, 
                               &bytesReturned, NULL, NULL)) {
    char *fileNotifyInfoBufferLocation = fileNotifyInfoBuffer;

    // Make sure we have at least one entry to process
    while (bytesReturned/sizeof(FILE_NOTIFY_INFORMATION) > 0) {
      // Point to the current directory info which is changed
      FILE_NOTIFY_INFORMATION &notifyInfo = 
        *((FILE_NOTIFY_INFORMATION*)fileNotifyInfoBufferLocation);
      fileNotifyInfoBufferLocation += notifyInfo .NextEntryOffset;
      bytesReturned -= notifyInfo .NextEntryOffset;
      if (!wcscmp(notifyInfo.FileName, L"stop") && gServiceStopping) {
        LOG(("A stop command was issued.\n"));
        return TRUE;
      }

      BOOL processedWorkItem = ProcessWorkItem(updateData, notifyInfo);
      if (processedWorkItem) {
        // We don't return here because during stop we will write out a stop 
        // file which will stop monitoring.
        StopService();
        break;
      }

      // NextEntryOffset will be 0 if there are no more items to monitor,
      // and we're ready to make another call to ReadDirectoryChangesW.
      if (0 == notifyInfo.NextEntryOffset) {
        break;
      }
    }
  }

  return TRUE;
}
