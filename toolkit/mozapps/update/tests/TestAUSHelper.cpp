/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "updatedefines.h"

#ifdef XP_WIN
#  include "commonupdatedir.h"
#  include "updatehelper.h"
#  include "certificatecheck.h"
#  define NS_main wmain
#  define NS_tgetcwd _wgetcwd
#  define NS_ttoi _wtoi
#else
#  define NS_main main
#  define NS_tgetcwd getcwd
#  define NS_ttoi atoi
#endif

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

static void WriteMsg(const NS_tchar* path, const char* status) {
  FILE* outFP = NS_tfopen(path, NS_T("wb"));
  if (!outFP) {
    return;
  }

  fprintf(outFP, "%s\n", status);
  fclose(outFP);
  outFP = nullptr;
}

static bool CheckMsg(const NS_tchar* path, const char* expected) {
  FILE* inFP = NS_tfopen(path, NS_T("rb"));
  if (!inFP) {
    return false;
  }

  struct stat ms;
  if (fstat(fileno(inFP), &ms)) {
    fclose(inFP);
    inFP = nullptr;
    return false;
  }

  char* mbuf = (char*)malloc(ms.st_size + 1);
  if (!mbuf) {
    fclose(inFP);
    inFP = nullptr;
    return false;
  }

  size_t r = ms.st_size;
  char* rb = mbuf;
  size_t c = fread(rb, sizeof(char), 50, inFP);
  r -= c;
  if (c == 0 && r) {
    free(mbuf);
    fclose(inFP);
    inFP = nullptr;
    return false;
  }
  mbuf[ms.st_size] = '\0';
  rb = mbuf;

  bool isMatch = strcmp(rb, expected) == 0;
  free(mbuf);
  fclose(inFP);
  inFP = nullptr;
  return isMatch;
}

int NS_main(int argc, NS_tchar** argv) {
  if (argc == 2) {
    if (!NS_tstrcmp(argv[1], NS_T("post-update-async")) ||
        !NS_tstrcmp(argv[1], NS_T("post-update-sync"))) {
      NS_tchar exePath[MAXPATHLEN];
#ifdef XP_WIN
      if (!::GetModuleFileNameW(0, exePath, MAXPATHLEN)) {
        return 1;
      }
#else
      if (!NS_tvsnprintf(exePath, sizeof(exePath) / sizeof(exePath[0]),
                         NS_T("%s"), argv[0])) {
        return 1;
      }
#endif
      NS_tchar runFilePath[MAXPATHLEN];
      if (!NS_tvsnprintf(runFilePath,
                         sizeof(runFilePath) / sizeof(runFilePath[0]),
                         NS_T("%s.running"), exePath)) {
        return 1;
      }
#ifdef XP_WIN
      if (!NS_taccess(runFilePath, F_OK)) {
        // This makes it possible to check if the post update process was
        // launched twice which happens when the service performs an update.
        NS_tchar runFilePathBak[MAXPATHLEN];
        if (!NS_tvsnprintf(runFilePathBak,
                           sizeof(runFilePathBak) / sizeof(runFilePathBak[0]),
                           NS_T("%s.bak"), runFilePath)) {
          return 1;
        }
        MoveFileExW(runFilePath, runFilePathBak, MOVEFILE_REPLACE_EXISTING);
      }
#endif
      WriteMsg(runFilePath, "running");

      if (!NS_tstrcmp(argv[1], NS_T("post-update-sync"))) {
#ifdef XP_WIN
        Sleep(2000);
#else
        sleep(2);
#endif
      }

      NS_tchar logFilePath[MAXPATHLEN];
      if (!NS_tvsnprintf(logFilePath,
                         sizeof(logFilePath) / sizeof(logFilePath[0]),
                         NS_T("%s.log"), exePath)) {
        return 1;
      }
      WriteMsg(logFilePath, "post-update");
      return 0;
    }
  }

  if (argc < 3) {
    fprintf(
        stderr,
        "\n"
        "Application Update Service Test Helper\n"
        "\n"
        "Usage: WORKINGDIR INFILE OUTFILE -s SECONDS [FILETOLOCK]\n"
        "   or: WORKINGDIR LOGFILE [ARG2 ARG3...]\n"
        "   or: signature-check filepath\n"
        "   or: setup-symlink dir1 dir2 file symlink\n"
        "   or: remove-symlink dir1 dir2 file symlink\n"
        "   or: check-symlink symlink\n"
        "   or: post-update\n"
        "   or: create-update-dir\n"
        "\n"
        "  WORKINGDIR  \tThe relative path to the working directory to use.\n"
        "  INFILE      \tThe relative path from the working directory for the "
        "file to\n"
        "              \tread actions to perform such as finish.\n"
        "  OUTFILE     \tThe relative path from the working directory for the "
        "file to\n"
        "              \twrite status information.\n"
        "  SECONDS     \tThe number of seconds to sleep.\n"
        "  FILETOLOCK  \tThe relative path from the working directory to an "
        "existing\n"
        "              \tfile to open exlusively.\n"
        "              \tOnly available on Windows platforms and silently "
        "ignored on\n"
        "              \tother platforms.\n"
        "  LOGFILE     \tThe relative path from the working directory to log "
        "the\n"
        "              \tcommand line arguments.\n"
        "  ARG2 ARG3...\tArguments to write to the LOGFILE after the preceding "
        "command\n"
        "              \tline arguments.\n"
        "\n"
        "Note: All paths must be relative.\n"
        "\n");
    return 1;
  }

  if (!NS_tstrcmp(argv[1], NS_T("check-signature"))) {
#if defined(XP_WIN) && defined(MOZ_MAINTENANCE_SERVICE)
    if (ERROR_SUCCESS == VerifyCertificateTrustForFile(argv[2])) {
      return 0;
    } else {
      return 1;
    }
#else
    // Not implemented on non-Windows platforms
    return 1;
#endif
  }

  if (!NS_tstrcmp(argv[1], NS_T("setup-symlink"))) {
#ifdef XP_UNIX
    NS_tchar path[MAXPATHLEN];
    if (!NS_tvsnprintf(path, sizeof(path) / sizeof(path[0]), NS_T("%s/%s"),
                       NS_T("/tmp"), argv[2])) {
      return 1;
    }
    if (mkdir(path, 0755)) {
      return 1;
    }
    if (!NS_tvsnprintf(path, sizeof(path) / sizeof(path[0]), NS_T("%s/%s/%s"),
                       NS_T("/tmp"), argv[2], argv[3])) {
      return 1;
    }
    if (mkdir(path, 0755)) {
      return 1;
    }
    if (!NS_tvsnprintf(path, sizeof(path) / sizeof(path[0]),
                       NS_T("%s/%s/%s/%s"), NS_T("/tmp"), argv[2], argv[3],
                       argv[4])) {
      return 1;
    }
    FILE* file = NS_tfopen(path, NS_T("w"));
    if (file) {
      fputs(NS_T("test"), file);
      fclose(file);
    }
    if (symlink(path, argv[5]) != 0) {
      return 1;
    }
    if (!NS_tvsnprintf(path, sizeof(path) / sizeof(path[0]), NS_T("%s/%s"),
                       NS_T("/tmp"), argv[2])) {
      return 1;
    }
    if (argc > 6 && !NS_tstrcmp(argv[6], NS_T("change-perm"))) {
      if (chmod(path, 0644)) {
        return 1;
      }
    }
    return 0;
#else
    // Not implemented on non-Unix platforms
    return 1;
#endif
  }

  if (!NS_tstrcmp(argv[1], NS_T("remove-symlink"))) {
#ifdef XP_UNIX
    // The following can be called at the start of a test in case these symlinks
    // need to be removed if they already exist and at the end of a test to
    // remove the symlinks created by the test so ignore file doesn't exist
    // errors.
    NS_tchar path[MAXPATHLEN];
    if (!NS_tvsnprintf(path, sizeof(path) / sizeof(path[0]), NS_T("%s/%s"),
                       NS_T("/tmp"), argv[2])) {
      return 1;
    }
    if (chmod(path, 0755) && errno != ENOENT) {
      return 1;
    }
    if (!NS_tvsnprintf(path, sizeof(path) / sizeof(path[0]),
                       NS_T("%s/%s/%s/%s"), NS_T("/tmp"), argv[2], argv[3],
                       argv[4])) {
      return 1;
    }
    if (unlink(path) && errno != ENOENT) {
      return 1;
    }
    if (!NS_tvsnprintf(path, sizeof(path) / sizeof(path[0]), NS_T("%s/%s/%s"),
                       NS_T("/tmp"), argv[2], argv[3])) {
      return 1;
    }
    if (rmdir(path) && errno != ENOENT) {
      return 1;
    }
    if (!NS_tvsnprintf(path, sizeof(path) / sizeof(path[0]), NS_T("%s/%s"),
                       NS_T("/tmp"), argv[2])) {
      return 1;
    }
    if (rmdir(path) && errno != ENOENT) {
      return 1;
    }
    return 0;
#else
    // Not implemented on non-Unix platforms
    return 1;
#endif
  }

  if (!NS_tstrcmp(argv[1], NS_T("check-symlink"))) {
#ifdef XP_UNIX
    struct stat ss;
    if (lstat(argv[2], &ss)) {
      return 1;
    }
    return S_ISLNK(ss.st_mode) ? 0 : 1;
#else
    // Not implemented on non-Unix platforms
    return 1;
#endif
  }

  if (!NS_tstrcmp(argv[1], NS_T("wait-for-service-stop"))) {
#ifdef XP_WIN
    const int maxWaitSeconds = NS_ttoi(argv[3]);
    LPCWSTR serviceName = argv[2];
    DWORD serviceState = WaitForServiceStop(serviceName, maxWaitSeconds);
    if (SERVICE_STOPPED == serviceState) {
      return 0;
    } else {
      return serviceState;
    }
#else
    // Not implemented on non-Windows platforms
    return 1;
#endif
  }

  if (!NS_tstrcmp(argv[1], NS_T("wait-for-application-exit"))) {
#ifdef XP_WIN
    const int maxWaitSeconds = NS_ttoi(argv[3]);
    LPCWSTR application = argv[2];
    DWORD ret = WaitForProcessExit(application, maxWaitSeconds);
    if (ERROR_SUCCESS == ret) {
      return 0;
    } else if (WAIT_TIMEOUT == ret) {
      return 1;
    } else {
      return 2;
    }
#else
    // Not implemented on non-Windows platforms
    return 1;
#endif
  }

  if (!NS_tstrcmp(argv[1], NS_T("is-process-running"))) {
#ifdef XP_WIN
    LPCWSTR application = argv[2];
    return (ERROR_NOT_FOUND == IsProcessRunning(application)) ? 0 : 1;
#else
    // Not implemented on non-Windows platforms
    return 1;
#endif
  }

  if (!NS_tstrcmp(argv[1], NS_T("launch-service"))) {
#ifdef XP_WIN
    DWORD ret =
        LaunchServiceSoftwareUpdateCommand(argc - 2, (LPCWSTR*)argv + 2);
    if (ret != ERROR_SUCCESS) {
      // 192 is used to avoid reusing a possible return value from the call to
      // WaitForServiceStop
      return 0x000000C0;
    }
    // Wait a maximum of 120 seconds.
    DWORD lastState = WaitForServiceStop(SVC_NAME, 120);
    if (SERVICE_STOPPED == lastState) {
      return 0;
    }
    return lastState;
#else
    // Not implemented on non-Windows platforms
    return 1;
#endif
  }

  if (!NS_tstrcmp(argv[1], NS_T("create-update-dir"))) {
#ifdef XP_WIN
    mozilla::UniquePtr<wchar_t[]> updateDir;
    HRESULT result = GetCommonUpdateDirectory(
        argv[2], SetPermissionsOf::BaseDirIfNotExists, updateDir);
    return SUCCEEDED(result) ? 0 : 1;
#else
    // Not implemented on non-Windows platforms
    return 1;
#endif
  }

  if (NS_tchdir(argv[1]) != 0) {
    return 1;
  }

  // File in use test helper section
  if (!NS_tstrcmp(argv[4], NS_T("-s"))) {
    // Note: glibc's getcwd() allocates the buffer dynamically using malloc(3)
    // if buf (the 1st param) is NULL so free cwd when it is no longer needed.
    NS_tchar* cwd = NS_tgetcwd(nullptr, 0);
    NS_tchar inFilePath[MAXPATHLEN];
    if (!NS_tvsnprintf(inFilePath, sizeof(inFilePath) / sizeof(inFilePath[0]),
                       NS_T("%s/%s"), cwd, argv[2])) {
      return 1;
    }
    NS_tchar outFilePath[MAXPATHLEN];
    if (!NS_tvsnprintf(outFilePath,
                       sizeof(outFilePath) / sizeof(outFilePath[0]),
                       NS_T("%s/%s"), cwd, argv[3])) {
      return 1;
    }
    free(cwd);

    int seconds = NS_ttoi(argv[5]);
#ifdef XP_WIN
    HANDLE hFile = INVALID_HANDLE_VALUE;
    if (argc == 7) {
      hFile = CreateFileW(argv[6], DELETE | GENERIC_WRITE, 0, nullptr,
                          OPEN_EXISTING, 0, nullptr);
      if (hFile == INVALID_HANDLE_VALUE) {
        WriteMsg(outFilePath, "error_locking");
        return 1;
      }
    }

    WriteMsg(outFilePath, "sleeping");
    int i = 0;
    while (!CheckMsg(inFilePath, "finish\n") && i++ <= seconds) {
      Sleep(1000);
    }

    if (argc == 7) {
      CloseHandle(hFile);
    }
#else
    WriteMsg(outFilePath, "sleeping");
    int i = 0;
    while (!CheckMsg(inFilePath, "finish\n") && i++ <= seconds) {
      sleep(1);
    }
#endif
    WriteMsg(outFilePath, "finished");
    return 0;
  }

  {
    // Command line argument test helper section
    NS_tchar logFilePath[MAXPATHLEN];
    if (!NS_tvsnprintf(logFilePath,
                       sizeof(logFilePath) / sizeof(logFilePath[0]), NS_T("%s"),
                       argv[2])) {
      return 1;
    }

    FILE* logFP = NS_tfopen(logFilePath, NS_T("wb"));
    if (!logFP) {
      return 1;
    }
    for (int i = 1; i < argc; ++i) {
      fprintf(logFP, LOG_S "\n", argv[i]);
    }

    fclose(logFP);
    logFP = nullptr;
  }

  return 0;
}
