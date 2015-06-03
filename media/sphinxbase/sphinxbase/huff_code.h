/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 2009 Carnegie Mellon University.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * This work was supported in part by funding from the Defense Advanced 
 * Research Projects Agency and the National Science Foundation of the 
 * United States of America, and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND 
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================
 *
 */

/**
 * @file huff_code.h
 * @brief Huffman code and bitstream implementation
 *
 * This interface supports building canonical Huffman codes from
 * string and integer values.  It also provides support for encoding
 * and decoding from strings and files, and for reading and writing
 * codebooks from files.
 */

#ifndef __HUFF_CODE_H__
#define __HUFF_CODE_H__

#include <stdio.h>

#include <sphinxbase/sphinxbase_export.h>
#include <sphinxbase/prim_type.h>
#include <sphinxbase/cmd_ln.h>

typedef struct huff_code_s huff_code_t;

/**
 * Create a codebook from 32-bit integer data.
 */
SPHINXBASE_EXPORT
huff_code_t *huff_code_build_int(int32 const *values, int32 const *frequencies, int nvals);

/**
 * Create a codebook from string data.
 */
SPHINXBASE_EXPORT
huff_code_t *huff_code_build_str(char * const *values, int32 const *frequencies, int nvals);

/**
 * Read a codebook from a file.
 */
SPHINXBASE_EXPORT
huff_code_t *huff_code_read(FILE *infh);

/**
 * Write a codebook to a file.
 */
SPHINXBASE_EXPORT
int huff_code_write(huff_code_t *hc, FILE *outfh);

/**
 * Print a codebook to a file as text (for debugging)
 */
SPHINXBASE_EXPORT
int huff_code_dump(huff_code_t *hc, FILE *dumpfh);

/**
 * Retain a pointer to a Huffman codec object.
 */
SPHINXBASE_EXPORT
huff_code_t *huff_code_retain(huff_code_t *hc);

/**
 * Release a pointer to a Huffman codec object.
 */
SPHINXBASE_EXPORT
int huff_code_free(huff_code_t *hc);

/**
 * Attach a Huffman codec to a file handle for input/output.
 */
SPHINXBASE_EXPORT
FILE *huff_code_attach(huff_code_t *hc, FILE *fh, char const *mode);

/**
 * Detach a Huffman codec from its file handle.
 */
SPHINXBASE_EXPORT
FILE *huff_code_detach(huff_code_t *hc);

/**
 * Encode an integer, writing it to the file handle, if any.
 */
SPHINXBASE_EXPORT
int huff_code_encode_int(huff_code_t *hc, int32 sym, uint32 *outcw);

/**
 * Encode a string, writing it to the file handle, if any.
 */
SPHINXBASE_EXPORT
int huff_code_encode_str(huff_code_t *hc, char const *sym, uint32 *outcw);

/**
 * Decode an integer, reading it from the file if no data given.
 */
SPHINXBASE_EXPORT
int huff_code_decode_int(huff_code_t *hc, int *outval,
                         char const **inout_data,
                         size_t *inout_data_len,
                         int *inout_offset);

/**
 * Decode a string, reading it from the file if no data given.
 */
SPHINXBASE_EXPORT
char const *huff_code_decode_str(huff_code_t *hc,
                                 char const **inout_data,
                                 size_t *inout_data_len,
                                 int *inout_offset);

#endif /* __HUFF_CODE_H__ */
