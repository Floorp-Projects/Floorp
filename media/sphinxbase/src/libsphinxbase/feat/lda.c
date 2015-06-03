/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 2006 Carnegie Mellon University.  All rights
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
 * lda.c -- Read and apply LDA matrices to features.
 *
 * Author: David Huggins-Daines <dhuggins@cs.cmu.edu>
 */

#include <assert.h>
#include <string.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef _MSC_VER
#pragma warning (disable: 4018)
#endif

#include "sphinxbase/feat.h"
#include "sphinxbase/ckd_alloc.h"
#include "sphinxbase/bio.h"
#include "sphinxbase/err.h"

#define MATRIX_FILE_VERSION "0.1"

int32
feat_read_lda(feat_t *feat, const char *ldafile, int32 dim)
{
    FILE *fh;
    int32 byteswap;
    uint32 chksum, i, m, n;
    char **argname, **argval;

    assert(feat);
    if (feat->n_stream != 1) {
        E_ERROR("LDA incompatible with multi-stream features (n_stream = %d)\n",
                feat->n_stream);
        return -1;
    }

    if ((fh = fopen(ldafile, "rb")) == NULL) {
        E_ERROR_SYSTEM("Failed to open transform file '%s' for reading", ldafile);
        return -1;
    }

    if (bio_readhdr(fh, &argname, &argval, &byteswap) < 0) {
        E_ERROR("Failed to read header from transform file '%s'\n", ldafile);
        fclose(fh);
        return -1;
    }

    for (i = 0; argname[i]; i++) {
        if (strcmp(argname[i], "version") == 0) {
            if (strcmp(argval[i], MATRIX_FILE_VERSION) != 0)
                E_WARN("%s: Version mismatch: %s, expecting %s\n",
                       ldafile, argval[i], MATRIX_FILE_VERSION);
        }
    }

    bio_hdrarg_free(argname, argval);
    argname = argval = NULL;

    chksum = 0;

    if (feat->lda)
        ckd_free_3d((void ***)feat->lda);

    {
        /* Use a temporary variable to avoid strict-aliasing problems. */
        void ***outlda;

        if (bio_fread_3d(&outlda, sizeof(float32),
                         &feat->n_lda, &m, &n,
                         fh, byteswap, &chksum) < 0) {
            E_ERROR_SYSTEM("%s: bio_fread_3d(lda) failed\n", ldafile);
            fclose(fh);
            return -1;
        }
        feat->lda = (void *)outlda;
    }
    fclose(fh);
    
#ifdef FIXED_POINT
    /* FIXME: This is a fragile hack that depends on mfcc_t and
     * float32 being the same size (which they are, but...) */
    for (i = 0; i < feat->n_lda * m * n; ++i) {
        feat->lda[0][0][i] = FLOAT2MFCC(((float *)feat->lda[0][0])[i]);
    }
#endif

    /* Note that SphinxTrain stores the eigenvectors as row vectors. */
    if (n != feat->stream_len[0])
	E_FATAL("LDA matrix dimension %d doesn't match feature stream size %d\n", n, feat->stream_len[0]);
    
    /* Override dim from file if it is 0 or greater than m. */
    if (dim > m || dim <= 0) {
        dim = m;
    }
    feat->out_dim = dim;

    return 0;
}

void
feat_lda_transform(feat_t *fcb, mfcc_t ***inout_feat, uint32 nfr)
{
    mfcc_t *tmp;
    uint32 i, j, k;

    tmp = ckd_calloc(fcb->stream_len[0], sizeof(mfcc_t));
    for (i = 0; i < nfr; ++i) {
        /* Do the matrix multiplication inline here since fcb->lda
         * is transposed (eigenvectors in rows not columns). */
        /* FIXME: In the future we ought to use the BLAS. */
        memset(tmp, 0, sizeof(mfcc_t) * fcb->stream_len[0]);
        for (j = 0; j < feat_dimension(fcb); ++j) {
            for (k = 0; k < fcb->stream_len[0]; ++k) {
                tmp[j] += MFCCMUL(inout_feat[i][0][k], fcb->lda[0][j][k]);
            }
        }
        memcpy(inout_feat[i][0], tmp, fcb->stream_len[0] * sizeof(mfcc_t));
    }
    ckd_free(tmp);
}
