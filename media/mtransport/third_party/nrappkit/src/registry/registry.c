/*
 *
 *    registry.c
 *
 *    $Source: /Users/ekr/tmp/nrappkit-dump/nrappkit/src/registry/registry.c,v $
 *    $Revision: 1.6 $
 *    $Date: 2007/11/21 00:09:12 $
 *
 *    Datastore for tracking configuration and related info.
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

#include <assert.h>
#include <string.h>
#ifndef _MSC_VER
#include <strings.h>
#include <sys/param.h>
#include <netinet/in.h>
#endif
#ifdef OPENSSL
#include <openssl/ssl.h>
#endif
#include <ctype.h>
#include "registry.h"
#include "registry_int.h"
#include "registry_vtbl.h"
#include "r_assoc.h"
#include "nr_common.h"
#include "r_log.h"
#include "r_errors.h"
#include "r_macros.h"
#include "c2ru.h"

/* vtbl used to switch hit between local and remote invocations */
static nr_registry_module *reg_vtbl = 0;

/* must be in the order the types are numbered */
static char *typenames[] = { "char", "UCHAR", "INT2", "UINT2", "INT4", "UINT4", "INT8", "UINT8", "double", "Data", "string", "registry" };

int NR_LOG_REGISTRY=0;

NR_registry NR_TOP_LEVEL_REGISTRY = "";

int
NR_reg_init(void *mode)
{
    int r, _status;
    nr_registry_module *module = (nr_registry_module*)mode;
#ifdef SANITY_CHECKS
    NR_registry registry;
#endif

    if (reg_vtbl) {
        if (reg_vtbl != module) {
          r_log(LOG_GENERIC,LOG_ERR,"Can't reinitialize registry in different mode");
          ABORT(R_INTERNAL);
        }

        return(0);
    }

    reg_vtbl = module;

    if ((r=reg_vtbl->vtbl->init(mode)))
        ABORT(r);

#ifdef SANITY_CHECKS
    if ((r=NR_reg_get_registry(NR_TOP_LEVEL_REGISTRY, registry)))
        ABORT(r);
    assert(strcmp(registry, NR_TOP_LEVEL_REGISTRY) == 0);
#endif

     r_log_init();
     r_log_register("registry",&NR_LOG_REGISTRY);

    _status=0;
  abort:
    r_log(NR_LOG_REGISTRY,
          (_status ? LOG_ERR                        :  LOG_INFO),
          (_status ? "Couldn't initialize registry" : "Initialized registry"));
    return(_status);
}

int
NR_reg_initted(void)
{
    return reg_vtbl!=0;
}

#define NRREGGET(func, method, type)                                \
int                                                                 \
func(NR_registry name, type *out)                                   \
{                                                                   \
    return reg_vtbl->vtbl->method(name, out);                             \
}

NRREGGET(NR_reg_get_char,     get_char,     char)
NRREGGET(NR_reg_get_uchar,    get_uchar,    UCHAR)
NRREGGET(NR_reg_get_int2,     get_int2,     INT2)
NRREGGET(NR_reg_get_uint2,    get_uint2,    UINT2)
NRREGGET(NR_reg_get_int4,     get_int4,     INT4)
NRREGGET(NR_reg_get_uint4,    get_uint4,    UINT4)
NRREGGET(NR_reg_get_int8,     get_int8,     INT8)
NRREGGET(NR_reg_get_uint8,    get_uint8,    UINT8)
NRREGGET(NR_reg_get_double,   get_double,   double)

int
NR_reg_get_registry(NR_registry name, NR_registry out)
{
    return reg_vtbl->vtbl->get_registry(name, out);
}

int
NR_reg_get_bytes(NR_registry name, UCHAR *out, size_t size, size_t *length)
{
    return reg_vtbl->vtbl->get_bytes(name, out, size, length);
}

int
NR_reg_get_string(NR_registry name, char *out, size_t size)
{
    return reg_vtbl->vtbl->get_string(name, out, size);
}

int
NR_reg_get_length(NR_registry name, size_t *length)
{
    return reg_vtbl->vtbl->get_length(name, length);
}

int
NR_reg_get_type(NR_registry name, NR_registry_type type)
{
    return reg_vtbl->vtbl->get_type(name, type);
}

#define NRREGSET(func, method, type)                            \
int                                                             \
func(NR_registry name, type data)                               \
{                                                               \
    return reg_vtbl->vtbl->method(name, data);                        \
}

NRREGSET(NR_reg_set_char,     set_char,     char)
NRREGSET(NR_reg_set_uchar,    set_uchar,    UCHAR)
NRREGSET(NR_reg_set_int2,     set_int2,     INT2)
NRREGSET(NR_reg_set_uint2,    set_uint2,    UINT2)
NRREGSET(NR_reg_set_int4,     set_int4,     INT4)
NRREGSET(NR_reg_set_uint4,    set_uint4,    UINT4)
NRREGSET(NR_reg_set_int8,     set_int8,     INT8)
NRREGSET(NR_reg_set_uint8,    set_uint8,    UINT8)
NRREGSET(NR_reg_set_double,   set_double,   double)
NRREGSET(NR_reg_set_string,   set_string,   char*)

int
NR_reg_set_registry(NR_registry name)
{
    return reg_vtbl->vtbl->set_registry(name);
}

int
NR_reg_set_bytes(NR_registry name, unsigned char *data, size_t length)
{
    return reg_vtbl->vtbl->set_bytes(name, data, length);
}


int
NR_reg_del(NR_registry name)
{
    return reg_vtbl->vtbl->del(name);
}

int
NR_reg_fin(NR_registry name)
{
    return reg_vtbl->vtbl->fin(name);
}

int
NR_reg_get_child_count(char *parent, unsigned int *count)
{
    assert(sizeof(count) == sizeof(size_t));
    return reg_vtbl->vtbl->get_child_count(parent, (size_t*)count);
}

int
NR_reg_get_child_registry(char *parent, unsigned int i, NR_registry child)
{
    int r, _status;
    size_t count;
    NR_registry *children=0;

    if ((r=reg_vtbl->vtbl->get_child_count(parent, &count)))
      ABORT(r);

    if (i >= count)
        ABORT(R_NOT_FOUND);
    else {
        count++;
        children = (NR_registry *)RCALLOC(count * sizeof(NR_registry));
        if (!children)
            ABORT(R_NO_MEMORY);

        if ((r=reg_vtbl->vtbl->get_children(parent, children, count, &count)))
            ABORT(r);

        if (i >= count)
            ABORT(R_NOT_FOUND);

        strncpy(child, children[i], sizeof(NR_registry));
    }

    _status=0;
  abort:
    RFREE(children);
    return(_status);
}

int
NR_reg_get_children(NR_registry parent, NR_registry *children, size_t size, size_t *length)
{
    return reg_vtbl->vtbl->get_children(parent, children, size, length);
}

int
NR_reg_dump()
{
    int r, _status;

    if ((r=reg_vtbl->vtbl->dump(0)))
      ABORT(r);

    _status=0;
  abort:
    return(_status);
}

// convenience methods, call RFREE on the returned data
int
NR_reg_alloc_data(NR_registry name, Data *data)
{
    int r, _status;
    size_t length;
    UCHAR  *tmp = 0;
    size_t sanity_check;

    if ((r=NR_reg_get_length(name, &length)))
      ABORT(r);

    if (!(tmp = (void*)RMALLOC(length)))
      ABORT(R_NO_MEMORY);

    if ((r=NR_reg_get_bytes(name, tmp, length, &sanity_check)))
      ABORT(r);

    assert(length == sanity_check);

    data->len = length;
    data->data = tmp;

    _status=0;
  abort:
    if (_status) {
      if (tmp) RFREE(tmp);
    }
    return(_status);
}

int
NR_reg_alloc_string(NR_registry name, char **data)
{
    int r, _status;
    size_t length;
    char  *tmp = 0;

    if ((r=NR_reg_get_length(name, &length)))
      ABORT(r);

    if (!(tmp = (void*)RMALLOC(length+1)))
      ABORT(R_NO_MEMORY);

    if ((r=NR_reg_get_string(name, tmp, length+1)))
      ABORT(r);

    assert(length == strlen(tmp));

    *data = tmp;

    _status=0;
  abort:
    if (_status) {
      if (tmp) RFREE(tmp);
    }
    return(_status);
}


char *
nr_reg_type_name(int type)
{
    if ((type < NR_REG_TYPE_CHAR) || (type > NR_REG_TYPE_REGISTRY))
       return(NULL);

    return(typenames[type]);
}

int
nr_reg_compute_type(char *typename, int *type)
{
    int _status;
    size_t i;

#ifdef SANITY_CHECKS
    assert(!strcasecmp(typenames[NR_REG_TYPE_CHAR],     "char"));
    assert(!strcasecmp(typenames[NR_REG_TYPE_UCHAR],    "UCHAR"));
    assert(!strcasecmp(typenames[NR_REG_TYPE_INT2],     "INT2"));
    assert(!strcasecmp(typenames[NR_REG_TYPE_UINT2],    "UINT2"));
    assert(!strcasecmp(typenames[NR_REG_TYPE_INT4],     "INT4"));
    assert(!strcasecmp(typenames[NR_REG_TYPE_UINT4],    "UINT4"));
    assert(!strcasecmp(typenames[NR_REG_TYPE_INT8],     "INT8"));
    assert(!strcasecmp(typenames[NR_REG_TYPE_UINT8],    "UINT8"));
    assert(!strcasecmp(typenames[NR_REG_TYPE_DOUBLE],   "double"));
    assert(!strcasecmp(typenames[NR_REG_TYPE_BYTES],    "Data"));
    assert(!strcasecmp(typenames[NR_REG_TYPE_STRING],   "string"));
    assert(!strcasecmp(typenames[NR_REG_TYPE_REGISTRY], "registry"));
    assert(sizeof(typenames)/sizeof(*typenames) == (NR_REG_TYPE_REGISTRY+1));
#endif

    for (i = 0; i < sizeof(typenames)/sizeof(*typenames); ++i) {
      if (!strcasecmp(typenames[i], typename)) {
        *type = i;
        return 0;
      }
    }
    ABORT(R_BAD_ARGS);

    _status=0;
  abort:
    return(_status);
}

/* More convenience functions: the same as their parents but they
   take a prefix and a suffix */
#define NRGET2(func, type, get) \
int                                                                  \
func(NR_registry parent, char *child, type *out)                     \
{                                                                    \
  int r, _status;                                                    \
  NR_registry registry;                                              \
                                                                     \
  if ((r = NR_reg_make_registry(parent, child, registry)))           \
    ABORT(r);                                                        \
                                                                     \
  if ((r = get(registry, out))) {                                    \
    ABORT(r);                                                        \
  }                                                                  \
                                                                     \
  _status = 0;                                                       \
abort:                                                               \
  return (_status);                                                  \
}

NRGET2(NR_reg_get2_char,     char,    NR_reg_get_char)
NRGET2(NR_reg_get2_uchar,    UCHAR,   NR_reg_get_uchar)
NRGET2(NR_reg_get2_int2,     INT2,    NR_reg_get_int2)
NRGET2(NR_reg_get2_uint2,    UINT2,   NR_reg_get_uint2)
NRGET2(NR_reg_get2_int4,     INT4,    NR_reg_get_int4)
NRGET2(NR_reg_get2_uint4,    UINT4,   NR_reg_get_uint4)
NRGET2(NR_reg_get2_int8,     INT8,    NR_reg_get_int8)
NRGET2(NR_reg_get2_uint8,    UINT8,   NR_reg_get_uint8)
NRGET2(NR_reg_get2_double,   double,  NR_reg_get_double)
NRGET2(NR_reg_alloc2_string,   char*,   NR_reg_alloc_string)
NRGET2(NR_reg_alloc2_data,     Data,    NR_reg_alloc_data)

int
NR_reg_get2_bytes(NR_registry parent, char *child, UCHAR *out, size_t size, size_t *length)
{
    int r, _status;
    NR_registry registry;

    if ((r=NR_reg_make_registry(parent, child, registry)))
      ABORT(r);

    if ((r=NR_reg_get_bytes(registry, out, size, length)))
      ABORT(r);

    _status = 0;
abort:
    return (_status);
}

int
NR_reg_get2_string(NR_registry parent, char *child, char *out, size_t size)
{
    int r, _status;
    NR_registry registry;

    if ((r=NR_reg_make_registry(parent, child, registry)))
      ABORT(r);

    if ((r=NR_reg_get_string(registry, out, size)))
      ABORT(r);

    _status = 0;
abort:
    return (_status);
}

/* More convenience functions: the same as their parents but they
   take a prefix and a suffix */
#define NRSET2(func, type, set) \
int                                                                  \
func(NR_registry parent, char *child, type in)                       \
{                                                                    \
  int r, _status;                                                    \
  NR_registry registry;                                              \
                                                                     \
  if ((r = NR_reg_make_registry(parent, child, registry)))           \
    ABORT(r);                                                        \
                                                                     \
  if ((r = set(registry, in))) {                                     \
    ABORT(r);                                                        \
  }                                                                  \
                                                                     \
  _status = 0;                                                       \
abort:                                                               \
  return (_status);                                                  \
}

NRSET2(NR_reg_set2_char,     char,    NR_reg_set_char)
NRSET2(NR_reg_set2_uchar,    UCHAR,   NR_reg_set_uchar)
NRSET2(NR_reg_set2_int2,     INT2,    NR_reg_set_int2)
NRSET2(NR_reg_set2_uint2,    UINT2,   NR_reg_set_uint2)
NRSET2(NR_reg_set2_int4,     INT4,    NR_reg_set_int4)
NRSET2(NR_reg_set2_uint4,    UINT4,   NR_reg_set_uint4)
NRSET2(NR_reg_set2_int8,     INT8,    NR_reg_set_int8)
NRSET2(NR_reg_set2_uint8,    UINT8,   NR_reg_set_uint8)
NRSET2(NR_reg_set2_double,   double,  NR_reg_set_double)
NRSET2(NR_reg_set2_string,   char*,   NR_reg_set_string)

int
NR_reg_set2_bytes(NR_registry prefix, char *name, UCHAR *data, size_t length)
{
    int r, _status;
    NR_registry registry;

    if ((r = NR_reg_make_registry(prefix, name, registry)))
      ABORT(r);

    if ((r = NR_reg_set_bytes(registry, data, length)))
      ABORT(r);

    _status = 0;
abort:
    return (_status);
}


int
NR_reg_make_child_registry(NR_registry parent, NR_registry descendant, unsigned int generation, NR_registry child)
{
    int _status;
    size_t length;

    length = strlen(parent);

    if (strncasecmp(parent, descendant, length))
        ABORT(R_BAD_ARGS);

    while (descendant[length] != '\0') {
        if (descendant[length] == '.') {
            if (generation == 0)
                break;

            --generation;
        }

        ++length;
        if (length >= sizeof(NR_registry))
            ABORT(R_BAD_ARGS);
    }

    strncpy(child, descendant, length);
    child[length] = '\0';

    _status=0;
  abort:
    return(_status);
}

int
NR_reg_get2_child_count(NR_registry base, NR_registry name, unsigned int *count)
  {
    int r, _status;
    NR_registry registry;

    if ((r=nr_c2ru_make_registry(base, name, registry)))
      ABORT(r);

    if (r=NR_reg_get_child_count(registry,count))
      ABORT(r);

    _status=0;
  abort:
    return(_status);
  }

int
NR_reg_get2_child_registry(NR_registry base, NR_registry name, unsigned int i, NR_registry child)
  {
    int r, _status;
    NR_registry registry;

    if ((r=nr_c2ru_make_registry(base, name, registry)))
      ABORT(r);

    if (r=NR_reg_get_child_registry(registry, i, child))
      ABORT(r);

    _status=0;
  abort:
    return(_status);
  }


/* requires parent already in legal form */
int
NR_reg_make_registry(NR_registry parent, char *child, NR_registry out)
{
    int r, _status;
    size_t plen;
    size_t clen;
    char *c;
    size_t i;

    if ((r=nr_reg_is_valid(parent)))
        ABORT(r);

    if (*child == '.')
        ABORT(R_BAD_ARGS);

    clen = strlen(child);
    if (!clen)
        ABORT(R_BAD_ARGS);
    plen = strlen(parent);
    if ((plen + clen + 2) > sizeof(NR_registry))
        ABORT(R_BAD_ARGS);

    if (out != parent)
        strcpy(out, parent);

    c = &(out[plen]);

    if (parent[0] != '\0') {
        *c = '.';
        ++c;
    }

    for (i = 0; i < clen; ++i, ++c) {
        *c = child[i];
        if (isspace(*c) || *c == '.' || *c == '/' || ! isprint(*c))
            *c = '_';
    }

    *c = '\0';

    _status = 0;
abort:
    return _status;
}

