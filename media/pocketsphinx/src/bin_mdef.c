/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 2005 Carnegie Mellon University.  All rights 
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
/*********************************************************************
 *
 * File: bin_mdef.c
 * 
 * Description: 
 *	Binary format model definition files, with support for
 *	heterogeneous topologies and variable-size N-phones
 *
 * Author: 
 * 	David Huggins-Daines <dhuggins@cs.cmu.edu>
 *********************************************************************/

/* System headers. */
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* SphinxBase headers. */
#include <sphinxbase/prim_type.h>
#include <sphinxbase/ckd_alloc.h>
#include <sphinxbase/byteorder.h>
#include <sphinxbase/case.h>
#include <sphinxbase/err.h>

/* Local headers. */
#include "mdef.h"
#include "bin_mdef.h"

bin_mdef_t *
bin_mdef_read_text(cmd_ln_t *config, const char *filename)
{
    bin_mdef_t *bmdef;
    mdef_t *mdef;
    int i, nodes, ci_idx, lc_idx, rc_idx;
    int nchars;

    if ((mdef = mdef_init((char *) filename, TRUE)) == NULL)
        return NULL;

    /* Enforce some limits.  */
    if (mdef->n_sen > BAD_SENID) {
        E_ERROR("Number of senones exceeds limit: %d > %d\n",
                mdef->n_sen, BAD_SENID);
        mdef_free(mdef);
        return NULL;
    }
    if (mdef->n_sseq > BAD_SSID) {
        E_ERROR("Number of senone sequences exceeds limit: %d > %d\n",
                mdef->n_sseq, BAD_SSID);
        mdef_free(mdef);
        return NULL;
    }
    /* We use uint8 for ciphones */
    if (mdef->n_ciphone > 255) {
        E_ERROR("Number of phones exceeds limit: %d > %d\n",
                mdef->n_ciphone, 255);
        mdef_free(mdef);
        return NULL;
    }

    bmdef = ckd_calloc(1, sizeof(*bmdef));
    bmdef->refcnt = 1;

    /* Easy stuff.  The mdef.c code has done the heavy lifting for us. */
    bmdef->n_ciphone = mdef->n_ciphone;
    bmdef->n_phone = mdef->n_phone;
    bmdef->n_emit_state = mdef->n_emit_state;
    bmdef->n_ci_sen = mdef->n_ci_sen;
    bmdef->n_sen = mdef->n_sen;
    bmdef->n_tmat = mdef->n_tmat;
    bmdef->n_sseq = mdef->n_sseq;
    bmdef->sseq = mdef->sseq;
    bmdef->cd2cisen = mdef->cd2cisen;
    bmdef->sen2cimap = mdef->sen2cimap;
    bmdef->n_ctx = 3;           /* Triphones only. */
    bmdef->sil = mdef->sil;
    mdef->sseq = NULL;          /* We are taking over this one. */
    mdef->cd2cisen = NULL;      /* And this one. */
    mdef->sen2cimap = NULL;     /* And this one. */

    /* Get the phone names.  If they are not sorted
     * ASCII-betically then we are in a world of hurt and
     * therefore will simply refuse to continue. */
    bmdef->ciname = ckd_calloc(bmdef->n_ciphone, sizeof(*bmdef->ciname));
    nchars = 0;
    for (i = 0; i < bmdef->n_ciphone; ++i)
        nchars += strlen(mdef->ciphone[i].name) + 1;
    bmdef->ciname[0] = ckd_calloc(nchars, 1);
    strcpy(bmdef->ciname[0], mdef->ciphone[0].name);
    for (i = 1; i < bmdef->n_ciphone; ++i) {
        bmdef->ciname[i] =
            bmdef->ciname[i - 1] + strlen(bmdef->ciname[i - 1]) + 1;
        strcpy(bmdef->ciname[i], mdef->ciphone[i].name);
        if (i > 0 && strcmp(bmdef->ciname[i - 1], bmdef->ciname[i]) > 0) {
            /* FIXME: there should be a solution to this, actually. */
            E_ERROR("Phone names are not in sorted order, sorry.");
            bin_mdef_free(bmdef);
            return NULL;
        }
    }

    /* Copy over phone information. */
    bmdef->phone = ckd_calloc(bmdef->n_phone, sizeof(*bmdef->phone));
    for (i = 0; i < mdef->n_phone; ++i) {
        bmdef->phone[i].ssid = mdef->phone[i].ssid;
        bmdef->phone[i].tmat = mdef->phone[i].tmat;
        if (i < bmdef->n_ciphone) {
            bmdef->phone[i].info.ci.filler = mdef->ciphone[i].filler;
        }
        else {
            bmdef->phone[i].info.cd.wpos = mdef->phone[i].wpos;
            bmdef->phone[i].info.cd.ctx[0] = mdef->phone[i].ci;
            bmdef->phone[i].info.cd.ctx[1] = mdef->phone[i].lc;
            bmdef->phone[i].info.cd.ctx[2] = mdef->phone[i].rc;
        }
    }

    /* Walk the wpos_ci_lclist once to find the total number of
     * nodes and the starting locations for each level. */
    nodes = lc_idx = ci_idx = rc_idx = 0;
    for (i = 0; i < N_WORD_POSN; ++i) {
        int j;
        for (j = 0; j < mdef->n_ciphone; ++j) {
            ph_lc_t *lc;

            for (lc = mdef->wpos_ci_lclist[i][j]; lc; lc = lc->next) {
                ph_rc_t *rc;
                for (rc = lc->rclist; rc; rc = rc->next) {
                    ++nodes;    /* RC node */
                }
                ++nodes;        /* LC node */
                ++rc_idx;       /* Start of RC nodes (after LC nodes) */
            }
            ++nodes;            /* CI node */
            ++lc_idx;           /* Start of LC nodes (after CI nodes) */
            ++rc_idx;           /* Start of RC nodes (after CI and LC nodes) */
        }
        ++nodes;                /* wpos node */
        ++ci_idx;               /* Start of CI nodes (after wpos nodes) */
        ++lc_idx;               /* Start of LC nodes (after CI nodes) */
        ++rc_idx;               /* STart of RC nodes (after wpos, CI, and LC nodes) */
    }
    E_INFO("Allocating %d * %d bytes (%d KiB) for CD tree\n",
           nodes, sizeof(*bmdef->cd_tree), 
           nodes * sizeof(*bmdef->cd_tree) / 1024);
    bmdef->n_cd_tree = nodes;
    bmdef->cd_tree = ckd_calloc(nodes, sizeof(*bmdef->cd_tree));
    for (i = 0; i < N_WORD_POSN; ++i) {
        int j;

        bmdef->cd_tree[i].ctx = i;
        bmdef->cd_tree[i].n_down = mdef->n_ciphone;
        bmdef->cd_tree[i].c.down = ci_idx;
#if 0
        E_INFO("%d => %c (%d@%d)\n",
               i, (WPOS_NAME)[i],
               bmdef->cd_tree[i].n_down, bmdef->cd_tree[i].c.down);
#endif

        /* Now we can build the rest of the tree. */
        for (j = 0; j < mdef->n_ciphone; ++j) {
            ph_lc_t *lc;

            bmdef->cd_tree[ci_idx].ctx = j;
            bmdef->cd_tree[ci_idx].c.down = lc_idx;
            for (lc = mdef->wpos_ci_lclist[i][j]; lc; lc = lc->next) {
                ph_rc_t *rc;

                bmdef->cd_tree[lc_idx].ctx = lc->lc;
                bmdef->cd_tree[lc_idx].c.down = rc_idx;
                for (rc = lc->rclist; rc; rc = rc->next) {
                    bmdef->cd_tree[rc_idx].ctx = rc->rc;
                    bmdef->cd_tree[rc_idx].n_down = 0;
                    bmdef->cd_tree[rc_idx].c.pid = rc->pid;
#if 0
                    E_INFO("%d => %s %s %s %c (%d@%d)\n",
                           rc_idx,
                           bmdef->ciname[j],
                           bmdef->ciname[lc->lc],
                           bmdef->ciname[rc->rc],
                           (WPOS_NAME)[i],
                           bmdef->cd_tree[rc_idx].n_down,
                           bmdef->cd_tree[rc_idx].c.down);
#endif

                    ++bmdef->cd_tree[lc_idx].n_down;
                    ++rc_idx;
                }
                /* If there are no triphones here,
                 * this is considered a leafnode, so
                 * set the pid to -1. */
                if (bmdef->cd_tree[lc_idx].n_down == 0)
                    bmdef->cd_tree[lc_idx].c.pid = -1;
#if 0
                E_INFO("%d => %s %s %c (%d@%d)\n",
                       lc_idx,
                       bmdef->ciname[j],
                       bmdef->ciname[lc->lc],
                       (WPOS_NAME)[i],
                       bmdef->cd_tree[lc_idx].n_down,
                       bmdef->cd_tree[lc_idx].c.down);
#endif

                ++bmdef->cd_tree[ci_idx].n_down;
                ++lc_idx;
            }

            /* As above, so below. */
            if (bmdef->cd_tree[ci_idx].n_down == 0)
                bmdef->cd_tree[ci_idx].c.pid = -1;
#if 0
            E_INFO("%d => %d=%s (%d@%d)\n",
                   ci_idx, j, bmdef->ciname[j],
                   bmdef->cd_tree[ci_idx].n_down,
                   bmdef->cd_tree[ci_idx].c.down);
#endif

            ++ci_idx;
        }
    }

    mdef_free(mdef);

    bmdef->alloc_mode = BIN_MDEF_FROM_TEXT;
    return bmdef;
}

bin_mdef_t *
bin_mdef_retain(bin_mdef_t *m)
{
    ++m->refcnt;
    return m;
}

int
bin_mdef_free(bin_mdef_t * m)
{
    if (m == NULL)
        return 0;
    if (--m->refcnt > 0)
        return m->refcnt;

    switch (m->alloc_mode) {
    case BIN_MDEF_FROM_TEXT:
        ckd_free(m->ciname[0]);
        ckd_free(m->sseq[0]);
        ckd_free(m->phone);
        ckd_free(m->cd_tree);
        break;
    case BIN_MDEF_IN_MEMORY:
        ckd_free(m->ciname[0]);
        break;
    case BIN_MDEF_ON_DISK:
        break;
    }
    if (m->filemap)
        mmio_file_unmap(m->filemap);
    ckd_free(m->cd2cisen);
    ckd_free(m->sen2cimap);
    ckd_free(m->ciname);
    ckd_free(m->sseq);
    ckd_free(m);
    return 0;
}

static const char format_desc[] =
    "BEGIN FILE FORMAT DESCRIPTION\n"
    "int32 n_ciphone;    /**< Number of base (CI) phones */\n"
    "int32 n_phone;	     /**< Number of base (CI) phones + (CD) triphones */\n"
    "int32 n_emit_state; /**< Number of emitting states per phone (0 if heterogeneous) */\n"
    "int32 n_ci_sen;     /**< Number of CI senones; these are the first */\n"
    "int32 n_sen;	     /**< Number of senones (CI+CD) */\n"
    "int32 n_tmat;	     /**< Number of transition matrices */\n"
    "int32 n_sseq;       /**< Number of unique senone sequences */\n"
    "int32 n_ctx;	     /**< Number of phones of context */\n"
    "int32 n_cd_tree;    /**< Number of nodes in CD tree structure */\n"
    "int32 sil;	     /**< CI phone ID for silence */\n"
    "char ciphones[][];  /**< CI phone strings (null-terminated) */\n"
    "char padding[];     /**< Padding to a 4-bytes boundary */\n"
    "struct { int16 ctx; int16 n_down; int32 pid/down } cd_tree[];\n"
    "struct { int32 ssid; int32 tmat; int8 attr[4] } phones[];\n"
    "int16 sseq[];       /**< Unique senone sequences */\n"
    "int8 sseq_len[];    /**< Number of states in each sseq (none if homogeneous) */\n"
    "END FILE FORMAT DESCRIPTION\n";

bin_mdef_t *
bin_mdef_read(cmd_ln_t *config, const char *filename)
{
    bin_mdef_t *m;
    FILE *fh;
    size_t tree_start;
    int32 val, i, do_mmap, swap;
    long pos, end;
    int32 *sseq_size;

    /* Try to read it as text first. */
    if ((m = bin_mdef_read_text(config, filename)) != NULL)
        return m;

    E_INFO("Reading binary model definition: %s\n", filename);
    if ((fh = fopen(filename, "rb")) == NULL)
        return NULL;

    if (fread(&val, 4, 1, fh) != 1) {
        fclose(fh);
        E_ERROR_SYSTEM("Failed to read byte-order marker from %s\n",
                       filename);
        return NULL;
    }
    swap = 0;
    if (val == BIN_MDEF_OTHER_ENDIAN) {
        swap = 1;
        E_INFO("Must byte-swap %s\n", filename);
    }
    if (fread(&val, 4, 1, fh) != 1) {
        fclose(fh);
        E_ERROR_SYSTEM("Failed to read version from %s\n", filename);
        return NULL;
    }
    if (swap)
        SWAP_INT32(&val);
    if (val > BIN_MDEF_FORMAT_VERSION) {
        E_ERROR("File format version %d for %s is newer than library\n",
                val, filename);
        fclose(fh);
        return NULL;
    }
    if (fread(&val, 4, 1, fh) != 1) {
        fclose(fh);
        E_ERROR_SYSTEM("Failed to read header length from %s\n", filename);
        return NULL;
    }
    if (swap)
        SWAP_INT32(&val);
    /* Skip format descriptor. */
    fseek(fh, val, SEEK_CUR);

    /* Finally allocate it. */
    m = ckd_calloc(1, sizeof(*m));
    m->refcnt = 1;

    /* Check these, to make gcc/glibc shut up. */
#define FREAD_SWAP32_CHK(dest)                                          \
    if (fread((dest), 4, 1, fh) != 1) {                                 \
        fclose(fh);                                                     \
        ckd_free(m);                                                    \
        E_ERROR_SYSTEM("Failed to read %s from %s\n", #dest, filename); \
        return NULL;                                                    \
    }                                                                   \
    if (swap) SWAP_INT32(dest);
    
    FREAD_SWAP32_CHK(&m->n_ciphone);
    FREAD_SWAP32_CHK(&m->n_phone);
    FREAD_SWAP32_CHK(&m->n_emit_state);
    FREAD_SWAP32_CHK(&m->n_ci_sen);
    FREAD_SWAP32_CHK(&m->n_sen);
    FREAD_SWAP32_CHK(&m->n_tmat);
    FREAD_SWAP32_CHK(&m->n_sseq);
    FREAD_SWAP32_CHK(&m->n_ctx);
    FREAD_SWAP32_CHK(&m->n_cd_tree);
    FREAD_SWAP32_CHK(&m->sil);

    /* CI names are first in the file. */
    m->ciname = ckd_calloc(m->n_ciphone, sizeof(*m->ciname));

    /* Decide whether to read in the whole file or mmap it. */
    do_mmap = config ? cmd_ln_boolean_r(config, "-mmap") : TRUE;
    if (swap) {
        E_WARN("-mmap specified, but mdef is other-endian.  Will not memory-map.\n");
        do_mmap = FALSE;
    } 
    /* Actually try to mmap it. */
    if (do_mmap) {
        m->filemap = mmio_file_read(filename);
        if (m->filemap == NULL)
            do_mmap = FALSE;
    }
    pos = ftell(fh);
    if (do_mmap) {
        /* Get the base pointer from the memory map. */
        m->ciname[0] = (char *)mmio_file_ptr(m->filemap) + pos;
        /* Success! */
        m->alloc_mode = BIN_MDEF_ON_DISK;
    }
    else {
        /* Read everything into memory. */
        m->alloc_mode = BIN_MDEF_IN_MEMORY;
        fseek(fh, 0, SEEK_END);
        end = ftell(fh);
        fseek(fh, pos, SEEK_SET);
        m->ciname[0] = ckd_malloc(end - pos);
        if (fread(m->ciname[0], 1, end - pos, fh) != end - pos)
            E_FATAL("Failed to read %d bytes of data from %s\n", end - pos, filename);
    }

    for (i = 1; i < m->n_ciphone; ++i)
        m->ciname[i] = m->ciname[i - 1] + strlen(m->ciname[i - 1]) + 1;

    /* Skip past the padding. */
    tree_start =
        m->ciname[i - 1] + strlen(m->ciname[i - 1]) + 1 - m->ciname[0];
    tree_start = (tree_start + 3) & ~3;
    m->cd_tree = (cd_tree_t *) (m->ciname[0] + tree_start);
    if (swap) {
        for (i = 0; i < m->n_cd_tree; ++i) {
            SWAP_INT16(&m->cd_tree[i].ctx);
            SWAP_INT16(&m->cd_tree[i].n_down);
            SWAP_INT32(&m->cd_tree[i].c.down);
        }
    }
    m->phone = (mdef_entry_t *) (m->cd_tree + m->n_cd_tree);
    if (swap) {
        for (i = 0; i < m->n_phone; ++i) {
            SWAP_INT32(&m->phone[i].ssid);
            SWAP_INT32(&m->phone[i].tmat);
        }
    }
    sseq_size = (int32 *) (m->phone + m->n_phone);
    if (swap)
        SWAP_INT32(sseq_size);
    m->sseq = ckd_calloc(m->n_sseq, sizeof(*m->sseq));
    m->sseq[0] = (uint16 *) (sseq_size + 1);
    if (swap) {
        for (i = 0; i < *sseq_size; ++i)
            SWAP_INT16(m->sseq[0] + i);
    }
    if (m->n_emit_state) {
        for (i = 1; i < m->n_sseq; ++i)
            m->sseq[i] = m->sseq[0] + i * m->n_emit_state;
    }
    else {
        m->sseq_len = (uint8 *) (m->sseq[0] + *sseq_size);
        for (i = 1; i < m->n_sseq; ++i)
            m->sseq[i] = m->sseq[i - 1] + m->sseq_len[i - 1];
    }

    /* Now build the CD-to-CI mappings using the senone sequences.
     * This is the only really accurate way to do it, though it is
     * still inaccurate in the case of heterogeneous topologies or
     * cross-state tying. */
    m->cd2cisen = (int16 *) ckd_malloc(m->n_sen * sizeof(*m->cd2cisen));
    m->sen2cimap = (int16 *) ckd_malloc(m->n_sen * sizeof(*m->sen2cimap));

    /* Default mappings (identity, none) */
    for (i = 0; i < m->n_ci_sen; ++i)
        m->cd2cisen[i] = i;
    for (; i < m->n_sen; ++i)
        m->cd2cisen[i] = -1;
    for (i = 0; i < m->n_sen; ++i)
        m->sen2cimap[i] = -1;
    for (i = 0; i < m->n_phone; ++i) {
        int32 j, ssid = m->phone[i].ssid;

        for (j = 0; j < bin_mdef_n_emit_state_phone(m, i); ++j) {
            int s = bin_mdef_sseq2sen(m, ssid, j);
            int ci = bin_mdef_pid2ci(m, i);
            /* Take the first one and warn if we have cross-state tying. */
            if (m->sen2cimap[s] == -1)
                m->sen2cimap[s] = ci;
            if (m->sen2cimap[s] != ci)
                E_WARN
                    ("Senone %d is shared between multiple base phones\n",
                     s);

            if (j > bin_mdef_n_emit_state_phone(m, ci))
                E_WARN("CD phone %d has fewer states than CI phone %d\n",
                       i, ci);
            else
                m->cd2cisen[s] =
                    bin_mdef_sseq2sen(m, m->phone[ci].ssid, j);
        }
    }

    /* Set the silence phone. */
    m->sil = bin_mdef_ciphone_id(m, S3_SILENCE_CIPHONE);

    E_INFO
        ("%d CI-phone, %d CD-phone, %d emitstate/phone, %d CI-sen, %d Sen, %d Sen-Seq\n",
         m->n_ciphone, m->n_phone - m->n_ciphone, m->n_emit_state,
         m->n_ci_sen, m->n_sen, m->n_sseq);
    fclose(fh);
    return m;
}

int
bin_mdef_write(bin_mdef_t * m, const char *filename)
{
    FILE *fh;
    int32 val, i;

    if ((fh = fopen(filename, "wb")) == NULL)
        return -1;

    /* Byteorder marker. */
    val = BIN_MDEF_NATIVE_ENDIAN;
    fwrite(&val, 1, 4, fh);
    /* Version. */
    val = BIN_MDEF_FORMAT_VERSION;
    fwrite(&val, 1, sizeof(val), fh);

    /* Round the format descriptor size up to a 4-byte boundary. */
    val = ((sizeof(format_desc) + 3) & ~3);
    fwrite(&val, 1, sizeof(val), fh);
    fwrite(format_desc, 1, sizeof(format_desc), fh);
    /* Pad it with zeros. */
    i = 0;
    fwrite(&i, 1, val - sizeof(format_desc), fh);

    /* Binary header things. */
    fwrite(&m->n_ciphone, 4, 1, fh);
    fwrite(&m->n_phone, 4, 1, fh);
    fwrite(&m->n_emit_state, 4, 1, fh);
    fwrite(&m->n_ci_sen, 4, 1, fh);
    fwrite(&m->n_sen, 4, 1, fh);
    fwrite(&m->n_tmat, 4, 1, fh);
    fwrite(&m->n_sseq, 4, 1, fh);
    fwrite(&m->n_ctx, 4, 1, fh);
    fwrite(&m->n_cd_tree, 4, 1, fh);
    /* Write this as a 32-bit value to preserve alignment for the
     * non-mmap case (we want things aligned both from the
     * beginning of the file and the beginning of the phone
     * strings). */
    val = m->sil;
    fwrite(&val, 4, 1, fh);

    /* Phone strings. */
    for (i = 0; i < m->n_ciphone; ++i)
        fwrite(m->ciname[i], 1, strlen(m->ciname[i]) + 1, fh);
    /* Pad with zeros. */
    val = (ftell(fh) + 3) & ~3;
    i = 0;
    fwrite(&i, 1, val - ftell(fh), fh);

    /* Write CD-tree */
    fwrite(m->cd_tree, sizeof(*m->cd_tree), m->n_cd_tree, fh);
    /* Write phones */
    fwrite(m->phone, sizeof(*m->phone), m->n_phone, fh);
    if (m->n_emit_state) {
        /* Write size of sseq */
        val = m->n_sseq * m->n_emit_state;
        fwrite(&val, 4, 1, fh);

        /* Write sseq */
        fwrite(m->sseq[0], sizeof(**m->sseq),
               m->n_sseq * m->n_emit_state, fh);
    }
    else {
        int32 n;

        /* Calcluate size of sseq */
        n = 0;
        for (i = 0; i < m->n_sseq; ++i)
            n += m->sseq_len[i];

        /* Write size of sseq */
        fwrite(&n, 4, 1, fh);

        /* Write sseq */
        fwrite(m->sseq[0], sizeof(**m->sseq), n, fh);

        /* Write sseq_len */
        fwrite(m->sseq_len, 1, m->n_sseq, fh);
    }
    fclose(fh);

    return 0;
}

int
bin_mdef_write_text(bin_mdef_t * m, const char *filename)
{
    FILE *fh;
    int p, i, n_total_state;

    if (strcmp(filename, "-") == 0)
        fh = stdout;
    else {
        if ((fh = fopen(filename, "w")) == NULL)
            return -1;
    }

    fprintf(fh, "0.3\n");
    fprintf(fh, "%d n_base\n", m->n_ciphone);
    fprintf(fh, "%d n_tri\n", m->n_phone - m->n_ciphone);
    if (m->n_emit_state)
        n_total_state = m->n_phone * (m->n_emit_state + 1);
    else {
        n_total_state = 0;
        for (i = 0; i < m->n_phone; ++i)
            n_total_state += m->sseq_len[m->phone[i].ssid] + 1;
    }
    fprintf(fh, "%d n_state_map\n", n_total_state);
    fprintf(fh, "%d n_tied_state\n", m->n_sen);
    fprintf(fh, "%d n_tied_ci_state\n", m->n_ci_sen);
    fprintf(fh, "%d n_tied_tmat\n", m->n_tmat);
    fprintf(fh, "#\n# Columns definitions\n");
    fprintf(fh, "#%4s %3s %3s %1s %6s %4s %s\n",
            "base", "lft", "rt", "p", "attrib", "tmat",
            "     ... state id's ...");

    for (p = 0; p < m->n_ciphone; p++) {
        int n_state;

        fprintf(fh, "%5s %3s %3s %1s", m->ciname[p], "-", "-", "-");

        if (bin_mdef_is_fillerphone(m, p))
            fprintf(fh, " %6s", "filler");
        else
            fprintf(fh, " %6s", "n/a");
        fprintf(fh, " %4d", m->phone[p].tmat);

        if (m->n_emit_state)
            n_state = m->n_emit_state;
        else
            n_state = m->sseq_len[m->phone[p].ssid];
        for (i = 0; i < n_state; i++) {
            fprintf(fh, " %6u", m->sseq[m->phone[p].ssid][i]);
        }
        fprintf(fh, " N\n");
    }


    for (; p < m->n_phone; p++) {
        int n_state;

        fprintf(fh, "%5s %3s %3s %c",
                m->ciname[m->phone[p].info.cd.ctx[0]],
                m->ciname[m->phone[p].info.cd.ctx[1]],
                m->ciname[m->phone[p].info.cd.ctx[2]],
                (WPOS_NAME)[m->phone[p].info.cd.wpos]);

        if (bin_mdef_is_fillerphone(m, p))
            fprintf(fh, " %6s", "filler");
        else
            fprintf(fh, " %6s", "n/a");
        fprintf(fh, " %4d", m->phone[p].tmat);


        if (m->n_emit_state)
            n_state = m->n_emit_state;
        else
            n_state = m->sseq_len[m->phone[p].ssid];
        for (i = 0; i < n_state; i++) {
            fprintf(fh, " %6u", m->sseq[m->phone[p].ssid][i]);
        }
        fprintf(fh, " N\n");
    }

    if (strcmp(filename, "-") != 0)
        fclose(fh);
    return 0;
}

int
bin_mdef_ciphone_id(bin_mdef_t * m, const char *ciphone)
{
    int low, mid, high;

    /* Exact binary search on m->ciphone */
    low = 0;
    high = m->n_ciphone;
    while (low < high) {
        int c;

        mid = (low + high) / 2;
        c = strcmp(ciphone, m->ciname[mid]);
        if (c == 0)
            return mid;
        else if (c > 0)
            low = mid + 1;
        else if (c < 0)
            high = mid;
    }
    return -1;
}

int
bin_mdef_ciphone_id_nocase(bin_mdef_t * m, const char *ciphone)
{
    int low, mid, high;

    /* Exact binary search on m->ciphone */
    low = 0;
    high = m->n_ciphone;
    while (low < high) {
        int c;

        mid = (low + high) / 2;
        c = strcmp_nocase(ciphone, m->ciname[mid]);
        if (c == 0)
            return mid;
        else if (c > 0)
            low = mid + 1;
        else if (c < 0)
            high = mid;
    }
    return -1;
}

const char *
bin_mdef_ciphone_str(bin_mdef_t * m, int32 ci)
{
    assert(m != NULL);
    assert(ci < m->n_ciphone);
    return m->ciname[ci];
}

int
bin_mdef_phone_id(bin_mdef_t * m, int32 ci, int32 lc, int32 rc, int32 wpos)
{
    cd_tree_t *cd_tree;
    int level, max;
    int16 ctx[4];

    assert(m);

    /* In the future, we might back off when context is not available,
     * but for now we'll just return the CI phone. */
    if (lc < 0 || rc < 0)
        return ci;

    assert((ci >= 0) && (ci < m->n_ciphone));
    assert((lc >= 0) && (lc < m->n_ciphone));
    assert((rc >= 0) && (rc < m->n_ciphone));
    assert((wpos >= 0) && (wpos < N_WORD_POSN));

    /* Create a context list, mapping fillers to silence. */
    ctx[0] = wpos;
    ctx[1] = ci;
    ctx[2] = (m->sil >= 0
              && m->phone[lc].info.ci.filler) ? m->sil : lc;
    ctx[3] = (m->sil >= 0
              && m->phone[rc].info.ci.filler) ? m->sil : rc;

    /* Walk down the cd_tree. */
    cd_tree = m->cd_tree;
    level = 0;                  /* What level we are on. */
    max = N_WORD_POSN;          /* Number of nodes on this level. */
    while (level < 4) {
        int i;

#if 0
        E_INFO("Looking for context %d=%s in %d at %d\n",
               ctx[level], m->ciname[ctx[level]],
               max, cd_tree - m->cd_tree);
#endif
        for (i = 0; i < max; ++i) {
#if 0
            E_INFO("Look at context %d=%s at %d\n",
                   cd_tree[i].ctx,
                   m->ciname[cd_tree[i].ctx], cd_tree + i - m->cd_tree);
#endif
            if (cd_tree[i].ctx == ctx[level])
                break;
        }
        if (i == max)
            return -1;
#if 0
        E_INFO("Found context %d=%s at %d, n_down=%d, down=%d\n",
               ctx[level], m->ciname[ctx[level]],
               cd_tree + i - m->cd_tree,
               cd_tree[i].n_down, cd_tree[i].c.down);
#endif
        /* Leaf node, stop here. */
        if (cd_tree[i].n_down == 0)
            return cd_tree[i].c.pid;

        /* Go down one level. */
        max = cd_tree[i].n_down;
        cd_tree = m->cd_tree + cd_tree[i].c.down;
        ++level;
    }
    /* We probably shouldn't get here. */
    return -1;
}

int
bin_mdef_phone_id_nearest(bin_mdef_t * m, int32 b, int32 l, int32 r, int32 pos)
{
    int p, tmppos;



    /* In the future, we might back off when context is not available,
     * but for now we'll just return the CI phone. */
    if (l < 0 || r < 0)
        return b;

    p = bin_mdef_phone_id(m, b, l, r, pos);
    if (p >= 0)
        return p;

    /* Exact triphone not found; backoff to other word positions */
    for (tmppos = 0; tmppos < N_WORD_POSN; tmppos++) {
        if (tmppos != pos) {
            p = bin_mdef_phone_id(m, b, l, r, tmppos);
            if (p >= 0)
                return p;
        }
    }

    /* Nothing yet; backoff to silence phone if non-silence filler context */
    /* In addition, backoff to silence phone on left/right if in beginning/end position */
    if (m->sil >= 0) {
        int newl = l, newr = r;
        if (m->phone[(int)l].info.ci.filler
            || pos == WORD_POSN_BEGIN || pos == WORD_POSN_SINGLE)
            newl = m->sil;
        if (m->phone[(int)r].info.ci.filler
            || pos == WORD_POSN_END || pos == WORD_POSN_SINGLE)
            newr = m->sil;
        if ((newl != l) || (newr != r)) {
            p = bin_mdef_phone_id(m, b, newl, newr, pos);
            if (p >= 0)
                return p;

            for (tmppos = 0; tmppos < N_WORD_POSN; tmppos++) {
                if (tmppos != pos) {
                    p = bin_mdef_phone_id(m, b, newl, newr, tmppos);
                    if (p >= 0)
                        return p;
                }
            }
        }
    }

    /* Nothing yet; backoff to base phone */
    return b;
}

int
bin_mdef_phone_str(bin_mdef_t * m, int pid, char *buf)
{
    char *wpos_name;

    assert(m);
    assert((pid >= 0) && (pid < m->n_phone));
    wpos_name = WPOS_NAME;

    buf[0] = '\0';
    if (pid < m->n_ciphone)
        sprintf(buf, "%s", bin_mdef_ciphone_str(m, pid));
    else {
        sprintf(buf, "%s %s %s %c",
                bin_mdef_ciphone_str(m, m->phone[pid].info.cd.ctx[0]),
                bin_mdef_ciphone_str(m, m->phone[pid].info.cd.ctx[1]),
                bin_mdef_ciphone_str(m, m->phone[pid].info.cd.ctx[2]),
                wpos_name[m->phone[pid].info.cd.wpos]);
    }
    return 0;
}
