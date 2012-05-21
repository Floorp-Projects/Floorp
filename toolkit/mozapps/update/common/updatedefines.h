/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef UPDATEDEFINES_H
#define UPDATEDEFINES_H

#include "prtypes.h"
#include "readstrings.h"

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

#if defined(XP_WIN)
# include <windows.h>
# include <direct.h>
# include <io.h>

# define F_OK 00
# define W_OK 02
# define R_OK 04
# define S_ISDIR(s) (((s) & _S_IFMT) == _S_IFDIR)
# define S_ISREG(s) (((s) & _S_IFMT) == _S_IFREG)

# define access _access

# define putenv _putenv
# define stat _stat
# define DELETE_DIR L"tobedeleted"
# define CALLBACK_BACKUP_EXT L".moz-callback"

# define LOG_S "%S"
# define NS_T(str) L ## str
// On Windows, _snprintf and _snwprintf don't guarantee null termination. These
// macros always leave room in the buffer for null termination and set the end
// of the buffer to null in case the string is larger than the buffer. Having
// multiple nulls in a string is fine and this approach is simpler (possibly
// faster) than calculating the string length to place the null terminator and
// truncates the string as _snprintf and _snwprintf do on other platforms.
# define snprintf(dest, count, fmt, ...) \
  PR_BEGIN_MACRO \
  int _count = count - 1; \
  _snprintf(dest, _count, fmt, ##__VA_ARGS__); \
  dest[_count] = '\0'; \
  PR_END_MACRO
#define NS_tsnprintf(dest, count, fmt, ...) \
  PR_BEGIN_MACRO \
  int _count = count - 1; \
  _snwprintf(dest, _count, fmt, ##__VA_ARGS__); \
  dest[_count] = L'\0'; \
  PR_END_MACRO
# define NS_taccess _waccess
# define NS_tchdir _wchdir
# define NS_tchmod _wchmod
# define NS_tfopen _wfopen
# define NS_tmkdir(path, perms) _wmkdir(path)
# define NS_tremove _wremove
// _wrename is used to avoid the link tracking service.
# define NS_trename _wrename
# define NS_trmdir _wrmdir
# define NS_tstat _wstat
# define NS_tstrcat wcscat
# define NS_tstrcmp wcscmp
# define NS_tstrcpy wcscpy
# define NS_tstrlen wcslen
# define NS_tstrrchr wcsrchr
# define NS_tstrstr wcsstr
#else
# include <sys/wait.h>
# include <unistd.h>

#ifdef SOLARIS
# include <sys/stat.h>
# include <dirent.h>
#else
# include <fts.h>
#endif

#ifdef XP_MACOSX
# include <sys/time.h>
#endif

# define LOG_S "%s"
# define NS_T(str) str
# define NS_tsnprintf snprintf
# define NS_taccess access
# define NS_tchdir chdir
# define NS_tchmod chmod
# define NS_tfopen fopen
# define NS_tmkdir mkdir
# define NS_tremove remove
# define NS_trename rename
# define NS_trmdir rmdir
# define NS_tstat stat
# define NS_tstrcat strcat
# define NS_tstrcmp strcmp
# define NS_tstrcpy strcpy
# define NS_tstrlen strlen
# define NS_tstrrchr strrchr
# define NS_tstrstr strstr
#endif

#define BACKUP_EXT NS_T(".moz-backup")

#endif
