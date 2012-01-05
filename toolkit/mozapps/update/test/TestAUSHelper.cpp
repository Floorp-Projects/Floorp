/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#ifdef XP_WIN
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")
# include <windows.h>
# include <wintrust.h>
# include <softpub.h>
# include <direct.h>
# include <io.h>
  typedef WCHAR NS_tchar;
# define NS_main wmain
# define F_OK 00
# define W_OK 02
# define R_OK 04
# define stat _stat
# define NS_T(str) L ## str
# define NS_tsnprintf(dest, count, fmt, ...) \
  { \
    int _count = count - 1; \
    _snwprintf(dest, _count, fmt, ##__VA_ARGS__); \
    dest[_count] = L'\0'; \
  }
# define NS_taccess _waccess
# define NS_tchdir _wchdir
# define NS_tfopen _wfopen
# define NS_tstrcmp wcscmp
# define NS_ttoi _wtoi
# define NS_tstat _wstat
# define LOG_S "%S"
#else
# include <unistd.h>
# define NS_main main
  typedef char NS_tchar;
# define NS_T(str) str
# define NS_tsnprintf snprintf
# define NS_taccess access
# define NS_tchdir chdir
# define NS_tfopen fopen
# define NS_tstrcmp strcmp
# define NS_ttoi atoi
# define NS_tstat stat
# define LOG_S "%s"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef MAXPATHLEN
# ifdef PATH_MAX
#  define MAXPATHLEN PATH_MAX
# elif defined(MAX_PATH)
#  define MAXPATHLEN MAX_PATH
# elif defined(_MAX_PATH)
#  define MAXPATHLEN _MAX_PATH
# elif defined(CCHMAXPATH)
#  define MAXPATHLEN CCHMAXPATH
# else
#  define MAXPATHLEN 1024
# endif
#endif

static void
WriteMsg(const NS_tchar *path, const char *status)
{
  FILE* outFP = NS_tfopen(path, NS_T("wb"));
  if (!outFP)
    return;

  fprintf(outFP, "%s\n", status);
  fclose(outFP);
  outFP = NULL;
}

static bool
CheckMsg(const NS_tchar *path, const char *expected)
{
  if (NS_taccess(path, F_OK)) {
    return false;
  }

  FILE *inFP = NS_tfopen(path, NS_T("rb"));
  if (!inFP) {
    return false;
  }

  struct stat ms;
  if (fstat(fileno(inFP), &ms)) {
    return false;
  }

  char *mbuf = (char *) malloc(ms.st_size + 1);
  if (!mbuf) {
    return false;
  }

  size_t r = ms.st_size;
  char *rb = mbuf;
  size_t c = fread(rb, sizeof(char), 50, inFP);
  r -= c;
  rb += c;
  if (c == 0 && r) {
    return false;
  }
  mbuf[ms.st_size] = '\0';
  rb = mbuf;

  fclose(inFP);
  inFP = NULL;
  return strcmp(rb, expected) == 0;
}

#ifdef XP_WIN
/**
 * Verifies the trust of the specified file path.
 *
 * @param  filePath  The file path to check.
 * @return ERROR_SUCCESS if successful, or the last error code otherwise.
 */
DWORD
VerifyCertificateTrustForFile(LPCWSTR filePath)
{
  // Setup the file to check.
  WINTRUST_FILE_INFO fileToCheck;
  ZeroMemory(&fileToCheck, sizeof(fileToCheck));
  fileToCheck.cbStruct = sizeof(WINTRUST_FILE_INFO);
  fileToCheck.pcwszFilePath = filePath;

  // Setup what to check, we want to check it is signed and trusted.
  WINTRUST_DATA trustData;
  ZeroMemory(&trustData, sizeof(trustData));
  trustData.cbStruct = sizeof(trustData);
  trustData.pPolicyCallbackData = NULL;
  trustData.pSIPClientData = NULL;
  trustData.dwUIChoice = WTD_UI_NONE;
  trustData.fdwRevocationChecks = WTD_REVOKE_NONE; 
  trustData.dwUnionChoice = WTD_CHOICE_FILE;
  trustData.dwStateAction = 0;
  trustData.hWVTStateData = NULL;
  trustData.pwszURLReference = NULL;
  // no UI
  trustData.dwUIContext = 0;
  trustData.pFile = &fileToCheck;

  GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
  // Check if the file is signed by something that is trusted.
  return WinVerifyTrust(NULL, &policyGUID, &trustData);
}

/**
 * Waits for a service to enter a stopped state.
 * This function does not stop the service, it just blocks until the service
 * is stopped.
 *
 * @param  serviceName    The service to wait for.
 * @param  maxWaitSeconds The maximum number of seconds to wait
 * @return true if the service was stopped after waiting at most maxWaitSeconds
 *         false on an error or when the service was not stopped
 */
bool WaitForServiceStop(LPCWSTR serviceName, DWORD maxWaitSeconds) 
{
  // Get a handle to the SCM database.
  SC_HANDLE serviceManager = OpenSCManager(NULL, NULL, 
                                           SC_MANAGER_CONNECT | 
                                           SC_MANAGER_ENUMERATE_SERVICE);
  if (!serviceManager)  {
    return false;
  }

  // Get a handle to the service.
  SC_HANDLE service = OpenServiceW(serviceManager, 
                                   serviceName, 
                                   SERVICE_QUERY_STATUS);
  if (!service) {
    CloseServiceHandle(serviceManager);
    return false;
  }

  bool gotStop = false;
  DWORD currentWaitMS = 0;
  while (currentWaitMS < maxWaitSeconds * 1000) {
    // Make sure the service is not stopped.
    SERVICE_STATUS_PROCESS ssp;
    DWORD bytesNeeded;
    if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp,
                              sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded)) {
      break;
    }

    // The service is already in use.
    if (ssp.dwCurrentState == SERVICE_STOPPED) {
      gotStop = true;
      break;
    }
    currentWaitMS += 50;
    Sleep(50);
  }

  CloseServiceHandle(service);
  CloseServiceHandle(serviceManager);
  return gotStop;
}


#endif

int NS_main(int argc, NS_tchar **argv)
{

  if (argc < 3) {
    fprintf(stderr, \
            "\n" \
            "Application Update Service Test Helper\n" \
            "\n" \
            "Usage: WORKINGDIR INFILE OUTFILE -s SECONDS [FILETOLOCK]\n"  \
            "   or: WORKINGDIR LOGFILE [ARG2 ARG3...]\n" \
            "   or: signature-check filepath\n" \
            "\n" \
            "  WORKINGDIR  \tThe relative path to the working directory to use.\n" \
            "  INFILE      \tThe relative path from the working directory for the file to\n" \
            "              \tread actions to perform such as finish.\n" \
            "  OUTFILE     \tThe relative path from the working directory for the file to\n" \
            "              \twrite status information.\n" \
            "  SECONDS     \tThe number of seconds to sleep.\n" \
            "  FILETOLOCK  \tThe relative path from the working directory to an existing\n" \
            "              \tfile to open exlusively.\n" \
            "              \tOnly available on Windows platforms and silently ignored on\n" \
            "              \tother platforms.\n" \
            "  LOGFILE     \tThe relative path from the working directory to log the\n" \
            "              \tcommand line arguments.\n" \
            "  ARG2 ARG3...\tArguments to write to the LOGFILE after the preceding command\n" \
            "              \tline arguments.\n" \
            "\n" \
            "Note: All paths must be relative.\n" \
            "\n");
    return 1;
  }

  if (!NS_tstrcmp(argv[1], NS_T("check-signature"))) {
#ifdef XP_WIN
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

  if (!NS_tstrcmp(argv[1], NS_T("wait-for-service-stop"))) {
#ifdef XP_WIN
    const int maxWaitSeconds = NS_ttoi(argv[3]);
    LPCWSTR serviceName = argv[2];
    if (WaitForServiceStop(serviceName, maxWaitSeconds)) {
      return 0;
    } else {
      return 1;
    }
#else 
    // Not implemented on non-Windows platforms
    return 1;
#endif
  }

  int i = 0;

  if (NS_tchdir(argv[1]) != 0) {
    return 1;
  }

  // File in use test helper section
  if (!NS_tstrcmp(argv[4], NS_T("-s"))) {
    NS_tchar inFilePath[MAXPATHLEN];
    NS_tsnprintf(inFilePath, sizeof(inFilePath)/sizeof(inFilePath[0]),
                 NS_T("%s"), argv[2]);
    NS_tchar outFilePath[MAXPATHLEN];
    NS_tsnprintf(outFilePath, sizeof(outFilePath)/sizeof(outFilePath[0]),
                 NS_T("%s"), argv[3]);

    int seconds = NS_ttoi(argv[5]);
#ifdef XP_WIN
    HANDLE hFile = INVALID_HANDLE_VALUE;
    if (argc == 7) {
      hFile = CreateFileW(argv[6],
                          DELETE | GENERIC_WRITE, 0,
                          NULL, OPEN_EXISTING, 0, NULL);
      if (hFile == INVALID_HANDLE_VALUE) {
        WriteMsg(outFilePath, "error_locking");
        return 1;
      }
    }

    WriteMsg(outFilePath, "sleeping");
    while (!CheckMsg(inFilePath, "finish\n") && i++ <= seconds)  {
      Sleep(1000);
    }

    if (argc == 7) {
      CloseHandle(hFile);
    }
#else
    WriteMsg(outFilePath, "sleeping");
    while (!CheckMsg(inFilePath, "finish\n") && i++ <= seconds)  {
      sleep(1);
    }
#endif
    WriteMsg(outFilePath, "finished");
    return 0;
  }

  // Command line argument test helper section
  NS_tchar logFilePath[MAXPATHLEN];
  NS_tsnprintf(logFilePath, sizeof(logFilePath)/sizeof(logFilePath[0]),
               NS_T("%s"), argv[2]);

  FILE* logFP = NS_tfopen(logFilePath, NS_T("wb"));
  for (i = 1; i < argc; ++i) {
    fprintf(logFP, LOG_S "\n", argv[i]);
  }

  fclose(logFP);
  logFP = NULL;

  return 0;
} 
