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
 * tmat.c
 *
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1997 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * $Log: tmat.c,v $
 * Revision 1.1.1.1  2006/05/23 18:45:01  dhuggins
 * re-importation
 *
 * Revision 1.4  2005/11/14 16:14:34  dhuggins
 * Use LOG() instead of logs3() for loading tmats, makes startup
 * ***much*** faster.
 *
 * Revision 1.3  2005/10/10 14:50:35  dhuggins
 * Deal properly with empty transition matrices.
 *
 * Revision 1.2  2005/09/30 15:01:23  dhuggins
 * More robust tmat reading - read the tmat in accordance with the fixed s2 topology
 *
 * Revision 1.1  2005/09/29 21:51:19  dhuggins
 * Add support for Sphinx3 tmat files.  Amazingly enough, it Just Works
 * (but it isn't terribly robust)
 *
 * Revision 1.6  2005/07/05 13:12:39  dhdfu
 * Add new arguments to logs3_init() in some tests, main_ep
 *
 * Revision 1.5  2005/06/21 19:23:35  arthchan2003
 * 1, Fixed doxygen documentation. 2, Added $ keyword.
 *
 * Revision 1.5  2005/05/03 04:09:09  archan
 * Implemented the heart of word copy search. For every ci-phone, every word end, a tree will be allocated to preserve its pathscore.  This is different from 3.5 or below, only the best score for a particular ci-phone, regardless of the word-ends will be preserved at every frame.  The graph propagation will not collect unused word tree at this point. srch_WST_propagate_wd_lv2 is also as the most stupid in the century.  But well, after all, everything needs a start.  I will then really get the results from the search and see how it looks.
 *
 * Revision 1.4  2005/04/21 23:50:26  archan
 * Some more refactoring on the how reporting of structures inside kbcore_t is done, it is now 50% nice. Also added class-based LM test case into test-decode.sh.in.  At this moment, everything in search mode 5 is already done.  It is time to test the idea whether the search can really be used.
 *
 * Revision 1.3  2005/03/30 01:22:47  archan
 * Fixed mistakes in last updates. Add
 *
 * 
 * 20.Apr.2001  RAH (rhoughton@mediasite.com, ricky.houghton@cs.cmu.edu)
 *              Added tmat_free to free allocated memory 
 *
 * 29-Feb-2000	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University.
 * 		Added tmat_chk_1skip(), and made tmat_chk_uppertri() public.
 * 
 * 10-Dec-1999	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University.
 * 		Made tmat_dump() public.
 * 
 * 11-Mar-97	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University.
 * 		Started based on original S3 implementation.
 */

/* System headers. */
#include <string.h>

/* SphinxBase headers. */
#include <sphinxbase/logmath.h>
#include <sphinxbase/err.h>
#include <sphinxbase/ckd_alloc.h>
#include <sphinxbase/bio.h>

/* Local headers. */
#include "tmat.h"
#include "hmm.h"
#include "vector.h"

#define TMAT_PARAM_VERSION		"1.0"


/**
 * Checks that no transition matrix in the given object contains backward arcs.
 * @returns 0 if successful, -1 if check failed.
 */
static int32 tmat_chk_uppertri(tmat_t *tmat, logmath_t *lmath);


/**
 * Checks that transition matrix arcs in the given object skip over
 * at most 1 state.  
 * @returns 0 if successful, -1 if check failed.  
 */

static int32 tmat_chk_1skip(tmat_t *tmat, logmath_t *lmath);


void
tmat_dump(tmat_t * tmat, FILE * fp)
{
    int32 i, src, dst;

    for (i = 0; i < tmat->n_tmat; i++) {
        fprintf(fp, "TMAT %d = %d x %d\n", i, tmat->n_state,
                tmat->n_state + 1);
        for (src = 0; src < tmat->n_state; src++) {
            for (dst = 0; dst <= tmat->n_state; dst++)
                fprintf(fp, " %12d", tmat->tp[i][src][dst]);
            fprintf(fp, "\n");
        }
        fprintf(fp, "\n");
    }
    fflush(fp);
}


/*
 * Check model tprob matrices that they conform to upper-triangular assumption;
 * i.e. no "backward" transitions allowed.
 */
int32
tmat_chk_uppertri(tmat_t * tmat, logmath_t *lmath)
{
    int32 i, src, dst;

    /* Check that each tmat is upper-triangular */
    for (i = 0; i < tmat->n_tmat; i++) {
        for (dst = 0; dst < tmat->n_state; dst++)
            for (src = dst + 1; src < tmat->n_state; src++)
                if (tmat->tp[i][src][dst] < 255) {
                    E_ERROR("tmat[%d][%d][%d] = %d\n",
                            i, src, dst, tmat->tp[i][src][dst]);
                    return -1;
                }
    }

    return 0;
}


int32
tmat_chk_1skip(tmat_t * tmat, logmath_t *lmath)
{
    int32 i, src, dst;

    for (i = 0; i < tmat->n_tmat; i++) {
        for (src = 0; src < tmat->n_state; src++)
            for (dst = src + 3; dst <= tmat->n_state; dst++)
                if (tmat->tp[i][src][dst] < 255) {
                    E_ERROR("tmat[%d][%d][%d] = %d\n",
                            i, src, dst, tmat->tp[i][src][dst]);
                    return -1;
                }
    }

    return 0;
}


tmat_t *
tmat_init(char const *file_name, logmath_t *lmath, float64 tpfloor, int32 breport)
{
    char tmp;
    int32 n_src, n_dst, n_tmat;
    FILE *fp;
    int32 byteswap, chksum_present;
    uint32 chksum;
    float32 **tp;
    int32 i, j, k, tp_per_tmat;
    char **argname, **argval;
    tmat_t *t;


    if (breport) {
        E_INFO("Reading HMM transition probability matrices: %s\n",
               file_name);
    }

    t = (tmat_t *) ckd_calloc(1, sizeof(tmat_t));

    if ((fp = fopen(file_name, "rb")) == NULL)
        E_FATAL_SYSTEM("Failed to open transition file '%s' for reading", file_name);

    /* Read header, including argument-value info and 32-bit byteorder magic */
    if (bio_readhdr(fp, &argname, &argval, &byteswap) < 0)
        E_FATAL("Failed to read header from file '%s'\n", file_name);

    /* Parse argument-value list */
    chksum_present = 0;
    for (i = 0; argname[i]; i++) {
        if (strcmp(argname[i], "version") == 0) {
            if (strcmp(argval[i], TMAT_PARAM_VERSION) != 0)
                E_WARN("Version mismatch(%s): %s, expecting %s\n",
                       file_name, argval[i], TMAT_PARAM_VERSION);
        }
        else if (strcmp(argname[i], "chksum0") == 0) {
            chksum_present = 1; /* Ignore the associated value */
        }
    }
    bio_hdrarg_free(argname, argval);
    argname = argval = NULL;

    chksum = 0;

    /* Read #tmat, #from-states, #to-states, arraysize */
    if ((bio_fread(&n_tmat, sizeof(int32), 1, fp, byteswap, &chksum)
         != 1)
        || (bio_fread(&n_src, sizeof(int32), 1, fp, byteswap, &chksum) !=
            1)
        || (bio_fread(&n_dst, sizeof(int32), 1, fp, byteswap, &chksum) !=
            1)
        || (bio_fread(&i, sizeof(int32), 1, fp, byteswap, &chksum) != 1)) {
        E_FATAL("Failed to read header from '%s'\n", file_name);
    }
    if (n_tmat >= MAX_INT16)
        E_FATAL("%s: Number of transition matrices (%d) exceeds limit (%d)\n", file_name,
                n_tmat, MAX_INT16);
    t->n_tmat = n_tmat;
    
    if (n_dst != n_src + 1)
        E_FATAL("%s: Unsupported transition matrix. Number of source states (%d) != number of target states (%d)-1\n", file_name,
                n_src, n_dst);
    t->n_state = n_src;

    if (i != t->n_tmat * n_src * n_dst) {
        E_FATAL
            ("%s: Invalid transitions. Number of coefficients (%d) doesn't match expected array dimension: %d x %d x %d\n",
             file_name, i, t->n_tmat, n_src, n_dst);
    }

    /* Allocate memory for tmat data */
    t->tp = ckd_calloc_3d(t->n_tmat, n_src, n_dst, sizeof(***t->tp));

    /* Temporary structure to read in the float data */
    tp = ckd_calloc_2d(n_src, n_dst, sizeof(**tp));

    /* Read transition matrices, normalize and floor them, and convert to log domain */
    tp_per_tmat = n_src * n_dst;
    for (i = 0; i < t->n_tmat; i++) {
        if (bio_fread(tp[0], sizeof(float32), tp_per_tmat, fp,
                      byteswap, &chksum) != tp_per_tmat) {
            E_FATAL("Failed to read transition matrix %d from '%s'\n", i, file_name);
        }

        /* Normalize and floor */
        for (j = 0; j < n_src; j++) {
            if (vector_sum_norm(tp[j], n_dst) == 0.0)
                E_WARN("Normalization failed for transition matrix %d from state %d\n",
                       i, j);
            vector_nz_floor(tp[j], n_dst, tpfloor);
            vector_sum_norm(tp[j], n_dst);

            /* Convert to logs3. */
            for (k = 0; k < n_dst; k++) {
                int ltp;
#if 0 /* No, don't do this!  It will subtly break 3-state HMMs. */
                /* For these ones, we floor them even if they are
                 * zero, otherwise HMM evaluation goes nuts. */
                if (k >= j && k-j < 3 && tp[j][k] == 0.0f)
                    tp[j][k] = tpfloor;
#endif
                /* Log and quantize them. */
                ltp = -logmath_log(lmath, tp[j][k]) >> SENSCR_SHIFT;
                if (ltp > 255) ltp = 255;
                t->tp[i][j][k] = (uint8)ltp;
            }
        }
    }

    ckd_free_2d(tp);

    if (chksum_present)
        bio_verify_chksum(fp, byteswap, chksum);

    if (fread(&tmp, 1, 1, fp) == 1)
        E_ERROR("Non-empty file beyond end of data\n");

    fclose(fp);

    if (tmat_chk_uppertri(t, lmath) < 0)
        E_FATAL("Tmat not upper triangular\n");
    if (tmat_chk_1skip(t, lmath) < 0)
        E_FATAL("Topology not Left-to-Right or Bakis\n");

    return t;
}

void
tmat_report(tmat_t * t)
{
    E_INFO_NOFN("Initialization of tmat_t, report:\n");
    E_INFO_NOFN("Read %d transition matrices of size %dx%d\n",
                t->n_tmat, t->n_state, t->n_state + 1);
    E_INFO_NOFN("\n");

}

/* 
 *  RAH, Free memory allocated in tmat_init ()
 */
void
tmat_free(tmat_t * t)
{
    if (t) {
        if (t->tp)
            ckd_free_3d(t->tp);
        ckd_free(t);
    }
}
