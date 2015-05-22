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
 * vector.c
 *
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1997 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * 
 * 22-Nov-2004	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Imported from s3.2, for supporting s3 format continuous
 * 		acoustic models.
 * 
 * 10-Mar-1999	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University.
 * 		Added vector_accum(), vector_vqlabel(), and vector_vqgen().
 * 
 * 09-Mar-1999	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University.
 * 		Added vector_is_zero(), vector_cmp(), and vector_dist_eucl().
 * 		Changed the name vector_dist_eval to vector_dist_maha.
 * 
 * 07-Oct-98	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University.
 * 		Added distance computation related functions.
 * 
 * 12-Nov-95	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University.
 * 		Copied from Eric Thayer.
 */

/* System headers. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

/* SphinxBase headers. */
#include <sphinxbase/err.h>
#include <sphinxbase/ckd_alloc.h>
#include <sphinxbase/bitvec.h>

/* Local headers. */
#include "vector.h"

#if defined(_WIN32)
#define srandom	srand
#define random	rand
#endif


float64
vector_sum_norm(float32 * vec, int32 len)
{
    float64 sum, f;
    int32 i;

    sum = 0.0;
    for (i = 0; i < len; i++)
        sum += vec[i];

    if (sum != 0.0) {
        f = 1.0 / sum;
        for (i = 0; i < len; i++)
            vec[i] *= f;
    }

    return sum;
}


void
vector_floor(float32 * vec, int32 len, float64 flr)
{
    int32 i;

    for (i = 0; i < len; i++)
        if (vec[i] < flr)
            vec[i] = (float32) flr;
}


void
vector_nz_floor(float32 * vec, int32 len, float64 flr)
{
    int32 i;

    for (i = 0; i < len; i++)
        if ((vec[i] != 0.0) && (vec[i] < flr))
            vec[i] = (float32) flr;
}


void
vector_print(FILE * fp, vector_t v, int32 dim)
{
    int32 i;

    for (i = 0; i < dim; i++)
        fprintf(fp, " %11.4e", v[i]);
    fprintf(fp, "\n");
    fflush(fp);
}


int32
vector_is_zero(float32 * vec, int32 len)
{
    int32 i;

    for (i = 0; (i < len) && (vec[i] == 0.0); i++);
    return (i == len);          /* TRUE iff all mean values are 0.0 */
}
