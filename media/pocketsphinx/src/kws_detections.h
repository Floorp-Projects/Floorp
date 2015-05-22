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
 * kws_detections.h -- Structures for storing keyword spotting results.
 */

#ifndef __KWS_DETECTIONS_H__
#define __KWS_DETECTIONS_H__

/* SphinxBase headers. */
#include <sphinxbase/glist.h>

/* Local headers. */
#include "pocketsphinx_internal.h"
#include "hmm.h"

typedef struct kws_detection_s {
    const char* keyphrase;
    frame_idx_t sf;
    frame_idx_t ef;
    int32 prob;
    int32 ascr;
} kws_detection_t;

typedef struct kws_detections_s {
    glist_t detect_list;
    gnode_t *insert_ptr;
} kws_detections_t;

/**
 * Reset history structure.
 */
void kws_detections_reset(kws_detections_t *detections);

/**
 * Add history entry.
 */
void kws_detections_add(kws_detections_t *detections, const char* keyphrase, int sf, int ef, int prob, int ascr);

/**
 * Compose hypothesis.
 */
void kws_detections_hyp_str(kws_detections_t *detections, char** hyp_str);

#endif                          /* __KWS_DETECTIONS_H__ */