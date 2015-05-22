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
 * feat.c -- Feature vector description and cepstra->feature computation.
 *
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1996 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * $Log$
 * Revision 1.22  2006/02/23  03:59:40  arthchan2003
 * Merged from branch SPHINX3_5_2_RCI_IRII_BRANCH: a, Free buffers correctly. b, Fixed dox-doc.
 * 
 * Revision 1.21.4.3  2005/10/17 04:45:57  arthchan2003
 * Free stuffs in cmn and feat corectly.
 *
 * Revision 1.21.4.2  2005/09/26 02:19:57  arthchan2003
 * Add message to show the directory which the feature is searched for.
 *
 * Revision 1.21.4.1  2005/07/03 22:55:50  arthchan2003
 * More correct deallocation in feat.c. The cmn deallocation is still not correct at this point.
 *
 * Revision 1.21  2005/06/22 03:29:35  arthchan2003
 * Makefile.am s  for all subdirectory of libs3decoder/
 *
 * Revision 1.4  2005/04/21 23:50:26  archan
 * Some more refactoring on the how reporting of structures inside kbcore_t is done, it is now 50% nice. Also added class-based LM test case into test-decode.sh.in.  At this moment, everything in search mode 5 is already done.  It is time to test the idea whether the search can really be used.
 *
 * Revision 1.3  2005/03/30 01:22:46  archan
 * Fixed mistakes in last updates. Add
 *
 * 
 * 20.Apr.2001  RAH (rhoughton@mediasite.com, ricky.houghton@cs.cmu.edu)
 *              Adding feat_free() to free allocated memory
 *
 * 02-Jan-2001	Rita Singh (rsingh@cs.cmu.edu) at Carnegie Mellon University
 *		Modified feat_s2mfc2feat_block() to handle empty buffers at
 *		the end of an utterance
 *
 * 30-Dec-2000	Rita Singh (rsingh@cs.cmu.edu) at Carnegie Mellon University
 *		Added feat_s2mfc2feat_block() to allow feature computation
 *		from sequences of blocks of cepstral vectors
 *
 * 12-Jun-98	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Major changes to accommodate arbitrary feature input types.  Added
 * 		feat_read(), moved various cep2feat functions from other files into
 *		this one.  Also, made this module object-oriented with the feat_t type.
 * 		Changed definition of s2mfc_read to let the caller manage MFC buffers.
 * 
 * 03-Oct-96	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Added unistd.h include.
 * 
 * 02-Oct-96	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Added check for sf argument to s2mfc_read being within file size.
 * 
 * 18-Sep-96	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Added sf, ef parameters to s2mfc_read().
 * 
 * 10-Jan-96	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Added feat_cepsize().
 * 		Added different feature-handling (s2_4x, s3_1x39 at this point).
 * 		Moved feature-dependent functions to feature-dependent files.
 * 
 * 09-Jan-96	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Moved constant declarations from feat.h into here.
 * 
 * 04-Nov-95	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Created.
 */


/*
 * This module encapsulates different feature streams used by the Sphinx group.  New
 * stream types can be added by augmenting feat_init() and providing an accompanying
 * compute_feat function.  It also provides a "generic" feature vector definition for
 * handling "arbitrary" speech input feature types (see the last section in feat_init()).
 * In this case the speech input data should already be feature vectors; no computation,
 * such as MFC->feature conversion, is available or needed.
 */

#include <assert.h>
#include <string.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef _MSC_VER
#pragma warning (disable: 4244 4996)
#endif

#include "sphinxbase/fe.h"
#include "sphinxbase/feat.h"
#include "sphinxbase/bio.h"
#include "sphinxbase/pio.h"
#include "sphinxbase/cmn.h"
#include "sphinxbase/agc.h"
#include "sphinxbase/err.h"
#include "sphinxbase/ckd_alloc.h"
#include "sphinxbase/prim_type.h"
#include "sphinxbase/glist.h"

#define FEAT_VERSION	"1.0"
#define FEAT_DCEP_WIN		2

#ifdef DUMP_FEATURES
static void
cep_dump_dbg(feat_t *fcb, mfcc_t **mfc, int32 nfr, const char *text)
{
    int32 i, j;

    E_INFO("%s\n", text);
    for (i = 0; i < nfr; i++) {
        for (j = 0; j < fcb->cepsize; j++) {
            fprintf(stderr, "%f ", MFCC2FLOAT(mfc[i][j]));
        }
        fprintf(stderr, "\n");
    }
}
static void
feat_print_dbg(feat_t *fcb, mfcc_t ***feat, int32 nfr, const char *text)
{
    E_INFO("%s\n", text);
    feat_print(fcb, feat, nfr, stderr);
}
#else /* !DUMP_FEATURES */
#define cep_dump_dbg(fcb,mfc,nfr,text)
#define feat_print_dbg(fcb,mfc,nfr,text)
#endif

int32 **
parse_subvecs(char const *str)
{
    char const *strp;
    int32 n, n2, l;
    glist_t dimlist;            /* List of dimensions in one subvector */
    glist_t veclist;            /* List of dimlists (subvectors) */
    int32 **subvec;
    gnode_t *gn, *gn2;

    veclist = NULL;

    strp = str;
    for (;;) {
        dimlist = NULL;

        for (;;) {
            if (sscanf(strp, "%d%n", &n, &l) != 1)
                E_FATAL("'%s': Couldn't read int32 @pos %d\n", str,
                        strp - str);
            strp += l;

            if (*strp == '-') {
                strp++;

                if (sscanf(strp, "%d%n", &n2, &l) != 1)
                    E_FATAL("'%s': Couldn't read int32 @pos %d\n", str,
                            strp - str);
                strp += l;
            }
            else
                n2 = n;

            if ((n < 0) || (n > n2))
                E_FATAL("'%s': Bad subrange spec ending @pos %d\n", str,
                        strp - str);

            for (; n <= n2; n++) {
		gnode_t *gn;
		for (gn = dimlist; gn; gn = gnode_next(gn))
		    if (gnode_int32(gn) == n)
			break;
		if (gn != NULL)
                    E_FATAL("'%s': Duplicate dimension ending @pos %d\n",
                            str, strp - str);

                dimlist = glist_add_int32(dimlist, n);
            }

            if ((*strp == '\0') || (*strp == '/'))
                break;

            if (*strp != ',')
                E_FATAL("'%s': Bad delimiter @pos %d\n", str, strp - str);

            strp++;
        }

        veclist = glist_add_ptr(veclist, (void *) dimlist);

        if (*strp == '\0')
            break;

        assert(*strp == '/');
        strp++;
    }

    /* Convert the glists to arrays; remember the glists are in reverse order of the input! */
    n = glist_count(veclist);   /* #Subvectors */
    subvec = (int32 **) ckd_calloc(n + 1, sizeof(int32 *));     /* +1 for sentinel */
    subvec[n] = NULL;           /* sentinel */

    for (--n, gn = veclist; (n >= 0) && gn; gn = gnode_next(gn), --n) {
        gn2 = (glist_t) gnode_ptr(gn);

        n2 = glist_count(gn2);  /* Length of this subvector */
        if (n2 <= 0)
            E_FATAL("'%s': 0-length subvector\n", str);

        subvec[n] = (int32 *) ckd_calloc(n2 + 1, sizeof(int32));        /* +1 for sentinel */
        subvec[n][n2] = -1;     /* sentinel */

        for (--n2; (n2 >= 0) && gn2; gn2 = gnode_next(gn2), --n2)
            subvec[n][n2] = gnode_int32(gn2);
        assert((n2 < 0) && (!gn2));
    }
    assert((n < 0) && (!gn));

    /* Free the glists */
    for (gn = veclist; gn; gn = gnode_next(gn)) {
        gn2 = (glist_t) gnode_ptr(gn);
        glist_free(gn2);
    }
    glist_free(veclist);

    return subvec;
}

void
subvecs_free(int32 **subvecs)
{
    int32 **sv;

    for (sv = subvecs; sv && *sv; ++sv)
        ckd_free(*sv);
    ckd_free(subvecs);
}

int
feat_set_subvecs(feat_t *fcb, int32 **subvecs)
{
    int32 **sv;
    uint32 n_sv, n_dim, i;

    if (subvecs == NULL) {
        subvecs_free(fcb->subvecs);
        ckd_free(fcb->sv_buf);
        ckd_free(fcb->sv_len);
        fcb->n_sv = 0;
        fcb->subvecs = NULL;
        fcb->sv_len = NULL;
        fcb->sv_buf = NULL;
        fcb->sv_dim = 0;
        return 0;
    }

    if (fcb->n_stream != 1) {
        E_ERROR("Subvector specifications require single-stream features!");
        return -1;
    }

    n_sv = 0;
    n_dim = 0;
    for (sv = subvecs; sv && *sv; ++sv) {
        int32 *d;

        for (d = *sv; d && *d != -1; ++d) {
            ++n_dim;
        }
        ++n_sv;
    }
    if (n_dim > feat_dimension(fcb)) {
        E_ERROR("Total dimensionality of subvector specification %d "
                "> feature dimensionality %d\n", n_dim, feat_dimension(fcb));
        return -1;
    }

    fcb->n_sv = n_sv;
    fcb->subvecs = subvecs;
    fcb->sv_len = (uint32 *)ckd_calloc(n_sv, sizeof(*fcb->sv_len));
    fcb->sv_buf = (mfcc_t *)ckd_calloc(n_dim, sizeof(*fcb->sv_buf));
    fcb->sv_dim = n_dim;
    for (i = 0; i < n_sv; ++i) {
        int32 *d;
        for (d = subvecs[i]; d && *d != -1; ++d) {
            ++fcb->sv_len[i];
        }
    }

    return 0;
}

/**
 * Project feature components to subvectors (if any).
 */
static void
feat_subvec_project(feat_t *fcb, mfcc_t ***inout_feat, uint32 nfr)
{
    uint32 i;

    if (fcb->subvecs == NULL)
        return;
    for (i = 0; i < nfr; ++i) {
        mfcc_t *out;
        int32 j;

        out = fcb->sv_buf;
        for (j = 0; j < fcb->n_sv; ++j) {
            int32 *d;
            for (d = fcb->subvecs[j]; d && *d != -1; ++d) {
                *out++ = inout_feat[i][0][*d];
            }
        }
        memcpy(inout_feat[i][0], fcb->sv_buf, fcb->sv_dim * sizeof(*fcb->sv_buf));
    }
}

mfcc_t ***
feat_array_alloc(feat_t * fcb, int32 nfr)
{
    int32 i, j, k;
    mfcc_t *data, *d, ***feat;

    assert(fcb);
    assert(nfr > 0);
    assert(feat_dimension(fcb) > 0);

    /* Make sure to use the dimensionality of the features *before*
       LDA and subvector projection. */
    k = 0;
    for (i = 0; i < fcb->n_stream; ++i)
        k += fcb->stream_len[i];
    assert(k >= feat_dimension(fcb));
    assert(k >= fcb->sv_dim);

    feat =
        (mfcc_t ***) ckd_calloc_2d(nfr, feat_dimension1(fcb), sizeof(mfcc_t *));
    data = (mfcc_t *) ckd_calloc(nfr * k, sizeof(mfcc_t));

    for (i = 0; i < nfr; i++) {
        d = data + i * k;
        for (j = 0; j < feat_dimension1(fcb); j++) {
            feat[i][j] = d;
            d += feat_dimension2(fcb, j);
        }
    }

    return feat;
}

mfcc_t ***
feat_array_realloc(feat_t *fcb, mfcc_t ***old_feat, int32 ofr, int32 nfr)
{
    int32 i, k, cf;
    mfcc_t*** new_feat;

    assert(fcb);
    assert(nfr > 0);
    assert(ofr > 0);
    assert(feat_dimension(fcb) > 0);

    /* Make sure to use the dimensionality of the features *before*
       LDA and subvector projection. */
    k = 0;
    for (i = 0; i < fcb->n_stream; ++i)
        k += fcb->stream_len[i];
    assert(k >= feat_dimension(fcb));
    assert(k >= fcb->sv_dim);
    
    new_feat = feat_array_alloc(fcb, nfr);

    cf = (nfr < ofr) ? nfr : ofr;
    memcpy(new_feat[0][0], old_feat[0][0], cf * k * sizeof(mfcc_t));

    feat_array_free(old_feat);
    
    return new_feat;
}

void
feat_array_free(mfcc_t ***feat)
{
    ckd_free(feat[0][0]);
    ckd_free_2d((void **)feat);
}

static void
feat_s2_4x_cep2feat(feat_t * fcb, mfcc_t ** mfc, mfcc_t ** feat)
{
    mfcc_t *f;
    mfcc_t *w, *_w;
    mfcc_t *w1, *w_1, *_w1, *_w_1;
    mfcc_t d1, d2;
    int32 i, j;

    assert(fcb);
    assert(feat_cepsize(fcb) == 13);
    assert(feat_n_stream(fcb) == 4);
    assert(feat_stream_len(fcb, 0) == 12);
    assert(feat_stream_len(fcb, 1) == 24);
    assert(feat_stream_len(fcb, 2) == 3);
    assert(feat_stream_len(fcb, 3) == 12);
    assert(feat_window_size(fcb) == 4);

    /* CEP; skip C0 */
    memcpy(feat[0], mfc[0] + 1, (feat_cepsize(fcb) - 1) * sizeof(mfcc_t));

    /*
     * DCEP(SHORT): mfc[2] - mfc[-2]
     * DCEP(LONG):  mfc[4] - mfc[-4]
     */
    w = mfc[2] + 1;             /* +1 to skip C0 */
    _w = mfc[-2] + 1;

    f = feat[1];
    for (i = 0; i < feat_cepsize(fcb) - 1; i++) /* Short-term */
        f[i] = w[i] - _w[i];

    w = mfc[4] + 1;             /* +1 to skip C0 */
    _w = mfc[-4] + 1;

    for (j = 0; j < feat_cepsize(fcb) - 1; i++, j++)    /* Long-term */
        f[i] = w[j] - _w[j];

    /* D2CEP: (mfc[3] - mfc[-1]) - (mfc[1] - mfc[-3]) */
    w1 = mfc[3] + 1;            /* Final +1 to skip C0 */
    _w1 = mfc[-1] + 1;
    w_1 = mfc[1] + 1;
    _w_1 = mfc[-3] + 1;

    f = feat[3];
    for (i = 0; i < feat_cepsize(fcb) - 1; i++) {
        d1 = w1[i] - _w1[i];
        d2 = w_1[i] - _w_1[i];

        f[i] = d1 - d2;
    }

    /* POW: C0, DC0, D2C0; differences computed as above for rest of cep */
    f = feat[2];
    f[0] = mfc[0][0];
    f[1] = mfc[2][0] - mfc[-2][0];

    d1 = mfc[3][0] - mfc[-1][0];
    d2 = mfc[1][0] - mfc[-3][0];
    f[2] = d1 - d2;
}


static void
feat_s3_1x39_cep2feat(feat_t * fcb, mfcc_t ** mfc, mfcc_t ** feat)
{
    mfcc_t *f;
    mfcc_t *w, *_w;
    mfcc_t *w1, *w_1, *_w1, *_w_1;
    mfcc_t d1, d2;
    int32 i;

    assert(fcb);
    assert(feat_cepsize(fcb) == 13);
    assert(feat_n_stream(fcb) == 1);
    assert(feat_stream_len(fcb, 0) == 39);
    assert(feat_window_size(fcb) == 3);

    /* CEP; skip C0 */
    memcpy(feat[0], mfc[0] + 1, (feat_cepsize(fcb) - 1) * sizeof(mfcc_t));
    /*
     * DCEP: mfc[2] - mfc[-2];
     */
    f = feat[0] + feat_cepsize(fcb) - 1;
    w = mfc[2] + 1;             /* +1 to skip C0 */
    _w = mfc[-2] + 1;

    for (i = 0; i < feat_cepsize(fcb) - 1; i++)
        f[i] = w[i] - _w[i];

    /* POW: C0, DC0, D2C0 */
    f += feat_cepsize(fcb) - 1;

    f[0] = mfc[0][0];
    f[1] = mfc[2][0] - mfc[-2][0];

    d1 = mfc[3][0] - mfc[-1][0];
    d2 = mfc[1][0] - mfc[-3][0];
    f[2] = d1 - d2;

    /* D2CEP: (mfc[3] - mfc[-1]) - (mfc[1] - mfc[-3]) */
    f += 3;

    w1 = mfc[3] + 1;            /* Final +1 to skip C0 */
    _w1 = mfc[-1] + 1;
    w_1 = mfc[1] + 1;
    _w_1 = mfc[-3] + 1;

    for (i = 0; i < feat_cepsize(fcb) - 1; i++) {
        d1 = w1[i] - _w1[i];
        d2 = w_1[i] - _w_1[i];

        f[i] = d1 - d2;
    }
}


static void
feat_s3_cep(feat_t * fcb, mfcc_t ** mfc, mfcc_t ** feat)
{
    assert(fcb);
    assert(feat_n_stream(fcb) == 1);
    assert(feat_window_size(fcb) == 0);

    /* CEP */
    memcpy(feat[0], mfc[0], feat_cepsize(fcb) * sizeof(mfcc_t));
}

static void
feat_s3_cep_dcep(feat_t * fcb, mfcc_t ** mfc, mfcc_t ** feat)
{
    mfcc_t *f;
    mfcc_t *w, *_w;
    int32 i;

    assert(fcb);
    assert(feat_n_stream(fcb) == 1);
    assert(feat_stream_len(fcb, 0) == feat_cepsize(fcb) * 2);
    assert(feat_window_size(fcb) == 2);

    /* CEP */
    memcpy(feat[0], mfc[0], feat_cepsize(fcb) * sizeof(mfcc_t));

    /*
     * DCEP: mfc[2] - mfc[-2];
     */
    f = feat[0] + feat_cepsize(fcb);
    w = mfc[2];
    _w = mfc[-2];

    for (i = 0; i < feat_cepsize(fcb); i++)
        f[i] = w[i] - _w[i];
}

static void
feat_1s_c_d_dd_cep2feat(feat_t * fcb, mfcc_t ** mfc, mfcc_t ** feat)
{
    mfcc_t *f;
    mfcc_t *w, *_w;
    mfcc_t *w1, *w_1, *_w1, *_w_1;
    mfcc_t d1, d2;
    int32 i;

    assert(fcb);
    assert(feat_n_stream(fcb) == 1);
    assert(feat_stream_len(fcb, 0) == feat_cepsize(fcb) * 3);
    assert(feat_window_size(fcb) == FEAT_DCEP_WIN + 1);

    /* CEP */
    memcpy(feat[0], mfc[0], feat_cepsize(fcb) * sizeof(mfcc_t));

    /*
     * DCEP: mfc[w] - mfc[-w], where w = FEAT_DCEP_WIN;
     */
    f = feat[0] + feat_cepsize(fcb);
    w = mfc[FEAT_DCEP_WIN];
    _w = mfc[-FEAT_DCEP_WIN];

    for (i = 0; i < feat_cepsize(fcb); i++)
        f[i] = w[i] - _w[i];

    /* 
     * D2CEP: (mfc[w+1] - mfc[-w+1]) - (mfc[w-1] - mfc[-w-1]), 
     * where w = FEAT_DCEP_WIN 
     */
    f += feat_cepsize(fcb);

    w1 = mfc[FEAT_DCEP_WIN + 1];
    _w1 = mfc[-FEAT_DCEP_WIN + 1];
    w_1 = mfc[FEAT_DCEP_WIN - 1];
    _w_1 = mfc[-FEAT_DCEP_WIN - 1];

    for (i = 0; i < feat_cepsize(fcb); i++) {
        d1 = w1[i] - _w1[i];
        d2 = w_1[i] - _w_1[i];

        f[i] = d1 - d2;
    }
}

static void
feat_1s_c_d_ld_dd_cep2feat(feat_t * fcb, mfcc_t ** mfc, mfcc_t ** feat)
{
    mfcc_t *f;
    mfcc_t *w, *_w;
    mfcc_t *w1, *w_1, *_w1, *_w_1;
    mfcc_t d1, d2;
    int32 i;

    assert(fcb);
    assert(feat_n_stream(fcb) == 1);
    assert(feat_stream_len(fcb, 0) == feat_cepsize(fcb) * 4);
    assert(feat_window_size(fcb) == FEAT_DCEP_WIN * 2);

    /* CEP */
    memcpy(feat[0], mfc[0], feat_cepsize(fcb) * sizeof(mfcc_t));

    /*
     * DCEP: mfc[w] - mfc[-w], where w = FEAT_DCEP_WIN;
     */
    f = feat[0] + feat_cepsize(fcb);
    w = mfc[FEAT_DCEP_WIN];
    _w = mfc[-FEAT_DCEP_WIN];

    for (i = 0; i < feat_cepsize(fcb); i++)
        f[i] = w[i] - _w[i];

    /*
     * LDCEP: mfc[w] - mfc[-w], where w = FEAT_DCEP_WIN * 2;
     */
    f += feat_cepsize(fcb);
    w = mfc[FEAT_DCEP_WIN * 2];
    _w = mfc[-FEAT_DCEP_WIN * 2];

    for (i = 0; i < feat_cepsize(fcb); i++)
        f[i] = w[i] - _w[i];

    /* 
     * D2CEP: (mfc[w+1] - mfc[-w+1]) - (mfc[w-1] - mfc[-w-1]), 
     * where w = FEAT_DCEP_WIN 
     */
    f += feat_cepsize(fcb);

    w1 = mfc[FEAT_DCEP_WIN + 1];
    _w1 = mfc[-FEAT_DCEP_WIN + 1];
    w_1 = mfc[FEAT_DCEP_WIN - 1];
    _w_1 = mfc[-FEAT_DCEP_WIN - 1];

    for (i = 0; i < feat_cepsize(fcb); i++) {
        d1 = w1[i] - _w1[i];
        d2 = w_1[i] - _w_1[i];

        f[i] = d1 - d2;
    }
}

static void
feat_copy(feat_t * fcb, mfcc_t ** mfc, mfcc_t ** feat)
{
    int32 win, i, j;

    win = feat_window_size(fcb);

    /* Concatenate input features */
    for (i = -win; i <= win; ++i) {
        uint32 spos = 0;

        for (j = 0; j < feat_n_stream(fcb); ++j) {
            uint32 stream_len;

            /* Unscale the stream length by the window. */
            stream_len = feat_stream_len(fcb, j) / (2 * win + 1);
            memcpy(feat[j] + ((i + win) * stream_len),
                   mfc[i] + spos,
                   stream_len * sizeof(mfcc_t));
            spos += stream_len;
        }
    }
}

feat_t *
feat_init(char const *type, cmn_type_t cmn, int32 varnorm,
          agc_type_t agc, int32 breport, int32 cepsize)
{
    feat_t *fcb;

    if (cepsize == 0)
        cepsize = 13;
    if (breport)
        E_INFO
            ("Initializing feature stream to type: '%s', ceplen=%d, CMN='%s', VARNORM='%s', AGC='%s'\n",
             type, cepsize, cmn_type_str[cmn], varnorm ? "yes" : "no", agc_type_str[agc]);

    fcb = (feat_t *) ckd_calloc(1, sizeof(feat_t));
    fcb->refcount = 1;
    fcb->name = (char *) ckd_salloc(type);
    if (strcmp(type, "s2_4x") == 0) {
        /* Sphinx-II format 4-stream feature (Hack!! hardwired constants below) */
        if (cepsize != 13) {
            E_ERROR("s2_4x features require cepsize == 13\n");
            ckd_free(fcb);
            return NULL;
        }
        fcb->cepsize = 13;
        fcb->n_stream = 4;
        fcb->stream_len = (uint32 *) ckd_calloc(4, sizeof(uint32));
        fcb->stream_len[0] = 12;
        fcb->stream_len[1] = 24;
        fcb->stream_len[2] = 3;
        fcb->stream_len[3] = 12;
        fcb->out_dim = 51;
        fcb->window_size = 4;
        fcb->compute_feat = feat_s2_4x_cep2feat;
    }
    else if ((strcmp(type, "s3_1x39") == 0) || (strcmp(type, "1s_12c_12d_3p_12dd") == 0)) {
        /* 1-stream cep/dcep/pow/ddcep (Hack!! hardwired constants below) */
        if (cepsize != 13) {
            E_ERROR("s2_4x features require cepsize == 13\n");
            ckd_free(fcb);
            return NULL;
        }
        fcb->cepsize = 13;
        fcb->n_stream = 1;
        fcb->stream_len = (uint32 *) ckd_calloc(1, sizeof(uint32));
        fcb->stream_len[0] = 39;
        fcb->out_dim = 39;
        fcb->window_size = 3;
        fcb->compute_feat = feat_s3_1x39_cep2feat;
    }
    else if (strncmp(type, "1s_c_d_dd", 9) == 0) {
        fcb->cepsize = cepsize;
        fcb->n_stream = 1;
        fcb->stream_len = (uint32 *) ckd_calloc(1, sizeof(uint32));
        fcb->stream_len[0] = cepsize * 3;
        fcb->out_dim = cepsize * 3;
        fcb->window_size = FEAT_DCEP_WIN + 1; /* ddcep needs the extra 1 */
        fcb->compute_feat = feat_1s_c_d_dd_cep2feat;
    }
    else if (strncmp(type, "1s_c_d_ld_dd", 12) == 0) {
        fcb->cepsize = cepsize;
        fcb->n_stream = 1;
        fcb->stream_len = (uint32 *) ckd_calloc(1, sizeof(uint32));
        fcb->stream_len[0] = cepsize * 4;
        fcb->out_dim = cepsize * 4;
        fcb->window_size = FEAT_DCEP_WIN * 2;
        fcb->compute_feat = feat_1s_c_d_ld_dd_cep2feat;
    }
    else if (strncmp(type, "cep_dcep", 8) == 0 || strncmp(type, "1s_c_d", 6) == 0) {
        /* 1-stream cep/dcep */
        fcb->cepsize = cepsize;
        fcb->n_stream = 1;
        fcb->stream_len = (uint32 *) ckd_calloc(1, sizeof(uint32));
        fcb->stream_len[0] = feat_cepsize(fcb) * 2;
        fcb->out_dim = fcb->stream_len[0];
        fcb->window_size = 2;
        fcb->compute_feat = feat_s3_cep_dcep;
    }
    else if (strncmp(type, "cep", 3) == 0 || strncmp(type, "1s_c", 4) == 0) {
        /* 1-stream cep */
        fcb->cepsize = cepsize;
        fcb->n_stream = 1;
        fcb->stream_len = (uint32 *) ckd_calloc(1, sizeof(uint32));
        fcb->stream_len[0] = feat_cepsize(fcb);
        fcb->out_dim = fcb->stream_len[0];
        fcb->window_size = 0;
        fcb->compute_feat = feat_s3_cep;
    }
    else if (strncmp(type, "1s_3c", 5) == 0 || strncmp(type, "1s_4c", 5) == 0) {
	/* 1-stream cep with frames concatenated, so called cepwin features */
        if (strncmp(type, "1s_3c", 5) == 0)
            fcb->window_size = 3;
        else
    	    fcb->window_size = 4;

        fcb->cepsize = cepsize;
        fcb->n_stream = 1;
        fcb->stream_len = (uint32 *) ckd_calloc(1, sizeof(uint32));
        fcb->stream_len[0] = feat_cepsize(fcb) * (2 * fcb->window_size + 1);
        fcb->out_dim = fcb->stream_len[0];
        fcb->compute_feat = feat_copy;
    }
    else {
        int32 i, k, l;
        size_t len;
        char *strp;
        char *mtype = ckd_salloc(type);
        char *wd = ckd_salloc(type);
        /*
         * Generic definition: Format should be %d,%d,%d,...,%d (i.e.,
         * comma separated list of feature stream widths; #items =
         * #streams).  An optional window size (frames will be
         * concatenated) is also allowed, which can be specified with
         * a colon after the list of feature streams.
         */
        len = strlen(mtype);
        k = 0;
        for (i = 1; i < len - 1; i++) {
            if (mtype[i] == ',') {
                mtype[i] = ' ';
                k++;
            }
            else if (mtype[i] == ':') {
                mtype[i] = '\0';
                fcb->window_size = atoi(mtype + i + 1);
                break;
            }
        }
        k++;                    /* Presumably there are (#commas+1) streams */
        fcb->n_stream = k;
        fcb->stream_len = (uint32 *) ckd_calloc(k, sizeof(uint32));

        /* Scan individual feature stream lengths */
        strp = mtype;
        i = 0;
        fcb->out_dim = 0;
        fcb->cepsize = 0;
        while (sscanf(strp, "%s%n", wd, &l) == 1) {
            strp += l;
            if ((i >= fcb->n_stream)
                || (sscanf(wd, "%u", &(fcb->stream_len[i])) != 1)
                || (fcb->stream_len[i] <= 0))
                E_FATAL("Bad feature type argument\n");
            /* Input size before windowing */
            fcb->cepsize += fcb->stream_len[i];
            if (fcb->window_size > 0)
                fcb->stream_len[i] *= (fcb->window_size * 2 + 1);
            /* Output size after windowing */
            fcb->out_dim += fcb->stream_len[i];
            i++;
        }
        if (i != fcb->n_stream)
            E_FATAL("Bad feature type argument\n");
        if (fcb->cepsize != cepsize)
    	    E_FATAL("Bad feature type argument\n");

        /* Input is already the feature stream */
        fcb->compute_feat = feat_copy;
        ckd_free(mtype);
        ckd_free(wd);
    }

    if (cmn != CMN_NONE)
        fcb->cmn_struct = cmn_init(feat_cepsize(fcb));
    fcb->cmn = cmn;
    fcb->varnorm = varnorm;
    if (agc != AGC_NONE) {
        fcb->agc_struct = agc_init();
        /*
         * No need to check if agc is set to EMAX; agc_emax_set() changes only emax related things
         * Moreover, if agc is not NONE and block mode is used, feat_agc() SILENTLY
         * switches to EMAX
         */
        /* HACK: hardwired initial estimates based on use of CMN (from Sphinx2) */
        agc_emax_set(fcb->agc_struct, (cmn != CMN_NONE) ? 5.0 : 10.0);
    }
    fcb->agc = agc;
    /*
     * Make sure this buffer is large enough to be used in feat_s2mfc2feat_block_utt()
     */
    fcb->cepbuf = (mfcc_t **) ckd_calloc_2d((LIVEBUFBLOCKSIZE < feat_window_size(fcb) * 2) ? feat_window_size(fcb) * 2 : LIVEBUFBLOCKSIZE,
                                            feat_cepsize(fcb),
                                            sizeof(mfcc_t));
    /* This one is actually just an array of pointers to "flatten out"
     * wraparounds. */
    fcb->tmpcepbuf = (mfcc_t** )ckd_calloc(2 * feat_window_size(fcb) + 1,
                                sizeof(*fcb->tmpcepbuf));

    return fcb;
}


void
feat_print(feat_t * fcb, mfcc_t *** feat, int32 nfr, FILE * fp)
{
    uint32 i, j, k;

    for (i = 0; i < nfr; i++) {
        fprintf(fp, "%8d:\n", i);

        for (j = 0; j < feat_dimension1(fcb); j++) {
            fprintf(fp, "\t%2d:", j);

            for (k = 0; k < feat_dimension2(fcb, j); k++)
                fprintf(fp, " %8.4f", MFCC2FLOAT(feat[i][j][k]));
            fprintf(fp, "\n");
        }
    }

    fflush(fp);
}

static void
feat_cmn(feat_t *fcb, mfcc_t **mfc, int32 nfr, int32 beginutt, int32 endutt)
{
    cmn_type_t cmn_type = fcb->cmn;

    if (!(beginutt && endutt)
        && cmn_type != CMN_NONE) /* Only cmn_prior in block computation mode. */
        fcb->cmn = cmn_type = CMN_PRIOR;

    switch (cmn_type) {
    case CMN_CURRENT:
        cmn(fcb->cmn_struct, mfc, fcb->varnorm, nfr);
        break;
    case CMN_PRIOR:
        cmn_prior(fcb->cmn_struct, mfc, fcb->varnorm, nfr);
        if (endutt)
            cmn_prior_update(fcb->cmn_struct);
        break;
    default:
        ;
    }
    cep_dump_dbg(fcb, mfc, nfr, "After CMN");
}

static void
feat_agc(feat_t *fcb, mfcc_t **mfc, int32 nfr, int32 beginutt, int32 endutt)
{
    agc_type_t agc_type = fcb->agc;

    if (!(beginutt && endutt)
        && agc_type != AGC_NONE) /* Only agc_emax in block computation mode. */
        agc_type = AGC_EMAX;

    switch (agc_type) {
    case AGC_MAX:
        agc_max(fcb->agc_struct, mfc, nfr);
        break;
    case AGC_EMAX:
        agc_emax(fcb->agc_struct, mfc, nfr);
        if (endutt)
            agc_emax_update(fcb->agc_struct);
        break;
    case AGC_NOISE:
        agc_noise(fcb->agc_struct, mfc, nfr);
        break;
    default:
        ;
    }
    cep_dump_dbg(fcb, mfc, nfr, "After AGC");
}

static void
feat_compute_utt(feat_t *fcb, mfcc_t **mfc, int32 nfr, int32 win, mfcc_t ***feat)
{
    int32 i;

    cep_dump_dbg(fcb, mfc, nfr, "Incoming features (after padding)");

    /* Create feature vectors */
    for (i = win; i < nfr - win; i++) {
        fcb->compute_feat(fcb, mfc + i, feat[i - win]);
    }

    feat_print_dbg(fcb, feat, nfr - win * 2, "After dynamic feature computation");

    if (fcb->lda) {
        feat_lda_transform(fcb, feat, nfr - win * 2);
        feat_print_dbg(fcb, feat, nfr - win * 2, "After LDA");
    }

    if (fcb->subvecs) {
        feat_subvec_project(fcb, feat, nfr - win * 2);
        feat_print_dbg(fcb, feat, nfr - win * 2, "After subvector projection");
    }
}


/**
 * Read Sphinx-II format mfc file (s2mfc = Sphinx-II format MFC data).
 * If out_mfc is NULL, no actual reading will be done, and the number of 
 * frames (plus padding) that would be read is returned.
 * 
 * It's important that normalization is done before padding because
 * frames outside the data we are interested in shouldn't be taken
 * into normalization stats.
 *
 * @return # frames read (plus padding) if successful, -1 if
 * error (e.g., mfc array too small).  
 */
static int32
feat_s2mfc_read_norm_pad(feat_t *fcb, char *file, int32 win,
            		 int32 sf, int32 ef,
            		 mfcc_t ***out_mfc,
            		 int32 maxfr,
            		 int32 cepsize)
{
    FILE *fp;
    int32 n_float32;
    float32 *float_feat;
    struct stat statbuf;
    int32 i, n, byterev;
    int32 start_pad, end_pad;
    mfcc_t **mfc;

    /* Initialize the output pointer to NULL, so that any attempts to
       free() it if we fail before allocating it will not segfault! */
    if (out_mfc)
        *out_mfc = NULL;
    E_INFO("Reading mfc file: '%s'[%d..%d]\n", file, sf, ef);
    if (ef >= 0 && ef <= sf) {
        E_ERROR("%s: End frame (%d) <= Start frame (%d)\n", file, ef, sf);
        return -1;
    }

    /* Find filesize; HACK!! To get around intermittent NFS failures, use stat_retry */
    if ((stat_retry(file, &statbuf) < 0)
        || ((fp = fopen(file, "rb")) == NULL)) {
        E_ERROR_SYSTEM("Failed to open file '%s' for reading", file);
        return -1;
    }

    /* Read #floats in header */
    if (fread_retry(&n_float32, sizeof(int32), 1, fp) != 1) {
        E_ERROR("%s: fread(#floats) failed\n", file);
        fclose(fp);
        return -1;
    }

    /* Check if n_float32 matches file size */
    byterev = 0;
    if ((int32) (n_float32 * sizeof(float32) + 4) != (int32) statbuf.st_size) { /* RAH, typecast both sides to remove compile warning */
        n = n_float32;
        SWAP_INT32(&n);

        if ((int32) (n * sizeof(float32) + 4) != (int32) (statbuf.st_size)) {   /* RAH, typecast both sides to remove compile warning */
            E_ERROR
                ("%s: Header size field: %d(%08x); filesize: %d(%08x)\n",
                 file, n_float32, n_float32, statbuf.st_size,
                 statbuf.st_size);
            fclose(fp);
            return -1;
        }

        n_float32 = n;
        byterev = 1;
    }
    if (n_float32 <= 0) {
        E_ERROR("%s: Header size field (#floats) = %d\n", file, n_float32);
        fclose(fp);
        return -1;
    }

    /* Convert n to #frames of input */
    n = n_float32 / cepsize;
    if (n * cepsize != n_float32) {
        E_ERROR("Header size field: %d; not multiple of %d\n", n_float32,
                cepsize);
        fclose(fp);
        return -1;
    }

    /* Check start and end frames */
    if (sf > 0) {
        if (sf >= n) {
            E_ERROR("%s: Start frame (%d) beyond file size (%d)\n", file,
                    sf, n);
            fclose(fp);
            return -1;
        }
    }
    if (ef < 0)
        ef = n-1;
    else if (ef >= n) {
        E_WARN("%s: End frame (%d) beyond file size (%d), will truncate\n",
               file, ef, n);
        ef = n-1;
    }

    /* Add window to start and end frames */
    sf -= win;
    ef += win;
    if (sf < 0) {
        start_pad = -sf;
        sf = 0;
    }
    else
        start_pad = 0;
    if (ef >= n) {
        end_pad = ef - n + 1;
        ef = n - 1;
    }
    else
        end_pad = 0;

    /* Limit n if indicated by [sf..ef] */
    if ((ef - sf + 1) < n)
        n = (ef - sf + 1);
    if (maxfr > 0 && n + start_pad + end_pad > maxfr) {
        E_ERROR("%s: Maximum output size(%d frames) < actual #frames(%d)\n",
                file, maxfr, n + start_pad + end_pad);
        fclose(fp);
        return -1;
    }

    /* If no output buffer was supplied, then skip the actual data reading. */
    if (out_mfc != NULL) {
        /* Position at desired start frame and read actual MFC data */
        mfc = (mfcc_t **)ckd_calloc_2d(n + start_pad + end_pad, cepsize, sizeof(mfcc_t));
        if (sf > 0)
            fseek(fp, sf * cepsize * sizeof(float32), SEEK_CUR);
        n_float32 = n * cepsize;
#ifdef FIXED_POINT
        float_feat = ckd_calloc(n_float32, sizeof(float32));
#else
        float_feat = mfc[start_pad];
#endif
        if (fread_retry(float_feat, sizeof(float32), n_float32, fp) != n_float32) {
            E_ERROR("%s: fread(%dx%d) (MFC data) failed\n", file, n, cepsize);
            ckd_free_2d(mfc);
            fclose(fp);
            return -1;
        }
        if (byterev) {
            for (i = 0; i < n_float32; i++) {
                SWAP_FLOAT32(&float_feat[i]);
            }
        }
#ifdef FIXED_POINT
        for (i = 0; i < n_float32; ++i) {
            mfc[start_pad][i] = FLOAT2MFCC(float_feat[i]);
        }
        ckd_free(float_feat);
#endif

        /* Normalize */
        feat_cmn(fcb, mfc + start_pad, n, 1, 1);
        feat_agc(fcb, mfc + start_pad, n, 1, 1);

        /* Replicate start and end frames if necessary. */
        for (i = 0; i < start_pad; ++i)
            memcpy(mfc[i], mfc[start_pad], cepsize * sizeof(mfcc_t));
        for (i = 0; i < end_pad; ++i)
            memcpy(mfc[start_pad + n + i], mfc[start_pad + n - 1],
                   cepsize * sizeof(mfcc_t));

        *out_mfc = mfc;
    }

    fclose(fp);
    return n + start_pad + end_pad;
}



int32
feat_s2mfc2feat(feat_t * fcb, const char *file, const char *dir, const char *cepext,
                int32 sf, int32 ef, mfcc_t *** feat, int32 maxfr)
{
    char *path;
    char *ps = "/";
    int32 win, nfr;
    size_t file_length, cepext_length, path_length = 0;
    mfcc_t **mfc;

    if (fcb->cepsize <= 0) {
        E_ERROR("Bad cepsize: %d\n", fcb->cepsize);
        return -1;
    }

    if (cepext == NULL)
        cepext = "";

    /*
     * Create mfc filename, combining file, dir and extension if
     * necessary
     */

    /*
     * First we decide about the path. If dir is defined, then use
     * it. Otherwise assume the filename already contains the path.
     */
    if (dir == NULL) {
        dir = "";
        ps = "";
        /*
         * This is not true but some 3rd party apps
         * may parse the output explicitly checking for this line
         */
        E_INFO("At directory . (current directory)\n");
    }
    else {
        E_INFO("At directory %s\n", dir);
        /*
         * Do not forget the path separator!
         */
        path_length += strlen(dir) + 1;
    }

    /*
     * Include cepext, if it's not already part of the filename.
     */
    file_length = strlen(file);
    cepext_length = strlen(cepext);
    if ((file_length > cepext_length)
        && (strcmp(file + file_length - cepext_length, cepext) == 0)) {
        cepext = "";
        cepext_length = 0;
    }

    /*
     * Do not forget the '\0'
     */
    path_length += file_length + cepext_length + 1;
    path = (char*) ckd_calloc(path_length, sizeof(char));

#ifdef HAVE_SNPRINTF
    /*
     * Paranoia is our best friend...
     */
    while ((file_length = snprintf(path, path_length, "%s%s%s%s", dir, ps, file, cepext)) > path_length) {
        path_length = file_length;
        path = (char*) ckd_realloc(path, path_length * sizeof(char));
    }
#else
    sprintf(path, "%s%s%s%s", dir, ps, file, cepext);
#endif

    win = feat_window_size(fcb);
    /* Pad maxfr with win, so we read enough raw feature data to
     * calculate the requisite number of dynamic features. */
    if (maxfr >= 0)
        maxfr += win * 2;

    if (feat != NULL) {
        /* Read mfc file including window or padding if necessary. */
        nfr = feat_s2mfc_read_norm_pad(fcb, path, win, sf, ef, &mfc, maxfr, fcb->cepsize);
        ckd_free(path);
        if (nfr < 0) {
            ckd_free_2d((void **) mfc);
            return -1;
        }

        /* Actually compute the features */
        feat_compute_utt(fcb, mfc, nfr, win, feat);
        
        ckd_free_2d((void **) mfc);
    }
    else {
        /* Just calculate the number of frames we would need. */
        nfr = feat_s2mfc_read_norm_pad(fcb, path, win, sf, ef, NULL, maxfr, fcb->cepsize);
        ckd_free(path);
        if (nfr < 0)
            return nfr;
    }


    return (nfr - win * 2);
}

static int32
feat_s2mfc2feat_block_utt(feat_t * fcb, mfcc_t ** uttcep,
			  int32 nfr, mfcc_t *** ofeat)
{
    mfcc_t **cepbuf;
    int32 i, win, cepsize;

    win = feat_window_size(fcb);
    cepsize = feat_cepsize(fcb);

    /* Copy and pad out the utterance (this requires that the
     * feature computation functions always access the buffer via
     * the frame pointers, which they do)  */
    cepbuf = (mfcc_t **)ckd_calloc(nfr + win * 2, sizeof(mfcc_t *));
    memcpy(cepbuf + win, uttcep, nfr * sizeof(mfcc_t *));

    /* Do normalization before we interpolate on the boundary */    
    feat_cmn(fcb, cepbuf + win, nfr, 1, 1);
    feat_agc(fcb, cepbuf + win, nfr, 1, 1);

    /* Now interpolate */    
    for (i = 0; i < win; ++i) {
        cepbuf[i] = fcb->cepbuf[i];
        memcpy(cepbuf[i], uttcep[0], cepsize * sizeof(mfcc_t));
        cepbuf[nfr + win + i] = fcb->cepbuf[win + i];
        memcpy(cepbuf[nfr + win + i], uttcep[nfr - 1], cepsize * sizeof(mfcc_t));
    }
    /* Compute as usual. */
    feat_compute_utt(fcb, cepbuf, nfr + win * 2, win, ofeat);
    ckd_free(cepbuf);
    return nfr;
}

int32
feat_s2mfc2feat_live(feat_t * fcb, mfcc_t ** uttcep, int32 *inout_ncep,
		     int32 beginutt, int32 endutt, mfcc_t *** ofeat)
{
    int32 win, cepsize, nbufcep;
    int32 i, j, nfeatvec;
    int32 zero = 0;

    /* Avoid having to check this everywhere. */
    if (inout_ncep == NULL) inout_ncep = &zero;

    /* Special case for entire utterances. */
    if (beginutt && endutt && *inout_ncep > 0)
        return feat_s2mfc2feat_block_utt(fcb, uttcep, *inout_ncep, ofeat);

    win = feat_window_size(fcb);
    cepsize = feat_cepsize(fcb);

    /* Empty the input buffer on start of utterance. */
    if (beginutt)
        fcb->bufpos = fcb->curpos;

    /* Calculate how much data is in the buffer already. */
    nbufcep = fcb->bufpos - fcb->curpos;
    if (nbufcep < 0)
	nbufcep = fcb->bufpos + LIVEBUFBLOCKSIZE - fcb->curpos;
    /* Add any data that we have to replicate. */
    if (beginutt && *inout_ncep > 0)
        nbufcep += win;
    if (endutt)
        nbufcep += win;

    /* Only consume as much input as will fit in the buffer. */
    if (nbufcep + *inout_ncep > LIVEBUFBLOCKSIZE) {
        /* We also can't overwrite the trailing window, hence the
         * reason why win is subtracted here. */
        *inout_ncep = LIVEBUFBLOCKSIZE - nbufcep - win;
        /* Cancel end of utterance processing. */
        endutt = FALSE;
    }

    /* FIXME: Don't modify the input! */
    feat_cmn(fcb, uttcep, *inout_ncep, beginutt, endutt);
    feat_agc(fcb, uttcep, *inout_ncep, beginutt, endutt);

    /* Replicate first frame into the first win frames if we're at the
     * beginning of the utterance and there was some actual input to
     * deal with.  (FIXME: Not entirely sure why that condition) */
    if (beginutt && *inout_ncep > 0) {
        for (i = 0; i < win; i++) {
            memcpy(fcb->cepbuf[fcb->bufpos++], uttcep[0],
                   cepsize * sizeof(mfcc_t));
            fcb->bufpos %= LIVEBUFBLOCKSIZE;
        }
        /* Move the current pointer past this data. */
        fcb->curpos = fcb->bufpos;
        nbufcep -= win;
    }

    /* Copy in frame data to the circular buffer. */
    for (i = 0; i < *inout_ncep; ++i) {
        memcpy(fcb->cepbuf[fcb->bufpos++], uttcep[i],
               cepsize * sizeof(mfcc_t));
        fcb->bufpos %= LIVEBUFBLOCKSIZE;
	++nbufcep;
    }

    /* Replicate last frame into the last win frames if we're at the
     * end of the utterance (even if there was no input, so we can
     * flush the output). */
    if (endutt) {
        int32 tpos; /* Index of last input frame. */
        if (fcb->bufpos == 0)
            tpos = LIVEBUFBLOCKSIZE - 1;
        else
            tpos = fcb->bufpos - 1;
        for (i = 0; i < win; ++i) {
            memcpy(fcb->cepbuf[fcb->bufpos++], fcb->cepbuf[tpos],
                   cepsize * sizeof(mfcc_t));
            fcb->bufpos %= LIVEBUFBLOCKSIZE;
        }
    }

    /* We have to leave the trailing window of frames. */
    nfeatvec = nbufcep - win;
    if (nfeatvec <= 0)
        return 0; /* Do nothing. */

    for (i = 0; i < nfeatvec; ++i) {
        /* Handle wraparound cases. */
        if (fcb->curpos - win < 0 || fcb->curpos + win >= LIVEBUFBLOCKSIZE) {
            /* Use tmpcepbuf for this case.  Actually, we just need the pointers. */
            for (j = -win; j <= win; ++j) {
                int32 tmppos =
                    (fcb->curpos + j + LIVEBUFBLOCKSIZE) % LIVEBUFBLOCKSIZE;
		fcb->tmpcepbuf[win + j] = fcb->cepbuf[tmppos];
            }
            fcb->compute_feat(fcb, fcb->tmpcepbuf + win, ofeat[i]);
        }
        else {
            fcb->compute_feat(fcb, fcb->cepbuf + fcb->curpos, ofeat[i]);
        }
	/* Move the read pointer forward. */
        ++fcb->curpos;
        fcb->curpos %= LIVEBUFBLOCKSIZE;
    }

    if (fcb->lda)
        feat_lda_transform(fcb, ofeat, nfeatvec);

    if (fcb->subvecs)
        feat_subvec_project(fcb, ofeat, nfeatvec);

    return nfeatvec;
}

void 
feat_update_stats(feat_t *fcb)
{
    if (fcb->cmn == CMN_PRIOR) {
        cmn_prior_update(fcb->cmn_struct);
    }
    if (fcb->agc == AGC_EMAX || fcb->agc == AGC_MAX) {
	agc_emax_update(fcb->agc_struct);	
    }
}

feat_t *
feat_retain(feat_t *f)
{
    ++f->refcount;
    return f;
}

int
feat_free(feat_t * f)
{
    if (f == NULL)
        return 0;
    if (--f->refcount > 0)
        return f->refcount;

    if (f->cepbuf)
        ckd_free_2d((void **) f->cepbuf);
    ckd_free(f->tmpcepbuf);

    if (f->name) {
        ckd_free((void *) f->name);
    }
    if (f->lda)
        ckd_free_3d((void ***) f->lda);

    ckd_free(f->stream_len);
    ckd_free(f->sv_len);
    ckd_free(f->sv_buf);
    subvecs_free(f->subvecs);

    cmn_free(f->cmn_struct);
    agc_free(f->agc_struct);

    ckd_free(f);
    return 0;
}


void
feat_report(feat_t * f)
{
    int i;
    E_INFO_NOFN("Initialization of feat_t, report:\n");
    E_INFO_NOFN("Feature type         = %s\n", f->name);
    E_INFO_NOFN("Cepstral size        = %d\n", f->cepsize);
    E_INFO_NOFN("Number of streams    = %d\n", f->n_stream);
    for (i = 0; i < f->n_stream; i++) {
        E_INFO_NOFN("Vector size of stream[%d]: %d\n", i,
                    f->stream_len[i]);
    }
    E_INFO_NOFN("Number of subvectors = %d\n", f->n_sv);
    for (i = 0; i < f->n_sv; i++) {
        int32 *sv;

        E_INFO_NOFN("Components of subvector[%d]:", i);
        for (sv = f->subvecs[i]; sv && *sv != -1; ++sv)
            E_INFOCONT(" %d", *sv);
        E_INFOCONT("\n");
    }
    E_INFO_NOFN("Whether CMN is used  = %d\n", f->cmn);
    E_INFO_NOFN("Whether AGC is used  = %d\n", f->agc);
    E_INFO_NOFN("Whether variance is normalized = %d\n", f->varnorm);
    E_INFO_NOFN("\n");
}
