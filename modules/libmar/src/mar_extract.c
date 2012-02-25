/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is Mozilla Archive code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include "mar_private.h"
#include "mar.h"

#ifdef XP_WIN
#include <io.h>
#include <direct.h>
#endif

/* Ensure that the directory containing this file exists */
static int mar_ensure_parent_dir(const char *path)
{
  char *slash = strrchr(path, '/');
  if (slash)
  {
    *slash = '\0';
    mar_ensure_parent_dir(path);
#ifdef XP_WIN
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
    *slash = '/';
  }
  return 0;
}

static int mar_test_callback(MarFile *mar, const MarItem *item, void *unused) {
  FILE *fp;
  char buf[BLOCKSIZE];
  int fd, len, offset = 0;

  if (mar_ensure_parent_dir(item->name))
    return -1;

#ifdef XP_WIN
  fd = _open(item->name, _O_BINARY|_O_CREAT|_O_TRUNC|_O_WRONLY, item->flags);
#else
  fd = creat(item->name, item->flags);
#endif
  if (fd == -1)
    return -1;

  fp = fdopen(fd, "wb");
  if (!fp)
    return -1;

  while ((len = mar_read(mar, item, offset, buf, sizeof(buf))) > 0) {
    if (fwrite(buf, len, 1, fp) != 1)
      break;
    offset += len;
  }

  fclose(fp);
  return len == 0 ? 0 : -1;
}

int mar_extract(const char *path) {
  MarFile *mar;
  int rv;

  mar = mar_open(path);
  if (!mar)
    return -1;

  rv = mar_enum_items(mar, mar_test_callback, NULL);

  mar_close(mar);
  return rv;
}
