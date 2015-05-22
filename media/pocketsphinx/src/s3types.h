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

#ifndef _S3_S3TYPES_H_
#define _S3_S3TYPES_H_

#include <float.h>
#include <assert.h>

#include <sphinxbase/prim_type.h>
#include <sphinxbase/err.h>
#include <sphinxbase/ckd_alloc.h>

/** \file s3types.h
 * \brief Size definition of semantically units. Common for both s3 and s3.X decoder. 
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Size definitions for more semantially meaningful units.
 * Illegal value definitions, limits, and tests for specific types.
 * NOTE: Types will be either int32 or smaller; only smaller ones may be unsigned (i.e.,
 * no type will be uint32).
 */

typedef int16		s3cipid_t;	/** Ci phone id */
#define BAD_S3CIPID	((s3cipid_t) -1)
#define NOT_S3CIPID(p)	((p)<0)
#define IS_S3CIPID(p)	((p)>=0)
#define MAX_S3CIPID	32767

/*#define MAX_S3CIPID	127*/

typedef int32		s3pid_t;	/** Phone id (triphone or ciphone) */
#define BAD_S3PID	((s3pid_t) -1)
#define NOT_S3PID(p)	((p)<0)
#define IS_S3PID(p)	((p)>=0)
#define MAX_S3PID	((int32)0x7ffffffe)

typedef uint16		s3ssid_t;	/** Senone sequence id (triphone or ciphone) */
#define BAD_S3SSID	((s3ssid_t) 0xffff)
#define NOT_S3SSID(p)	((p) == BAD_S3SSID)
#define IS_S3SSID(p)	((p) != BAD_S3SSID)
#define MAX_S3SSID	((s3ssid_t)0xfffe)

typedef int32		s3tmatid_t;	/** Transition matrix id; there can be as many as pids */
#define BAD_S3TMATID	((s3tmatid_t) -1)
#define NOT_S3TMATID(t)	((t)<0)
#define IS_S3TMATID(t)	((t)>=0)
#define MAX_S3TMATID	((int32)0x7ffffffe)

typedef int32		s3wid_t;	/** Dictionary word id */
#define BAD_S3WID	((s3wid_t) -1)
#define NOT_S3WID(w)	((w)<0)
#define IS_S3WID(w)	((w)>=0)
#define MAX_S3WID	((int32)0x7ffffffe)

#ifdef __cplusplus
}
#endif

#endif
