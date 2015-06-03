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

#include <string.h>

#include "sphinxbase/huff_code.h"
#include "sphinxbase/ckd_alloc.h"
#include "sphinxbase/hash_table.h"
#include "sphinxbase/byteorder.h"
#include "sphinxbase/heap.h"
#include "sphinxbase/pio.h"
#include "sphinxbase/err.h"

typedef struct huff_node_s {
    int nbits;
    struct huff_node_s *l;
    union {
        int32 ival;
        char *sval;
        struct huff_node_s *r;
    } r;
} huff_node_t;

typedef struct huff_codeword_s {
    union {
        int32 ival;
        char *sval;
    } r;
    uint32 nbits, codeword;
} huff_codeword_t;

enum {
    HUFF_CODE_INT,
    HUFF_CODE_STR
};

struct huff_code_s {
    int16 refcount;
    uint8 maxbits;
    uint8 type;
    uint32 *firstcode;
    uint32 *numl;
    huff_codeword_t **syms;
    hash_table_t *codewords;
    FILE *fh;
    bit_encode_t *be;
    int boff;
};

static huff_node_t *
huff_node_new_int(int32 val)
{
    huff_node_t *hn = ckd_calloc(1, sizeof(*hn));
    hn->r.ival = val;
    return hn;
}

static huff_node_t *
huff_node_new_str(char const *val)
{
    huff_node_t *hn = ckd_calloc(1, sizeof(*hn));
    hn->r.sval = ckd_salloc(val);
    return hn;
}

static huff_node_t *
huff_node_new_parent(huff_node_t *l, huff_node_t *r)
{
    huff_node_t *hn = ckd_calloc(1, sizeof(*hn));
    hn->l = l;
    hn->r.r = r;
    /* Propagate maximum bit length. */
    if (r->nbits > l->nbits)
        hn->nbits = r->nbits + 1;
    else
        hn->nbits = l->nbits + 1;
    return hn;
}

static void
huff_node_free_int(huff_node_t *root)
{
    if (root->l) {
        huff_node_free_int(root->l);
        huff_node_free_int(root->r.r);
    }
    ckd_free(root);
}

static void
huff_node_free_str(huff_node_t *root, int freestr)
{
    if (root->l) {
        huff_node_free_str(root->l, freestr);
        huff_node_free_str(root->r.r, freestr);
    }
    else {
        if (freestr)
            ckd_free(root->r.sval);
    }
    ckd_free(root);
}

static huff_node_t *
huff_code_build_tree(heap_t *q)
{
    huff_node_t *root = NULL;
    int32 rf;

    while (heap_size(q) > 1) {
        huff_node_t *l, *r, *p;
        int32 lf, rf;

        heap_pop(q, (void *)&l, &lf);
        heap_pop(q, (void *)&r, &rf);
        p = huff_node_new_parent(l, r);
        heap_insert(q, p, lf + rf);
    }
    heap_pop(q, (void **)&root, &rf);
    return root;
}

static void
huff_code_canonicalize(huff_code_t *hc, huff_node_t *root)
{
    glist_t agenda;
    uint32 *nextcode;
    int i, ncw;

    hc->firstcode = ckd_calloc(hc->maxbits+1, sizeof(*hc->firstcode));
    hc->syms = ckd_calloc(hc->maxbits+1, sizeof(*hc->syms));
    hc->numl = ckd_calloc(hc->maxbits+1, sizeof(*nextcode));
    nextcode = ckd_calloc(hc->maxbits+1, sizeof(*nextcode));

    /* Traverse the tree, annotating it with the actual bit
     * lengths, and histogramming them in numl. */
    root->nbits = 0;
    ncw = 0;
    agenda = glist_add_ptr(NULL, root);
    while (agenda) {
        huff_node_t *node = gnode_ptr(agenda);
        agenda = gnode_free(agenda, NULL);
        if (node->l) {
            node->l->nbits = node->nbits + 1;
            agenda = glist_add_ptr(agenda, node->l);
            node->r.r->nbits = node->nbits + 1;
            agenda = glist_add_ptr(agenda, node->r.r);
        }
        else {
            hc->numl[node->nbits]++;
            ncw++;
        }
    }
    /* Create starting codes and symbol tables for each bit length. */
    hc->syms[hc->maxbits] = ckd_calloc(hc->numl[hc->maxbits], sizeof(**hc->syms));
    for (i = hc->maxbits - 1; i > 0; --i) {
        hc->firstcode[i] = (hc->firstcode[i+1] + hc->numl[i+1]) / 2;
        hc->syms[i] = ckd_calloc(hc->numl[i], sizeof(**hc->syms));
    }
    memcpy(nextcode, hc->firstcode, (hc->maxbits + 1) * sizeof(*nextcode));
    /* Traverse the tree again to produce the codebook itself. */
    hc->codewords = hash_table_new(ncw, HASH_CASE_YES);
    agenda = glist_add_ptr(NULL, root);
    while (agenda) {
        huff_node_t *node = gnode_ptr(agenda);
        agenda = gnode_free(agenda, NULL);
        if (node->l) {
            agenda = glist_add_ptr(agenda, node->l);
            agenda = glist_add_ptr(agenda, node->r.r);
        }
        else {
            /* Initialize codebook entry, which also retains symbol pointer. */
            huff_codeword_t *cw;
            uint32 codeword = nextcode[node->nbits] & ((1 << node->nbits) - 1);
            cw = hc->syms[node->nbits] + (codeword - hc->firstcode[node->nbits]);
            cw->nbits = node->nbits;
            cw->r.sval = node->r.sval; /* Will copy ints too... */
            cw->codeword = codeword;
            if (hc->type == HUFF_CODE_INT) {
                hash_table_enter_bkey(hc->codewords,
                                      (char const *)&cw->r.ival,
                                      sizeof(cw->r.ival),
                                      (void *)cw);
            }
            else {
                hash_table_enter(hc->codewords, cw->r.sval, (void *)cw);
            }
            ++nextcode[node->nbits];
        }
    }
    ckd_free(nextcode);
}

huff_code_t *
huff_code_build_int(int32 const *values, int32 const *frequencies, int nvals)
{
    huff_code_t *hc;
    huff_node_t *root;
    heap_t *q;
    int i;

    hc = ckd_calloc(1, sizeof(*hc));
    hc->refcount = 1;
    hc->type = HUFF_CODE_INT;

    /* Initialize the heap with nodes for each symbol. */
    q = heap_new();
    for (i = 0; i < nvals; ++i) {
        heap_insert(q,
                    huff_node_new_int(values[i]),
                    frequencies[i]);
    }

    /* Now build the tree, which gives us codeword lengths. */
    root = huff_code_build_tree(q);
    heap_destroy(q);
    if (root == NULL || root->nbits > 32) {
        E_ERROR("Huffman trees currently limited to 32 bits\n");
        huff_node_free_int(root);
        huff_code_free(hc);
        return NULL;
    }

    /* Build a canonical codebook. */
    hc->maxbits = root->nbits;
    huff_code_canonicalize(hc, root);

    /* Tree no longer needed. */
    huff_node_free_int(root);

    return hc;
}

huff_code_t *
huff_code_build_str(char * const *values, int32 const *frequencies, int nvals)
{
    huff_code_t *hc;
    huff_node_t *root;
    heap_t *q;
    int i;

    hc = ckd_calloc(1, sizeof(*hc));
    hc->refcount = 1;
    hc->type = HUFF_CODE_STR;

    /* Initialize the heap with nodes for each symbol. */
    q = heap_new();
    for (i = 0; i < nvals; ++i) {
        heap_insert(q,
                    huff_node_new_str(values[i]),
                    frequencies[i]);
    }

    /* Now build the tree, which gives us codeword lengths. */
    root = huff_code_build_tree(q);
    heap_destroy(q);
    if (root == NULL || root->nbits > 32) {
        E_ERROR("Huffman trees currently limited to 32 bits\n");
        huff_node_free_str(root, TRUE);
        huff_code_free(hc);
        return NULL;
    }

    /* Build a canonical codebook. */
    hc->maxbits = root->nbits;
    huff_code_canonicalize(hc, root);

    /* Tree no longer needed (note we retain pointers to its strings). */
    huff_node_free_str(root, FALSE);

    return hc;
}

huff_code_t *
huff_code_read(FILE *infh)
{
    huff_code_t *hc;
    int i, j;

    hc = ckd_calloc(1, sizeof(*hc));
    hc->refcount = 1;

    hc->maxbits = fgetc(infh);
    hc->type = fgetc(infh);

    /* Two bytes of padding. */
    fgetc(infh);
    fgetc(infh);

    /* Allocate stuff. */
    hc->firstcode = ckd_calloc(hc->maxbits + 1, sizeof(*hc->firstcode));
    hc->numl = ckd_calloc(hc->maxbits + 1, sizeof(*hc->numl));
    hc->syms = ckd_calloc(hc->maxbits + 1, sizeof(*hc->syms));

    /* Read the symbol tables. */
    hc->codewords = hash_table_new(hc->maxbits, HASH_CASE_YES);
    for (i = 1; i <= hc->maxbits; ++i) {
        if (fread(&hc->firstcode[i], 4, 1, infh) != 1)
            goto error_out;
        SWAP_BE_32(&hc->firstcode[i]);
        if (fread(&hc->numl[i], 4, 1, infh) != 1)
            goto error_out;
        SWAP_BE_32(&hc->numl[i]);
        hc->syms[i] = ckd_calloc(hc->numl[i], sizeof(**hc->syms));
        for (j = 0; j < hc->numl[i]; ++j) {
            huff_codeword_t *cw = &hc->syms[i][j];
            cw->nbits = i;
            cw->codeword = hc->firstcode[i] + j;
            if (hc->type == HUFF_CODE_INT) {
                if (fread(&cw->r.ival, 4, 1, infh) != 1)
                    goto error_out;
                SWAP_BE_32(&cw->r.ival);
                hash_table_enter_bkey(hc->codewords,
                                      (char const *)&cw->r.ival,
                                      sizeof(cw->r.ival),
                                      (void *)cw);
            }
            else {
                size_t len;
                cw->r.sval = fread_line(infh, &len);
                cw->r.sval[len-1] = '\0';
                hash_table_enter(hc->codewords, cw->r.sval, (void *)cw);
            }
        }
    }

    return hc;
error_out:
    huff_code_free(hc);
    return NULL;
}

int
huff_code_write(huff_code_t *hc, FILE *outfh)
{
    int i, j;

    /* Maximum codeword length */
    fputc(hc->maxbits, outfh);
    /* Symbol type */
    fputc(hc->type, outfh);
    /* Two extra bytes (for future use and alignment) */
    fputc(0, outfh);
    fputc(0, outfh);
    /* For each codeword length: */
    for (i = 1; i <= hc->maxbits; ++i) {
        uint32 val;

        /* Starting code, number of codes. */
        val = hc->firstcode[i];
        /* Canonically big-endian (like the data itself) */
        SWAP_BE_32(&val);
        fwrite(&val, 4, 1, outfh);
        val = hc->numl[i];
        SWAP_BE_32(&val);
        fwrite(&val, 4, 1, outfh);

        /* Symbols for each code (FIXME: Should compress these too) */
        for (j = 0; j < hc->numl[i]; ++j) {
            if (hc->type == HUFF_CODE_INT) {
                int32 val = hc->syms[i][j].r.ival;
                SWAP_BE_32(&val);
                fwrite(&val, 4, 1, outfh);
            }
            else {
                /* Write them all separated by newlines, so that
                 * fgets() will read them for us. */
                fprintf(outfh, "%s\n", hc->syms[i][j].r.sval);
            }
        }
    }
    return 0;
}

int
huff_code_dump_codebits(FILE *dumpfh, uint32 nbits, uint32 codeword)
{
    uint32 i;

    for (i = 0; i < nbits; ++i)
        fputc((codeword & (1<<(nbits-i-1))) ? '1' : '0', dumpfh);
    return 0;
}

int
huff_code_dump(huff_code_t *hc, FILE *dumpfh)
{
    int i, j;

    /* Print out all codewords. */
    fprintf(dumpfh, "Maximum codeword length: %d\n", hc->maxbits);
    fprintf(dumpfh, "Symbols are %s\n", (hc->type == HUFF_CODE_STR) ? "strings" : "ints");
    fprintf(dumpfh, "Codewords:\n");
    for (i = 1; i <= hc->maxbits; ++i) {
        for (j = 0; j < hc->numl[i]; ++j) {
            if (hc->type == HUFF_CODE_STR)
                fprintf(dumpfh, "%-30s", hc->syms[i][j].r.sval);
            else
                fprintf(dumpfh, "%-30d", hc->syms[i][j].r.ival);
            huff_code_dump_codebits(dumpfh, hc->syms[i][j].nbits,
                                    hc->syms[i][j].codeword);
            fprintf(dumpfh, "\n");
        }
    }
    return 0;
}

huff_code_t *
huff_code_retain(huff_code_t *hc)
{
    ++hc->refcount;
    return hc;
}

int
huff_code_free(huff_code_t *hc)
{
    int i;

    if (hc == NULL)
        return 0;
    if (--hc->refcount > 0)
        return hc->refcount;
    for (i = 0; i <= hc->maxbits; ++i) {
        int j;
        for (j = 0; j < hc->numl[i]; ++j) {
            if (hc->type == HUFF_CODE_STR)
                ckd_free(hc->syms[i][j].r.sval);
        }
        ckd_free(hc->syms[i]);
    }
    ckd_free(hc->firstcode);
    ckd_free(hc->numl);
    ckd_free(hc->syms);
    hash_table_free(hc->codewords);
    ckd_free(hc);
    return 0;
}

FILE *
huff_code_attach(huff_code_t *hc, FILE *fh, char const *mode)
{
    FILE *oldfh = huff_code_detach(hc);

    hc->fh = fh;
    if (mode[0] == 'w')
        hc->be = bit_encode_attach(hc->fh);
    return oldfh;
}

FILE *
huff_code_detach(huff_code_t *hc)
{
    FILE *oldfh = hc->fh;
	
    if (hc->be) {
        bit_encode_flush(hc->be);
        bit_encode_free(hc->be);
        hc->be = NULL;
    }
    hc->fh = NULL;
    return oldfh;
}

int
huff_code_encode_int(huff_code_t *hc, int32 sym, uint32 *outcw)
{
    huff_codeword_t *cw;

    if (hash_table_lookup_bkey(hc->codewords,
                               (char const *)&sym,
                               sizeof(sym),
                               (void **)&cw) < 0)
        return 0;
    if (hc->be)
        bit_encode_write_cw(hc->be, cw->codeword, cw->nbits);
    if (outcw) *outcw = cw->codeword;
    return cw->nbits;
}

int
huff_code_encode_str(huff_code_t *hc, char const *sym, uint32 *outcw)
{
    huff_codeword_t *cw;

    if (hash_table_lookup(hc->codewords,
                          sym,
                          (void **)&cw) < 0)
        return 0;
    if (hc->be)
        bit_encode_write_cw(hc->be, cw->codeword, cw->nbits);
    if (outcw) *outcw = cw->codeword;
    return cw->nbits;
}

static huff_codeword_t *
huff_code_decode_data(huff_code_t *hc, char const **inout_data,
                      size_t *inout_data_len, int *inout_offset)
{
    char const *data = *inout_data;
    char const *end = data + *inout_data_len;
    int offset = *inout_offset;
    uint32 cw;
    int cwlen;
    int byte;

    if (data == end)
        return NULL;
    byte = *data++;
    cw = !!(byte & (1 << (7-offset++)));
    cwlen = 1;
    /* printf("%.*x ", cwlen, cw); */
    while (cwlen <= hc->maxbits && cw < hc->firstcode[cwlen]) {
        ++cwlen;
        cw <<= 1;
        if (offset > 7) {
            if (data == end)
                return NULL;
            byte = *data++;
            offset = 0;
        }
        cw |= !!(byte & (1 << (7-offset++)));
        /* printf("%.*x ", cwlen, cw); */
    }
    if (cwlen > hc->maxbits) /* FAIL: invalid data */
        return NULL;

    /* Put the last byte back if there are bits left over. */
    if (offset < 8)
        --data;
    else
        offset = 0;

    /* printf("%.*x\n", cwlen, cw); */
    *inout_data_len = end - data;
    *inout_data = data;
    *inout_offset = offset;
    return hc->syms[cwlen] + (cw - hc->firstcode[cwlen]);
}

static huff_codeword_t *
huff_code_decode_fh(huff_code_t *hc)
{
    uint32 cw;
    int cwlen;
    int byte;

    if ((byte = fgetc(hc->fh)) == EOF)
        return NULL;
    cw = !!(byte & (1 << (7-hc->boff++)));
    cwlen = 1;
    /* printf("%.*x ", cwlen, cw); */
    while (cwlen <= hc->maxbits && cw < hc->firstcode[cwlen]) {
        ++cwlen;
        cw <<= 1;
        if (hc->boff > 7) {
            if ((byte = fgetc(hc->fh)) == EOF)
                return NULL;
            hc->boff = 0;
        }
        cw |= !!(byte & (1 << (7-hc->boff++)));
        /* printf("%.*x ", cwlen, cw); */
    }
    if (cwlen > hc->maxbits) /* FAIL: invalid data */
        return NULL;

    /* Put the last byte back if there are bits left over. */
    if (hc->boff < 8)
        ungetc(byte, hc->fh);
    else
        hc->boff = 0;

    /* printf("%.*x\n", cwlen, cw); */
    return hc->syms[cwlen] + (cw - hc->firstcode[cwlen]);
}

int
huff_code_decode_int(huff_code_t *hc, int *outval,
                     char const **inout_data,
                     size_t *inout_data_len, int *inout_offset)
{
    huff_codeword_t *cw;

    if (inout_data)
        cw = huff_code_decode_data(hc, inout_data, inout_data_len, inout_offset);
    else if (hc->fh)
        cw = huff_code_decode_fh(hc);
    else
        return -1;

    if (cw == NULL)
        return -1;
    if (outval)
        *outval = cw->r.ival;

    return 0;
}

char const *
huff_code_decode_str(huff_code_t *hc,
                     char const **inout_data,
                     size_t *inout_data_len, int *inout_offset)
{
    huff_codeword_t *cw;

    if (inout_data)
        cw = huff_code_decode_data(hc, inout_data, inout_data_len, inout_offset);
    else if (hc->fh)
        cw = huff_code_decode_fh(hc);
    else
        return NULL;

    if (cw == NULL)
        return NULL;

    return cw->r.sval;
}
