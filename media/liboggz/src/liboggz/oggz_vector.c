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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oggz_macros.h"

typedef int (*OggzFunc) (void * data);
typedef int (*OggzFunc1) (void * data, void * arg);
typedef int (*OggzFindFunc) (void * data, long serialno);
typedef int (*OggzCmpFunc) (const void * a, const void * b, void * user_data);

typedef struct _OggzVector OggzVector;

typedef union {
  void * p;
  long l;
} oggz_data_t;

struct _OggzVector {
  int max_elements;
  int nr_elements;
  oggz_data_t * data;
  OggzCmpFunc compare;
  void * compare_user_data;
};

/*
 * A vector of void * or long; iff it's a vector of void * objects, it
 * can be optionally sorted. (The sorting is used to implement the
 * packet queue; the vector of longs is used to implement OggzTable)
 *
 * if you set a comparison function (oggz_vector_set_cmp()), the vector
 * will be sorted and new elements will be inserted in sorted order.
 *
 * if you don't set a comparison function, new elements will be appended
 * at the tail
 *
 * to unset the comparison function, call oggz_vector_set_cmp (NULL,NULL)
 */

OggzVector *
oggz_vector_new (void)
{
  OggzVector * vector;

  vector = oggz_malloc (sizeof (OggzVector));
  if (vector == NULL) return NULL;

  vector->max_elements = 0;
  vector->nr_elements = 0;
  vector->data = NULL;
  vector->compare = NULL;
  vector->compare_user_data = NULL;

  return vector;
}

static void
oggz_vector_clear (OggzVector * vector)
{
  if (vector->data)
  {
    oggz_free (vector->data);
    vector->data = NULL;
  }

  vector->nr_elements = 0;
  vector->max_elements = 0;
}

void
oggz_vector_delete (OggzVector * vector)
{
  oggz_vector_clear (vector);
  oggz_free (vector);
}

int
oggz_vector_size (OggzVector * vector)
{
  if (vector == NULL) return 0;

  return vector->nr_elements;
}

void *
oggz_vector_nth_p (OggzVector * vector, int n)
{
  if (vector == NULL) return NULL;

  if (n >= vector->nr_elements) return NULL;

  return vector->data[n].p;
}

long
oggz_vector_nth_l (OggzVector * vector, int n)
{
  if (vector == NULL) return -1L;

  if (n >= vector->nr_elements) return -1L;

  return vector->data[n].l;
}

void *
oggz_vector_find_p (OggzVector * vector, const void * data)
{
  void * d;
  int i;

  if (vector->compare == NULL) return NULL;

  for (i = 0; i < vector->nr_elements; i++) {
    d = vector->data[i].p;
    if (vector->compare (d, data, vector->compare_user_data))
      return d;
  }

  return NULL;
}

int
oggz_vector_find_index_p (OggzVector * vector, const void * data)
{
  void * d;
  int i;

  if (vector->compare == NULL) return -1;

  for (i = 0; i < vector->nr_elements; i++) {
    d = vector->data[i].p;
    if (vector->compare (d, data, vector->compare_user_data))
      return i;
  }

  return -1;
}

void *
oggz_vector_find_with (OggzVector * vector, OggzFindFunc func, long serialno)
{
  void * d;
  int i;

  for (i = 0; i < vector->nr_elements; i++) {
    d = vector->data[i].p;
    if (func (d, serialno))
      return d;
  }

  return NULL;
}

int
oggz_vector_foreach (OggzVector * vector, OggzFunc func)
{
  int i;

  for (i = 0; i < vector->nr_elements; i++) {
    func (vector->data[i].p);
  }

  return 0;
}

int
oggz_vector_foreach1 (OggzVector * vector, OggzFunc1 func, void *arg)
{
  int i;

  for (i = 0; i < vector->nr_elements; i++) {
    func (vector->data[i].p, arg);
  }

  return 0;
}

static void
_array_swap (oggz_data_t v[], int i, int j)
{
  void * t;

  t = v[i].p;
  v[i].p = v[j].p;
  v[j].p = t;
}

/**
 * Helper function for oggz_vector_insert (). Sorts the vector by
 * insertion sort, assuming the tail element has just been added and the
 * rest of the vector is sorted.
 * \param vector An OggzVector
 * \pre The vector has just had a new element added to its tail
 * \pre All elements other than the tail element are already sorted.
 */
static void
oggz_vector_tail_insertion_sort (OggzVector * vector)
{
  int i;

  if (vector->compare == NULL) return;

  for (i = vector->nr_elements-1; i > 0; i--) {
    if (vector->compare (vector->data[i-1].p, vector->data[i].p,
			 vector->compare_user_data) > 0) {
      _array_swap (vector->data, i, i-1);
    } else {
      break;
    }
  }

  return;
}

static OggzVector *
oggz_vector_grow (OggzVector * vector)
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
      oggz_realloc (vector->data, (size_t)new_max_elements * sizeof (oggz_data_t));

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
oggz_vector_insert_p (OggzVector * vector, void * data)
{
  if (oggz_vector_grow (vector) == NULL)
    return NULL;

  vector->data[vector->nr_elements-1].p = data;

  oggz_vector_tail_insertion_sort (vector);

  return data;

}

long
oggz_vector_insert_l (OggzVector * vector, long ldata)
{
  if (oggz_vector_grow (vector) == NULL)
    return -1;

  vector->data[vector->nr_elements-1].l = ldata;

  return ldata;
}

static void
oggz_vector_qsort (OggzVector * vector, int left, int right)
{
  int i, last;
  oggz_data_t * v = vector->data;

  if (left >= right) return;

  _array_swap (v, left, (left + right)/2);
  last = left;
  for (i = left+1; i <= right; i++) {
    if (vector->compare (v[i].p, v[left].p, vector->compare_user_data) < 0)
      _array_swap (v, ++last, i);
  }
  _array_swap (v, left, last);
  oggz_vector_qsort (vector, left, last-1);
  oggz_vector_qsort (vector, last+1, right);
}

int
oggz_vector_set_cmp (OggzVector * vector, OggzCmpFunc compare,
		     void * user_data)
{
  vector->compare = compare;
  vector->compare_user_data = user_data;

  if (compare) {
    oggz_vector_qsort (vector, 0, vector->nr_elements-1);
  }

  return 0;
}


static void *
oggz_vector_remove_nth (OggzVector * vector, int n)
{
  int i;
  oggz_data_t * new_elements;
  int new_max_elements;

  vector->nr_elements--;

  if (vector->nr_elements == 0) {
    oggz_vector_clear (vector);
  } else {
    for (i = n; i < vector->nr_elements; i++) {
      vector->data[i] = vector->data[i+1];
    }

    if (vector->nr_elements < vector->max_elements/2) {
      new_max_elements = vector->max_elements/2;

      new_elements =
        oggz_realloc (vector->data,
        (size_t)new_max_elements * sizeof (oggz_data_t));

      if (new_elements == NULL) {
        vector->data = NULL;
        return NULL;
      }

      vector->max_elements = new_max_elements;
      vector->data = new_elements;
    }
  }

  return vector;
}

OggzVector *
oggz_vector_remove_p (OggzVector * vector, void * data)
{
  int i;

  for (i = 0; i < vector->nr_elements; i++) {
    if (vector->data[i].p == data) {
      return oggz_vector_remove_nth (vector, i);
    }
  }

  return vector;
}

OggzVector *
oggz_vector_remove_l (OggzVector * vector, long ldata)
{
  int i;

  for (i = 0; i < vector->nr_elements; i++) {
    if (vector->data[i].l == ldata) {
      return oggz_vector_remove_nth (vector, i);
    }
  }

  return vector;
}

void *
oggz_vector_pop (OggzVector * vector)
{
  void * data;

  if (vector == NULL || vector->data == NULL) return NULL;

  data = vector->data[0].p;

  oggz_vector_remove_nth (vector, 0);

  return data;

}
