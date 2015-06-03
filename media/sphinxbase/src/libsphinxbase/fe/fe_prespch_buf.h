/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 2013 Carnegie Mellon University.  All rights 
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

/* Buffer that maintains both features and raw audio for the VAD implementation */

#ifndef FE_INTERNAL_H
#define FE_INTERNAL_H

#include "sphinxbase/fe.h"

typedef struct prespch_buf_s prespch_buf_t;

/* Creates prespeech buffer */
prespch_buf_t *fe_prespch_init(int num_frames, int num_cepstra,
                               int num_samples);

/* Extends pcm prespeech buffer with specified amount of frames */
void fe_prespch_extend_pcm(prespch_buf_t* prespch_buf, int num_frames_pcm);

/* Reads mfcc frame from prespeech buffer */
int fe_prespch_read_cep(prespch_buf_t * prespch_buf, mfcc_t * fea);

/* Writes mfcc frame to prespeech buffer */
void fe_prespch_write_cep(prespch_buf_t * prespch_buf, mfcc_t * fea);

/* Reads pcm frame from prespeech buffer */
void fe_prespch_read_pcm(prespch_buf_t * prespch_buf, int16 ** samples,
                         int32 * samples_num);

/* Writes pcm frame to prespeech buffer */
void fe_prespch_write_pcm(prespch_buf_t * prespch_buf, int16 * samples);

/* Resets read/write pointers for cepstrum buffer */
void fe_prespch_reset_cep(prespch_buf_t * prespch_buf);

/* Resets read/write pointer for pcm audio buffer */
void fe_prespch_reset_pcm(prespch_buf_t * prespch_buf);

/* Releases prespeech buffer */
void fe_prespch_free(prespch_buf_t * prespch_buf);

/* Returns number of accumulated frames */
int32 fe_prespch_ncep(prespch_buf_t * prespch_buf);

#endif                          /* FE_INTERNAL_H */
