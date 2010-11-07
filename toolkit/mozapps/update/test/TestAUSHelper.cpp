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
# define LOG_S "%S"
#else
# include <unistd.h>
# define NS_main main
  typedef char NS_tchar;
# define NS_T(str) str
# define NS_tsnprintf snprintf
# define NS_tfopen fopen
# define LOG_S "%s"
#endif

#include <stdio.h>

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
  NS_tchar logFilePath[MAXPATHLEN];

#ifdef XP_WIN
  NS_tchar exePath[MAXPATHLEN];

  ::GetModuleFileNameW(0, exePath, MAXPATHLEN);
  NS_tsnprintf(logFilePath, sizeof(logFilePath)/sizeof(logFilePath[0]),
               NS_T("%s.log"), exePath);
#else
  NS_tsnprintf(logFilePath, sizeof(logFilePath)/sizeof(logFilePath[0]),
               NS_T("%s.log"), argv[0]);
#endif

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
