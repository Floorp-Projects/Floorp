/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#ifdef XP_WIN
# include <windows.h>
# include <direct.h>
# include <io.h>
  typedef WCHAR NS_tchar;
# define NS_main wmain
# define NS_T(str) L ## str
# define NS_tsnprintf(dest, count, fmt, ...) \
    int _count = count - 1; \
    _snwprintf(dest, _count, fmt, ##__VA_ARGS__); \
    dest[_count] = L'\0';
# define NS_tfopen _wfopen
# define NS_tstrcmp wcscmp
# define NS_ttoi _wtoi
# define LOG_S "%S"
#else
# include <unistd.h>
# define NS_main main
  typedef char NS_tchar;
# define NS_T(str) str
# define NS_tsnprintf snprintf
# define NS_tfopen fopen
# define NS_tstrcmp strcmp
# define NS_ttoi atoi
# define LOG_S "%s"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

int NS_main(int argc, NS_tchar **argv)
{

  if (argc < 2) {
    fprintf(stderr, \
            "\n" \
            "Application Update Service Test Helper\n" \
            "\n" \
            "Usage: -s SECONDS [LOCKTYPE FILETOLOCK]\n"  \
            "   or: LOGFILE [ARG2 ARG3...]\n" \
            "\n" \
            "  SECONDS     \tThe number of seconds to sleep.\n" \
            "  FILETOLOCK  \tThe relative path to an existing file to open exlusively.\n" \
            "              \tOnly available on Windows platforms and silently ignored on" \
            "              \tother platforms.\n" \
            "  LOGFILE     \tThe file path relative to the working directory to log the\n" \
            "              \targuments passed to.\n" \
            "  ARG2 ARG3...\tArguments to write to the log file.\n" \
            "\n" \
            "All paths should be relative since paths in a build environment can have a\n" \
            "path length that is larger than the maximum path length on Windows.\n" \
            "\n");
    return 1;
  }

  // File in use test helper section
  if (NS_tstrcmp(argv[1], NS_T("-s")) == 0) {
#ifdef XP_WIN
    int milliseconds = NS_ttoi(argv[2]) * 1000;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    if (argc == 4) {
      hFile = CreateFileW(argv[3],
#ifdef WINCE
                          GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
#else
                          DELETE | GENERIC_WRITE, 0,
#endif
                          NULL, OPEN_EXISTING, 0, NULL);
      if (hFile == INVALID_HANDLE_VALUE) {
        return 1;
      }
    }

    Sleep(milliseconds);

    if (argc == 5) {
      CloseHandle(hFile);
    }
#else
    int seconds = NS_ttoi(argv[2]);
    sleep(seconds);
#endif
    return 0;
  }

  // Command line argument test helper section
  NS_tchar logFilePath[MAXPATHLEN];

  NS_tsnprintf(logFilePath, sizeof(logFilePath)/sizeof(logFilePath[0]),
               NS_T("%s"), argv[1]);

  FILE* logFP = NS_tfopen(logFilePath, NS_T("w"));

  fprintf(logFP, "executed\n");

  int i;
  for (i = 1; i < argc; ++i) {
    fprintf(logFP, LOG_S "\n", argv[i]);
  }

  fclose(logFP);
  logFP = NULL;

  return 0;
} 
