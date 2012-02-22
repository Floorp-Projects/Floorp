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
 * The Original Code is common code between maintenanceservice and updater
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
