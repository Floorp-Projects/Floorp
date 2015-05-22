/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (c) 2008 Beyond Access, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY BEYOND ACCESS, INC. ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL BEYOND ACCESS, INC.  NOR
 * ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file yin.h
 * @brief Implementation of pitch estimation
 * @author David Huggins-Daines <dhuggins@cs.cmu.edu>
 *
 * This implements part of the YIN algorithm:
 *
 * "YIN, a fundamental frequency estimator for speech and music".
 * Alain de Cheveigné and Hideki Kawahara.  Journal of the Acoustical
 * Society of America, 111 (4), April 2002.
 */

#ifndef __YIN_H__
#define __YIN_H__

#ifdef __cplusplus
extern "C"
#endif
#if 0
} /* Fool Emacs. */
#endif

/* Win32/WinCE DLL gunk */
#include <sphinxbase/sphinxbase_export.h>
#include <sphinxbase/prim_type.h>

/**
 * Frame-based moving-window pitch estimator.
 */
typedef struct yin_s yin_t;

/**
 * Initialize moving-window pitch estimation.
 */
SPHINXBASE_EXPORT
yin_t *yin_init(int frame_size, float search_threshold,
                float search_range, int smooth_window);

/**
 * Free a moving-window pitch estimator.
 */
SPHINXBASE_EXPORT
void yin_free(yin_t *pe);

/**
 * Start processing an utterance.
 */
SPHINXBASE_EXPORT
void yin_start(yin_t *pe);

/**
 * Mark the end of an utterance.
 */
SPHINXBASE_EXPORT
void yin_end(yin_t *pe);

/**
 * Store a frame of data to the pitch estimator.
 *
 * @param pe Pitch estimator.
 * @param frame Frame of <code>frame_size</code> (see
 * yin_init()) samples of audio data.
 */
SPHINXBASE_EXPORT
void yin_store(yin_t *pe, int16 const *frame);

/**
 * Feed a frame of data to the pitch estimator.
 *
 * @param pe Pitch estimator.
 * @param frame Frame of <code>frame_size</code> (see
 * yin_init()) samples of audio data.
 */
SPHINXBASE_EXPORT
void yin_write(yin_t *pe, int16 const *frame);

/**
 * Feed stored frame of data to the pitch estimator.
 * (see yin_store())
 *
 * @param pe Pitch estimator.
 */
SPHINXBASE_EXPORT
void yin_write_stored(yin_t *pe);

/**
 * Read a raw estimated pitch value from the pitch estimator.
 *
 * @param pe Pitch estimator.
 * @param out_period Output: an estimate of the period (*not* the pitch)
 *                  of the signal in samples.
 * @param out_bestdiff Output: the minimum normalized difference value
 *                     associated with <code>*out_pitch</code>, in Q15
 *                     format (i.e. scaled by 32768).  This can be
 *                     interpreted as one minus the probability of voicing.
 * @return Non-zero if enough data was avaliable to return a pitch
 *         estimate, zero otherwise.
 */
SPHINXBASE_EXPORT
int yin_read(yin_t *pe, uint16 *out_period, float *out_bestdiff);

#ifdef __cplusplus
}
#endif

#endif /* __YIN_H__ */

