/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1999-2004 Carnegie Mellon University.  All rights
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
/*
 * bitvec.c -- Bit vector type.
 *
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1999 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * $Log: bitvec.c,v $
 * Revision 1.4  2005/06/22 02:58:22  arthchan2003
 * Added  keyword
 *
 * Revision 1.3  2005/03/30 01:22:48  archan
 * Fixed mistakes in last updates. Add
 *
 * 
 * 05-Mar-1999	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon
 * 		Started.
 */


#include "sphinxbase/bitvec.h"

bitvec_t *
bitvec_realloc(bitvec_t *vec,
	       size_t old_len,
               size_t new_len)
{
    bitvec_t *new_vec;
    size_t old_size = bitvec_size(old_len);
    size_t new_size = bitvec_size(new_len);
    
    new_vec = ckd_realloc(vec, new_size * sizeof(bitvec_t));
    if (new_size > old_size)
	memset(new_vec + old_size, 0, (new_size - old_size) * sizeof(bitvec_t));

    return new_vec;
}

size_t
bitvec_count_set(bitvec_t *vec, size_t len)
{
    size_t words, bits, w, b, n;
    bitvec_t *v;

    words = len / BITVEC_BITS;
    bits = len % BITVEC_BITS;
    v = vec;
    n = 0;
    for (w = 0; w < words; ++w, ++v) {
        if (*v == 0)
            continue;
        for (b = 0; b < BITVEC_BITS; ++b)
            if (*v & (1<<b))
                ++n;
    }
    for (b = 0; b < bits; ++b)
        if (*v & (1<<b))
            ++n;

    return n;
}
