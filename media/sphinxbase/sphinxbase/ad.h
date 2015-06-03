/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1999-2014 Carnegie Mellon University.  All rights
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
/** \file ad.h
 * \brief generic live audio interface for recording and playback
 */

#ifndef _AD_H_
#define _AD_H_

#include <sphinx_config.h>

#include <sphinxbase/sphinxbase_export.h>
#include <sphinxbase/prim_type.h>

#ifdef __cplusplus
extern "C" {
#endif
#if 0
/* Fool Emacs. */
}
#endif

#define DEFAULT_SAMPLES_PER_SEC	16000

/* Return codes */
#define AD_OK		0
#define AD_EOF		-1
#define AD_ERR_GEN	-1
#define AD_ERR_NOT_OPEN	-2
#define AD_ERR_WAVE	-3

typedef struct ad_rec_s ad_rec_t;

/**
 * Open a specific audio device for recording.
 *
 * The device is opened in non-blocking mode and placed in idle state.
 *
 * @return pointer to read-only ad_rec_t structure if successful, NULL
 * otherwise.  The return value to be used as the first argument to
 * other recording functions.
 */
SPHINXBASE_EXPORT
ad_rec_t *ad_open_dev (
	const char *dev, /**< Device name (platform-specific) */
	int32 samples_per_sec /**< Samples per second */
	);

/**
 * Open the default audio device with a given sampling rate.
 */
SPHINXBASE_EXPORT
ad_rec_t *ad_open_sps (
		       int32 samples_per_sec /**< Samples per second */
		       );


/**
 * Open the default audio device.
 */
SPHINXBASE_EXPORT
ad_rec_t *ad_open ( void );


/* Start audio recording.  Return value: 0 if successful, <0 otherwise */
SPHINXBASE_EXPORT
int32 ad_start_rec (ad_rec_t *);


/* Stop audio recording.  Return value: 0 if successful, <0 otherwise */
SPHINXBASE_EXPORT
int32 ad_stop_rec (ad_rec_t *);


/* Close the recording device.  Return value: 0 if successful, <0 otherwise */
SPHINXBASE_EXPORT
int32 ad_close (ad_rec_t *);

/*
 * Read next block of audio samples while recording; read upto max samples into buf.
 * Return value: # samples actually read (could be 0 since non-blocking); -1 if not
 * recording and no more samples remaining to be read from most recent recording.
 */
SPHINXBASE_EXPORT
int32 ad_read (ad_rec_t *, int16 *buf, int32 max);


#ifdef __cplusplus
}
#endif

#endif
