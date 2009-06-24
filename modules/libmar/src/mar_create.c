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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mar.h"
#include "mar_private.h"

#ifdef XP_WIN
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <unistd.h>
#endif

struct MarItemStack {
  void *head;
  PRUint32 size_used;
  PRUint32 size_allocated;
  PRUint32 last_offset;
};

/**
 * Push a new item onto the stack of items.  The stack is a single block
 * of memory.
 */
static int mar_push(struct MarItemStack *stack, PRUint32 length, PRUint32 flags,
                    const char *name) {
  int namelen;
  PRUint32 n_offset, n_length, n_flags;
  PRUint32 size;
  char *data;
  
  namelen = strlen(name);
  size = MAR_ITEM_SIZE(namelen);

  if (stack->size_allocated - stack->size_used < size) {
    /* increase size of stack */
    size_t size_needed = ROUND_UP(stack->size_used + size, BLOCKSIZE);
    stack->head = realloc(stack->head, size_needed);
    if (!stack->head)
      return -1;
    stack->size_allocated = size_needed;
  }

  data = (((char *) stack->head) + stack->size_used);

  n_offset = htonl(stack->last_offset);
  n_length = htonl(length);
  n_flags = htonl(flags);

  memcpy(data, &n_offset, sizeof(n_offset));
  data += sizeof(n_offset);

  memcpy(data, &n_length, sizeof(n_length));
  data += sizeof(n_length);

  memcpy(data, &n_flags, sizeof(n_flags));
  data += sizeof(n_flags);

  memcpy(data, name, namelen + 1);
  
  stack->size_used += size;
  stack->last_offset += length;
  return 0;
}

static int mar_concat_file(FILE *fp, const char *path) {
  FILE *in;
  char buf[BLOCKSIZE];
  size_t len;
  int rv = 0;

  in = fopen(path, "rb");
  if (!in)
    return -1;

  while ((len = fread(buf, 1, BLOCKSIZE, in)) > 0) {
    if (fwrite(buf, len, 1, fp) != 1) {
      rv = -1;
      break;
    }
  }

  fclose(in);
  return rv;
}

int mar_create(const char *dest, int num_files, char **files) {
  struct MarItemStack stack;
  PRUint32 offset_to_index = 0, size_of_index;
#ifdef WINCE
  WIN32_FIND_DATAW file_data;
  PRUnichar wide_path[MAX_PATH];
  size_t size;
  PRInt32 flags;
#else
  struct stat st;
#endif
  FILE *fp;
  int i, rv = -1;

  memset(&stack, 0, sizeof(stack));

  fp = fopen(dest, "wb");
  if (!fp) {
    fprintf(stderr, "ERROR: could not create target file: %s\n", dest);
    return -1;
  }

  if (fwrite(MAR_ID, MAR_ID_SIZE, 1, fp) != 1)
    goto failure;
  if (fwrite(&offset_to_index, sizeof(PRUint32), 1, fp) != 1)
    goto failure;

  stack.last_offset = MAR_ID_SIZE + sizeof(PRUint32);

  for (i = 0; i < num_files; ++i) {
#ifdef WINCE
    HANDLE handle;
    MultiByteToWideChar(CP_ACP, 0, files[i], -1, wide_path, MAX_PATH);
    handle = FindFirstFile(wide_path, &file_data);
    if (handle == INVALID_HANDLE_VALUE) {
#else
    if (stat(files[i], &st)) {
#endif
      fprintf(stderr, "ERROR: file not found: %s\n", files[i]);
      goto failure;
    }
#ifdef WINCE
    flags = (file_data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ?
      0444 : 0666;
    size = (file_data.nFileSizeHigh * (MAXDWORD + 1)) + file_data.nFileSizeLow;
    FindClose(handle);
    if (mar_push(&stack, size, flags, files[i]))
#else

    if (mar_push(&stack, st.st_size, st.st_mode & 0777, files[i]))
#endif
      goto failure;

    /* concatenate input file to archive */
    if (mar_concat_file(fp, files[i]))
      goto failure;
  }

  /* write out the index (prefixed with length of index) */
  size_of_index = htonl(stack.size_used);
  if (fwrite(&size_of_index, sizeof(size_of_index), 1, fp) != 1)
    goto failure;
  if (fwrite(stack.head, stack.size_used, 1, fp) != 1)
    goto failure;

  /* write out offset to index file in network byte order */
  offset_to_index = htonl(stack.last_offset);
  if (fseek(fp, MAR_ID_SIZE, SEEK_SET))
    goto failure;
  if (fwrite(&offset_to_index, sizeof(offset_to_index), 1, fp) != 1)
    goto failure;

  rv = 0;
failure: 
  if (stack.head)
    free(stack.head);
  fclose(fp);
  if (rv)
#ifdef WINCE
    {
      MultiByteToWideChar(CP_ACP, 0, dest, -1, wide_path, MAX_PATH);
      DeleteFileW(wide_path);
    }
#else
    remove(dest);
#endif
  return rv;
}
