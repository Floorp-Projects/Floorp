/*
   Copyright (C) 2003 Commonwealth Scientific and Industrial Research
   Organisation (CSIRO) Australia

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of CSIRO Australia nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ORGANISATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"

#include <stdlib.h>
#include "oggz_macros.h"
#include "oggz_vector.h"

typedef struct _OggzTable OggzTable;

struct _OggzTable {
  OggzVector * keys;
  OggzVector * data;
};

OggzTable *
oggz_table_new (void)
{
  OggzTable * table;

  table = oggz_malloc (sizeof (OggzTable));
  if (table == NULL) return NULL;

  table->keys = oggz_vector_new ();
  table->data = oggz_vector_new ();

  return table;
}

void
oggz_table_delete (OggzTable * table)
{
  if (table == NULL) return;

  oggz_vector_delete (table->keys);
  oggz_vector_delete (table->data);
  oggz_free (table);
}

void *
oggz_table_lookup (OggzTable * table, long key)
{
  int i, size;

  if (table == NULL) return NULL;

  size = oggz_vector_size (table->keys);
  for (i = 0; i < size; i++) {
    if (oggz_vector_nth_l (table->keys, i) == key) {
      return oggz_vector_nth_p (table->data, i);
    }
  }

  return NULL;
}

void *
oggz_table_insert (OggzTable * table, long key, void * data)
{
  void * old_data;

  if ((old_data = oggz_table_lookup (table, key)) != NULL) {
    if (oggz_vector_remove_l (table->keys, key) == NULL)
      return NULL;

    if (oggz_vector_remove_p (table->data, old_data) == NULL) {
      /* XXX: This error condition can only happen if the previous
       * removal succeeded, and this removal failed, ie. there was
       * an error reallocing table->data->data downwards. */
      return NULL;
    }
  }

  if (oggz_vector_insert_l (table->keys, key) == -1)
    return NULL;
  
  if (oggz_vector_insert_p (table->data, data) == NULL) {
    oggz_vector_remove_l (table->keys, key);
    return NULL;
  }

  return data;
}

int
oggz_table_remove (OggzTable * table, long key)
{
  void * old_data;

  if ((old_data = oggz_table_lookup (table, key)) != NULL) {
    if (oggz_vector_remove_l (table->keys, key) == NULL)
      return -1;

    if (oggz_vector_remove_p (table->data, old_data) == NULL) {
      /* XXX: This error condition can only happen if the previous
       * removal succeeded, and this removal failed, ie. there was
       * an error reallocing table->data->data downwards. */
      return -1;
    }
  }
  
  return 0;
}

int
oggz_table_size (OggzTable * table)
{
  if (table == NULL) return 0;
  return oggz_vector_size (table->data);
}

void *
oggz_table_nth (OggzTable * table, int n, long * key)
{
  if (table == NULL) return NULL;
  if (key) *key = oggz_vector_nth_l (table->keys, n);
  return oggz_vector_nth_p (table->data, n);
}
