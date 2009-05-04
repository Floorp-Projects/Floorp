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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs_compat.h"

typedef int (*FishSoundFunc) (void * data);
typedef int (*FishSoundCmpFunc) (const void * data1, const void * data2);

typedef struct _FishSoundVector FishSoundVector;

struct _FishSoundVector {
  int max_elements;
  int nr_elements;
  FishSoundCmpFunc cmp;
  void ** data;
};

/*
 * A vector of void *. New elements will be appended at the tail.
 */

FishSoundVector *
fs_vector_new (FishSoundCmpFunc cmp)
{
  FishSoundVector * vector;

  vector = fs_malloc (sizeof (FishSoundVector));
  if (vector == NULL) return NULL;

  vector->max_elements = 0;
  vector->nr_elements = 0;
  vector->cmp = cmp;
  vector->data = NULL;

  return vector;
}

static void
fs_vector_clear (FishSoundVector * vector)
{
  fs_free (vector->data);
  vector->data = NULL;
  vector->nr_elements = 0;
  vector->max_elements = 0;
}

void
fs_vector_delete (FishSoundVector * vector)
{
  fs_vector_clear (vector);
  fs_free (vector);
}

int
fs_vector_size (FishSoundVector * vector)
{
  if (vector == NULL) return 0;

  return vector->nr_elements;
}

void *
fs_vector_nth (FishSoundVector * vector, int n)
{
  if (vector == NULL) return NULL;

  if (n >= vector->nr_elements) return NULL;

  return vector->data[n];
}

int
fs_vector_find_index (FishSoundVector * vector, const void * data)
{
  void * v_data;
  int i;

  for (i = 0; i < vector->nr_elements; i++) {
    v_data = vector->data[i];
    if (vector->cmp (v_data, data))
      return i;
  }

  return -1;
}

void *
fs_vector_find (FishSoundVector * vector, const void * data)
{
  void * v_data;
  int i;

  for (i = 0; i < vector->nr_elements; i++) {
    v_data = vector->data[i];
    if (vector->cmp (v_data, data))
      return v_data;
  }

  return NULL;
}

int
fs_vector_foreach (FishSoundVector * vector, FishSoundFunc func)
{
  int i;

  for (i = 0; i < vector->nr_elements; i++) {
    func (vector->data[i]);
  }

  return 0;
}

static FishSoundVector *
fs_vector_grow (FishSoundVector * vector)
{
  void * new_elements;
  int new_max_elements;

  vector->nr_elements++;

  if (vector->nr_elements > vector->max_elements) {
    if (vector->max_elements == 0) {
      new_max_elements = 1;
    } else {
      new_max_elements = vector->max_elements * 2;
    }

    new_elements =
      fs_realloc (vector->data, (size_t)new_max_elements * sizeof (void *));

    if (new_elements == NULL) {
      vector->nr_elements--;
      return NULL;
    }

    vector->max_elements = new_max_elements;
    vector->data = new_elements;
  }

  return vector;
}

void *
fs_vector_insert (FishSoundVector * vector, void * data)
{
  if (vector == NULL) return NULL;

  if (fs_vector_grow (vector) == NULL)
    return NULL;

  vector->data[vector->nr_elements-1] = data;

  return data;

}

static void *
fs_vector_remove_nth (FishSoundVector * vector, int n)
{
  int i;
  void * new_elements;
  int new_max_elements;

  vector->nr_elements--;

  if (vector->nr_elements == 0) {
    fs_vector_clear (vector);
  } else {
    for (i = n; i < vector->nr_elements; i++) {
      vector->data[i] = vector->data[i+1];
    }

    if (vector->nr_elements < vector->max_elements/2) {
      new_max_elements = vector->max_elements/2;

      new_elements =
	fs_realloc (vector->data,
		    (size_t)new_max_elements * sizeof (void *));
      
      if (new_elements == NULL)
	return NULL;

      vector->max_elements = new_max_elements;
      vector->data = new_elements;
    }
  }

  return vector;
}

FishSoundVector *
fs_vector_remove (FishSoundVector * vector, void * data)
{
  int i;

  for (i = 0; i < vector->nr_elements; i++) {
    if (vector->data[i] == data) {
      return fs_vector_remove_nth (vector, i);
    }
  }

  return vector;
}
