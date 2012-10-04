/*
 *
 *    c2ru.h
 *
 *    $Source: /Users/ekr/tmp/nrappkit-dump/nrappkit/src/registry/c2ru.h,v $
 *    $Revision: 1.2 $
 *    $Date: 2006/08/16 19:39:13 $
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

#ifndef __C2RU_H__
#define __C2RU_H__

#include <sys/types.h>
#include <r_types.h>
#include <r_data.h>
#include "registry_int.h"

int nr_c2ru_get_char(NR_registry parent, char *child, char **out);
int nr_c2ru_get_uchar(NR_registry parent, char *child, UCHAR **out);
int nr_c2ru_get_int2(NR_registry parent, char *child, INT2 **out);
int nr_c2ru_get_uint2(NR_registry parent, char *child, UINT2 **out);
int nr_c2ru_get_int4(NR_registry parent, char *child, INT4 **out);
int nr_c2ru_get_uint4(NR_registry parent, char *child, UINT4 **out);
int nr_c2ru_get_int8(NR_registry parent, char *child, INT8 **out);
int nr_c2ru_get_uint8(NR_registry parent, char *child, UINT8 **out);
int nr_c2ru_get_double(NR_registry parent, char *child, double **out);
int nr_c2ru_get_data(NR_registry parent, char *child, Data **out);
int nr_c2ru_get_string(NR_registry parent, char *child, char ***out);

int nr_c2ru_set_char(NR_registry parent, char *child, char *data);
int nr_c2ru_set_uchar(NR_registry parent, char *child, UCHAR *data);
int nr_c2ru_set_int2(NR_registry parent, char *child, INT2 *data);
int nr_c2ru_set_uint2(NR_registry parent, char *child, UINT2 *data);
int nr_c2ru_set_int4(NR_registry parent, char *child, INT4 *data);
int nr_c2ru_set_uint4(NR_registry parent, char *child, UINT4 *data);
int nr_c2ru_set_int8(NR_registry parent, char *child, INT8 *data);
int nr_c2ru_set_uint8(NR_registry parent, char *child, UINT8 *data);
int nr_c2ru_set_double(NR_registry parent, char *child, double *data);
int nr_c2ru_set_registry(NR_registry parent, char *child);
int nr_c2ru_set_data(NR_registry parent, char *child, Data *data);
int nr_c2ru_set_string(NR_registry parent, char *child, char **data);

int nr_c2ru_free_char(char *data);
int nr_c2ru_free_uchar(UCHAR *data);
int nr_c2ru_free_int2(INT2 *data);
int nr_c2ru_free_uint2(UINT2 *data);
int nr_c2ru_free_int4(INT4 *data);
int nr_c2ru_free_uint4(UINT4 *data);
int nr_c2ru_free_int8(INT8 *data);
int nr_c2ru_free_uint8(UINT8 *data);
int nr_c2ru_free_double(double *data);
int nr_c2ru_free_data(Data *data);
int nr_c2ru_free_string(char **data);

int nr_c2ru_get_children(NR_registry parent, char *child, void *ptr, size_t size, int (*get)(NR_registry, void*));
int nr_c2ru_set_children(NR_registry parent, char *child, void *ptr, int (*set)(NR_registry, void*), int (*label)(NR_registry, void*, char[NR_REG_MAX_NR_REGISTRY_LEN]));
int nr_c2ru_free_children(void *ptr, int (*free)(void*));

int nr_c2ru_make_registry(NR_registry parent, char *child, NR_registry out);

#endif
