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
#include <string.h>
#include "mar_private.h"
#include "mar.h"

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
  PRUint32 offset_to_index = 0, size_of_index, num_signatures;
  PRUint64 size_of_entire_MAR = 0;
  struct stat st;
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

  stack.last_offset = MAR_ID_SIZE + 
                      sizeof(num_signatures) + 
                      sizeof(offset_to_index) +
                      sizeof(size_of_entire_MAR);

  /* We will circle back on this at the end of the MAR creation to fill it */
  if (fwrite(&size_of_entire_MAR, sizeof(size_of_entire_MAR), 1, fp) != 1) {
    goto failure;
  }

  /* Write out the number of signatures, for now only at most 1 is supported */
  num_signatures = 0;
  if (fwrite(&num_signatures, sizeof(num_signatures), 1, fp) != 1) {
    goto failure;
  }

  for (i = 0; i < num_files; ++i) {
    if (stat(files[i], &st)) {
      fprintf(stderr, "ERROR: file not found: %s\n", files[i]);
      goto failure;
    }

    if (mar_push(&stack, st.st_size, st.st_mode & 0777, files[i]))
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

  /* To protect against invalid MAR files, we assumes that the MAR file 
     size is less than or equal to MAX_SIZE_OF_MAR_FILE. */
  if (ftell(fp) > MAX_SIZE_OF_MAR_FILE) {
    goto failure;
  }

  /* write out offset to index file in network byte order */
  offset_to_index = htonl(stack.last_offset);
  if (fseek(fp, MAR_ID_SIZE, SEEK_SET))
    goto failure;
  if (fwrite(&offset_to_index, sizeof(offset_to_index), 1, fp) != 1)
    goto failure;
  offset_to_index = ntohl(stack.last_offset);
  
  size_of_entire_MAR = ((PRUint64)stack.last_offset) +
                       stack.size_used +
                       sizeof(size_of_index);
  size_of_entire_MAR = HOST_TO_NETWORK64(size_of_entire_MAR);
  if (fwrite(&size_of_entire_MAR, sizeof(size_of_entire_MAR), 1, fp) != 1)
    goto failure;
  size_of_entire_MAR = NETWORK_TO_HOST64(size_of_entire_MAR);

  rv = 0;
failure: 
  if (stack.head)
    free(stack.head);
  fclose(fp);
  if (rv)
    remove(dest);
  return rv;
}
