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
 * The Original Code is Mozilla Application Update..
 *
 * The Initial Developer of the Original Code is
 * Brad Lassey <blassey@mozilla.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include <windows.h>
#include "updater_wince.h"
#include "environment.cpp"

# define F_OK 00
# define W_OK 02
# define R_OK 04

int errno = 0;

int chmod(const char* path, unsigned int mode) 
{
  return 0;
}

int _wchmod(const WCHAR* path, unsigned int mode) 
{
  return 0;
}

int fstat(FILE* handle, struct stat* buff)
{
  int position = ftell(handle);
  if (position < 0)
    return -1;

  if (fseek(handle, 0, SEEK_END) < 0)
    return -1;

  buff->st_size = ftell(handle);

  if (fseek(handle, position, SEEK_SET) < 0)
    return -1;

  if (buff->st_size < 0)
    return -1;

  buff->st_mode = _S_IFREG | _S_IREAD | _S_IWRITE | _S_IEXEC;
  /* can't get time from a file handle on wince */
  buff->st_ctime = 0;
  buff->st_atime = 0;
  buff->st_mtime = 0;
  return 0;
}

int stat(const char* path, struct stat* buf) 
{
  FILE* f = fopen(path, "r");
  int rv = fstat(f, buf);
  fclose(f);
  return rv;
}

int _wstat(const WCHAR* path, struct stat* buf) 
{
  FILE* f = _wfopen(path, L"r");
  int rv = fstat(f, buf);
  fclose(f);
  return rv;
}

int _wmkdir(const WCHAR* path) 
{
  DWORD dwAttr = GetFileAttributesW(path);
  if (dwAttr != INVALID_FILE_ATTRIBUTES)
    return (dwAttr & FILE_ATTRIBUTE_DIRECTORY) ? 0 : -1;
  return CreateDirectoryW(path, NULL) ? 0 : -1;
}

FILE* fileno(FILE* f) 
{
  return f;
}

int _access(const char* path, int amode) 
{
  WCHAR wpath[MAX_PATH];
  MultiByteToWideChar(CP_ACP,
		      0,
		      path,
		      -1,
		      wpath,
		      MAX_PATH );
  return _waccess(wpath, amode);
}

int _waccess(const WCHAR* path, int amode)
{
  if (amode == F_OK || amode == R_OK)
    return (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) ? -1 : 0;
  return -1;
}

int _wremove(const WCHAR* wpath) 
{
  if (DeleteFileW(wpath)) 
  {
    return 0;
  } 
  else if (GetLastError() == ERROR_ACCESS_DENIED) {
    return RemoveDirectoryW(wpath) ? 0:-1;
  } 
  else {
    return -1;
  }
}
