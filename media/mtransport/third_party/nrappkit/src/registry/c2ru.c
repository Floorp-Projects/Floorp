/*
 *
 *    c2ru.c
 *
 *    $Source: /Users/ekr/tmp/nrappkit-dump/nrappkit/src/registry/c2ru.c,v $
 *    $Revision: 1.3 $
 *    $Date: 2007/06/26 22:37:50 $
 *
 *    c2r utility methods
 *
 *
 *    Copyright (C) 2005, Network Resonance, Inc.
 *    Copyright (C) 2006, Network Resonance, Inc.
 *    All Rights Reserved
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions
 *    are met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of Network Resonance, Inc. nor the name of any
 *       contributors to this software may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

#include <sys/queue.h>
#include <string.h>
#include <registry.h>
#include "nr_common.h"
#include <r_errors.h>
#include <r_macros.h>
#include <ctype.h>
#include "c2ru.h"


#define NRGET(func, type, get) \
int                                                                  \
func(NR_registry parent, char *child, type **out)                    \
{                                                                    \
  int r, _status;                                                    \
  NR_registry registry;                                              \
  type tmp;                                                          \
                                                                     \
  if ((r = nr_c2ru_make_registry(parent, child, registry)))          \
    ABORT(r);                                                        \
                                                                     \
  if ((r = get(registry, &tmp))) {                                   \
    if (r != R_NOT_FOUND)                                            \
      ABORT(r);                                                      \
    *out = 0;                                                        \
  }                                                                  \
  else {                                                             \
    *out = RCALLOC(sizeof(tmp));                                     \
    if (*out == 0)                                                   \
      ABORT(R_NO_MEMORY);                                            \
    **out = tmp;                                                     \
  }                                                                  \
                                                                     \
  _status = 0;                                                       \
abort:                                                               \
  return (_status);                                                  \
}

int
nr_c2ru_get_char(NR_registry parent, char *child, char **out)
{
  int r, _status;
  NR_registry registry;
  char tmp;

  if ((r = nr_c2ru_make_registry(parent, child, registry)))
    ABORT(r);

  if ((r = NR_reg_get_char(registry, &tmp))) {
    if (r != R_NOT_FOUND)
      ABORT(r);
    *out = 0;
  }
  else {
    *out = RCALLOC(sizeof(tmp));
    if (*out == 0)
      ABORT(R_NO_MEMORY);
    **out = tmp;
  }

  _status = 0;
abort:
  return (_status);
}
NRGET(nr_c2ru_get_uchar,    UCHAR,   NR_reg_get_uchar)
NRGET(nr_c2ru_get_int2,     INT2,    NR_reg_get_int2)
NRGET(nr_c2ru_get_uint2,    UINT2,   NR_reg_get_uint2)
NRGET(nr_c2ru_get_int4,     INT4,    NR_reg_get_int4)
NRGET(nr_c2ru_get_uint4,    UINT4,   NR_reg_get_uint4)
NRGET(nr_c2ru_get_int8,     INT8,    NR_reg_get_int8)
NRGET(nr_c2ru_get_uint8,    UINT8,   NR_reg_get_uint8)
NRGET(nr_c2ru_get_double,   double,  NR_reg_get_double)
NRGET(nr_c2ru_get_string,   char*,   NR_reg_alloc_string)
NRGET(nr_c2ru_get_data,     Data,    NR_reg_alloc_data)


#define NRSET(func, type, set) \
int                                                                  \
func(NR_registry parent, char *child, type *in)                      \
{                                                                    \
  int r, _status;                                                    \
  NR_registry registry;                                              \
                                                                     \
  if (in == 0)                                                       \
    return 0;                                                        \
                                                                     \
  if ((r = nr_c2ru_make_registry(parent, child, registry)))          \
    ABORT(r);                                                        \
                                                                     \
  if ((r = set(registry, *in)))                                      \
    ABORT(r);                                                        \
                                                                     \
  _status = 0;                                                       \
abort:                                                               \
  return (_status);                                                  \
}

NRSET(nr_c2ru_set_char,      char,       NR_reg_set_char)
NRSET(nr_c2ru_set_uchar,     UCHAR,      NR_reg_set_uchar)
NRSET(nr_c2ru_set_int2,      INT2,       NR_reg_set_int2)
NRSET(nr_c2ru_set_uint2,     UINT2,      NR_reg_set_uint2)
NRSET(nr_c2ru_set_int4,      INT4,       NR_reg_set_int4)
NRSET(nr_c2ru_set_uint4,     UINT4,      NR_reg_set_uint4)
NRSET(nr_c2ru_set_int8,      INT8,       NR_reg_set_int8)
NRSET(nr_c2ru_set_uint8,     UINT8,      NR_reg_set_uint8)
NRSET(nr_c2ru_set_double,    double,     NR_reg_set_double)
NRSET(nr_c2ru_set_string,    char*,      NR_reg_set_string)

int
nr_c2ru_set_data(NR_registry parent, char *child, Data *in)
{
  int r, _status;
  NR_registry registry;

  if (in == 0)
    return 0;

  if ((r = nr_c2ru_make_registry(parent, child, registry)))
    ABORT(r);

  if ((r = NR_reg_set_bytes(registry, in->data, in->len)))
    ABORT(r);

  _status = 0;
abort:
  return (_status);
}

#define NRFREE(func, type) \
int                                                                  \
func(type *in)                                                       \
{                                                                    \
  if (in)                                                            \
    RFREE(in);                                                       \
  return 0;                                                          \
}

NRFREE(nr_c2ru_free_char,      char)
NRFREE(nr_c2ru_free_uchar,     UCHAR)
NRFREE(nr_c2ru_free_int2,      INT2)
NRFREE(nr_c2ru_free_uint2,     UINT2)
NRFREE(nr_c2ru_free_int4,      INT4)
NRFREE(nr_c2ru_free_uint4,     UINT4)
NRFREE(nr_c2ru_free_int8,      INT8)
NRFREE(nr_c2ru_free_uint8,     UINT8)
NRFREE(nr_c2ru_free_double,    double)


int
nr_c2ru_free_string(char **in)
{
  if (*in)
    RFREE(*in);
  if (in)
    RFREE(in);
  return 0;
}

int
nr_c2ru_free_data(Data *in)
{
  int r, _status;

  if (in) {
    if ((r=r_data_destroy(&in)))
      ABORT(r);
  }

  _status = 0;
abort:
  return (_status);
}

int
nr_c2ru_get_children(NR_registry parent, char *child, void *ptr, size_t size, int (*get)(NR_registry, void*))
{
  int r, _status;
  NR_registry registry;
  unsigned int count;
  int i;
  NR_registry name;
  struct entry { TAILQ_ENTRY(entry) entries; } *entry;
  TAILQ_HEAD(, entry) *tailq = (void*)ptr;

  TAILQ_INIT(tailq);

  if ((r=nr_c2ru_make_registry(parent, child, registry)))
    ABORT(r);

  if ((r=NR_reg_get_child_count(registry, &count))) {
    if (r != R_NOT_FOUND)
      ABORT(r);
  }
  else {
    for (i = 0; i < count; ++i) {
      if ((r=NR_reg_get_child_registry(registry, i, name))) {
        /* ignore R_NOT_FOUND errors */
        if (r == R_NOT_FOUND)
          continue;
        else
          ABORT(r);
      }

      if ((r=get(name, &entry))) {
        /* ignore R_NOT_FOUND errors */
        if (r == R_NOT_FOUND)
          continue;
        else
          ABORT(r);
      }

      TAILQ_INSERT_TAIL(tailq, entry, entries);
    }
  }

  _status = 0;
abort:
  return (_status);
}

int
nr_c2ru_set_children(NR_registry parent, char *child, void *ptr, int (*set)(NR_registry, void*), int (*label)(NR_registry, void*, char[NR_REG_MAX_NR_REGISTRY_LEN]))
{
  int r, _status;
  NR_registry registry;
  int i;
  NR_registry name;
  char buffer[NR_REG_MAX_NR_REGISTRY_LEN];
  struct entry { TAILQ_ENTRY(entry) entries; } *entry;
  TAILQ_HEAD(, entry) *tailq = (void*)ptr;

  if ((r=nr_c2ru_make_registry(parent, child, registry)))
    ABORT(r);

  (void)NR_reg_del(registry);

  i = 0;
  TAILQ_FOREACH(entry, tailq, entries) {
    if (label == 0 || (r=label(registry, entry, buffer))) {
      snprintf(buffer, sizeof(buffer), "%d", i);
    }
    if ((r=nr_c2ru_make_registry(registry, buffer, name)))
      ABORT(r);

    if ((r=set(name, entry)))
      ABORT(r);

    ++i;
  }

  _status = 0;
abort:
  return (_status);
}

int
nr_c2ru_free_children(void *ptr, int (*free)(void*))
{
  struct entry { TAILQ_ENTRY(entry) entries; } *entry;
  TAILQ_HEAD(, entry) *tailq = (void*)ptr;

  while (! TAILQ_EMPTY(tailq)) {
    entry = TAILQ_FIRST(tailq);
    TAILQ_REMOVE(tailq, entry, entries);
    (void)free(entry);
  }

  return 0;
}

/* requires parent already in legal form */
int
nr_c2ru_make_registry(NR_registry parent, char *child, NR_registry out)
{
    return NR_reg_make_registry(parent, child, out);
}
