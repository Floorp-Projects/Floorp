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

#ifndef MAR_H__
#define MAR_H__

/* We use NSPR here just to import the definition of PRUint32 */
#ifndef WINCE
#include "prtypes.h"
#else
typedef int PRInt32;
typedef unsigned int PRUint32;
typedef wchar_t PRUnichar;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The MAR item data structure.
 */
typedef struct MarItem_ {
  struct MarItem_ *next;  /* private field */
  PRUint32 offset;        /* offset into archive */
  PRUint32 length;        /* length of data in bytes */
  PRUint32 flags;         /* contains file mode bits */
  char name[1];           /* file path */
} MarItem;

typedef struct MarFile_ MarFile;

/**
 * Signature of callback function passed to mar_enum_items.
 * @param mar       The MAR file being visited.
 * @param item      The MAR item being visited.
 * @param data      The data parameter passed by the caller of mar_enum_items.
 * @returns         A non-zero value to stop enumerating.
 */
typedef int (* MarItemCallback)(MarFile *mar, const MarItem *item, void *data);

/**
 * Open a MAR file for reading.
 * @param path      Specifies the path to the MAR file to open.  This path must
 *                  be compatible with fopen.
 * @returns         NULL if an error occurs.
 */
MarFile *mar_open(const char *path);

#ifdef XP_WIN
MarFile *mar_wopen(const PRUnichar *path);
#endif

/**
 * Close a MAR file that was opened using mar_open.
 * @param mar       The MarFile object to close.
 */
void mar_close(MarFile *mar);

/**
 * Find an item in the MAR file by name.
 * @param mar       The MarFile object to query.
 * @param item      The name of the item to query.
 * @returns         A const reference to a MAR item or NULL if not found.
 */
const MarItem *mar_find_item(MarFile *mar, const char *item);

/**
 * Enumerate all MAR items via callback function.
 * @param mar       The MAR file to enumerate.
 * @param callback  The function to call for each MAR item.
 * @param data      A caller specified value that is passed along to the
 *                  callback function.
 * @returns         Zero if the enumeration ran to completion.  Otherwise, any
 *                  non-zero return value from the callback is returned.
 */
int mar_enum_items(MarFile *mar, MarItemCallback callback, void *data);

/**
 * Read from MAR item at given offset up to bufsize bytes.
 * @param mar       The MAR file to read.
 * @param item      The MAR item to read.
 * @param offset    The byte offset relative to the start of the item.
 * @param buf       A pointer to a buffer to copy the data into.
 * @param bufsize   The length of the buffer to copy the data into.
 * @returns         The number of bytes written or a negative value if an
 *                  error occurs.
 */
int mar_read(MarFile *mar, const MarItem *item, int offset, char *buf,
             int bufsize);

/**
 * Create a MAR file from a set of files.
 * @param dest      The path to the file to create.  This path must be
 *                  compatible with fopen.
 * @param numfiles  The number of files to store in the archive.
 * @param files     The list of null-terminated file paths.  Each file
 *                  path must be compatible with fopen.
 * @returns         A non-zero value if an error occurs.
 */
int mar_create(const char *dest, int numfiles, char **files);

/**
 * Extract a MAR file to the current working directory.
 * @param path      The path to the MAR file to extract.  This path must be
 *                  compatible with fopen.
 * @returns         A non-zero value if an error occurs.
 */
int mar_extract(const char *path);

#ifdef __cplusplus
}
#endif

#endif  /* MAR_H__ */
