/*
 *
 *    registry_vtbl.h
 *
 *    $Source: /Users/ekr/tmp/nrappkit-dump/nrappkit/src/registry/registry_vtbl.h,v $
 *    $Revision: 1.2 $
 *    $Date: 2006/08/16 19:39:14 $
 *
 *    
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

#ifndef __REGISTRY_VTBL_H__
#define __REGISTRY_VTBL_H__

typedef struct nr_registry_module_ nr_registry_module;

typedef struct nr_registry_module_vtbl_ {
    int (*init)(nr_registry_module*);

    int (*get_char)(NR_registry name, char *out);
    int (*get_uchar)(NR_registry name, UCHAR *out);
    int (*get_int2)(NR_registry name, INT2 *out);
    int (*get_uint2)(NR_registry name, UINT2 *out);
    int (*get_int4)(NR_registry name, INT4 *out);
    int (*get_uint4)(NR_registry name, UINT4 *out);
    int (*get_int8)(NR_registry name, INT8 *out);
    int (*get_uint8)(NR_registry name, UINT8 *out);
    int (*get_double)(NR_registry name, double *out);
    int (*get_registry)(NR_registry name, NR_registry out);

    int (*get_bytes)(NR_registry name, UCHAR *out, size_t size, size_t *length);
    int (*get_string)(NR_registry name, char *out, size_t size);
    int (*get_length)(NR_registry name, size_t *length);
    int (*get_type)(NR_registry name, NR_registry_type type);

    int (*set_char)(NR_registry name, char data);
    int (*set_uchar)(NR_registry name, UCHAR data);
    int (*set_int2)(NR_registry name, INT2 data);
    int (*set_uint2)(NR_registry name, UINT2 data);
    int (*set_int4)(NR_registry name, INT4 data);
    int (*set_uint4)(NR_registry name, UINT4 data);
    int (*set_int8)(NR_registry name, INT8 data);
    int (*set_uint8)(NR_registry name, UINT8 data);
    int (*set_double)(NR_registry name, double data);
    int (*set_registry)(NR_registry name);

    int (*set_bytes)(NR_registry name, UCHAR *data, size_t length);
    int (*set_string)(NR_registry name, char *data);

    int (*del)(NR_registry name);

    int (*get_child_count)(NR_registry parent, size_t *count);
    int (*get_children)(NR_registry parent, NR_registry *data, size_t size, size_t *length);

    int (*fin)(NR_registry name);

    int (*dump)(int sorted);
} nr_registry_module_vtbl;

struct nr_registry_module_ {
    void *handle;
    nr_registry_module_vtbl *vtbl;
};

#endif

