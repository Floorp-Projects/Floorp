/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 2014 Carnegie Mellon University.  All rights
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
* kws_detections.c -- Object for storing keyword search results
*/

#include "kws_detections.h"

void
kws_detections_reset(kws_detections_t *detections)
{
    gnode_t *gn;

    if (!detections->detect_list)
        return;

    for (gn = detections->detect_list; gn; gn = gnode_next(gn))
        ckd_free(gnode_ptr(gn));
    detections->detect_list = NULL;
}

void
kws_detections_add(kws_detections_t *detections, const char* keyphrase, int sf, int ef, int prob, int ascr)
{
    kws_detection_t* detection;

    detection = (kws_detection_t *)ckd_calloc(1, sizeof(*detection));
    detection->sf = sf;
    detection->ef = ef;
    detection->keyphrase = keyphrase;
    detection->prob = prob;
    detection->ascr = ascr;
    if (!detections->detect_list) {
        detections->detect_list = glist_add_ptr(detections->detect_list, (void *)detection);
        detections->insert_ptr = detections->detect_list;
    } else {
        detections->insert_ptr = glist_insert_ptr(detections->insert_ptr, (void *)detection);
    }
}

void
kws_detections_hyp_str(kws_detections_t *detections, char** hyp_str)
{
    gnode_t *gn;
    char *c;
    int len;

    len = 0;
    for (gn = detections->detect_list; gn; gn = gnode_next(gn))
        len += strlen(((kws_detection_t *)gnode_ptr(gn))->keyphrase) + 2;

    if (len == 0) {
        hyp_str = NULL;
        return;
    }

    *hyp_str = (char *)ckd_calloc(len, sizeof(char));
    c = *hyp_str;
    for (gn = detections->detect_list; gn; gn = gnode_next(gn)) {
        const char *word = ((kws_detection_t *)gnode_ptr(gn))->keyphrase;
        memcpy(c, word, strlen(word));
        c += strlen(word);
        *c = ' ';
        c++;
    }
    c--;
    *c = '\0';
}

