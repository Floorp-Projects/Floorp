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
 * feat.h -- Cepstral features computation.
 */

#ifndef _S3_FEAT_H_
#define _S3_FEAT_H_

#include <stdio.h>

/* Win32/WinCE DLL gunk */
#include <sphinxbase/sphinxbase_export.h>
#include <sphinxbase/prim_type.h>
#include <sphinxbase/fe.h>
#include <sphinxbase/cmn.h>
#include <sphinxbase/agc.h>

#ifdef __cplusplus
extern "C" {
#endif
#if 0
/* Fool Emacs. */
}
#endif

/** \file feat.h
 * \brief compute the dynamic coefficients from the cepstral vector. 
 */
#define LIVEBUFBLOCKSIZE        256    /** Blocks of 256 vectors allocated 
					   for livemode decoder */
#define S3_MAX_FRAMES		15000    /* RAH, I believe this is still too large, but better than before */

#define cepstral_to_feature_command_line_macro()                        \
{ "-feat",                                                              \
      ARG_STRING,                                                       \
      "1s_c_d_dd",                                                      \
      "Feature stream type, depends on the acoustic model" },           \
{ "-ceplen",                                                            \
      ARG_INT32,                                                        \
      "13",                                                             \
     "Number of components in the input feature vector" },              \
{ "-cmn",                                                               \
      ARG_STRING,                                                       \
      "current",                                                        \
      "Cepstral mean normalization scheme ('current', 'prior', or 'none')" }, \
{ "-cmninit",                                                           \
      ARG_STRING,                                                       \
      "8.0",                                                            \
      "Initial values (comma-separated) for cepstral mean when 'prior' is used" }, \
{ "-varnorm",                                                           \
      ARG_BOOLEAN,                                                      \
      "no",                                                             \
      "Variance normalize each utterance (only if CMN == current)" },   \
{ "-agc",                                                               \
      ARG_STRING,                                                       \
      "none",                                                           \
      "Automatic gain control for c0 ('max', 'emax', 'noise', or 'none')" }, \
{ "-agcthresh",                                                         \
      ARG_FLOAT32,                                                      \
      "2.0",                                                            \
      "Initial threshold for automatic gain control" },                 \
{ "-lda",                                                               \
      ARG_STRING,                                                       \
      NULL,                                                             \
      "File containing transformation matrix to be applied to features (single-stream features only)" }, \
{ "-ldadim",                                                            \
      ARG_INT32,                                                        \
      "0",                                                              \
      "Dimensionality of output of feature transformation (0 to use entire matrix)" }, \
{"-svspec",                                                             \
     ARG_STRING,                                                        \
     NULL,                                                           \
     "Subvector specification (e.g., 24,0-11/25,12-23/26-38 or 0-12/13-25/26-38)"}

/**
 * \struct feat_t
 * \brief Structure for describing a speech feature type
 * Structure for describing a speech feature type (no. of streams and stream widths),
 * as well as the computation for converting the input speech (e.g., Sphinx-II format
 * MFC cepstra) into this type of feature vectors.
 */
typedef struct feat_s {
    int refcount;       /**< Reference count. */
    char *name;		/**< Printable name for this feature type */
    int32 cepsize;	/**< Size of input speech vector (typically, a cepstrum vector) */
    int32 n_stream;	/**< Number of feature streams; e.g., 4 in Sphinx-II */
    uint32 *stream_len;	/**< Vector length of each feature stream */
    int32 window_size;	/**< Number of extra frames around given input frame needed to compute
                           corresponding output feature (so total = window_size*2 + 1) */
    int32 n_sv;         /**< Number of subvectors */
    uint32 *sv_len;      /**< Vector length of each subvector */
    int32 **subvecs;    /**< Subvector specification (or NULL for none) */
    mfcc_t *sv_buf;      /**< Temporary copy buffer for subvector projection */
    int32 sv_dim;       /**< Total dimensionality of subvector (length of sv_buf) */

    cmn_type_t cmn;	/**< Type of CMN to be performed on each utterance */
    int32 varnorm;	/**< Whether variance normalization is to be performed on each utt;
                           Irrelevant if no CMN is performed */
    agc_type_t agc;	/**< Type of AGC to be performed on each utterance */

    /**
     * Feature computation function. 
     * @param fcb the feat_t describing this feature type
     * @param input pointer into the input cepstra
     * @param feat a 2-d array of output features (n_stream x stream_len)
     * @return 0 if successful, -ve otherwise.
     *
     * Function for converting window of input speech vector
     * (input[-window_size..window_size]) to output feature vector
     * (feat[stream][]).  If NULL, no conversion available, the
     * speech input must be feature vector itself.
     **/
    void (*compute_feat)(struct feat_s *fcb, mfcc_t **input, mfcc_t **feat);
    cmn_t *cmn_struct;	/**< Structure that stores the temporary variables for cepstral 
                           means normalization*/
    agc_t *agc_struct;	/**< Structure that stores the temporary variables for acoustic
                           gain control*/

    mfcc_t **cepbuf;    /**< Circular buffer of MFCC frames for live feature computation. */
    mfcc_t **tmpcepbuf; /**< Array of pointers into cepbuf to handle border cases. */
    int32   bufpos;     /**< Write index in cepbuf. */
    int32   curpos;     /**< Read index in cepbuf. */

    mfcc_t ***lda; /**< Array of linear transformations (for LDA, MLLT, or whatever) */
    uint32 n_lda;   /**< Number of linear transformations in lda. */
    uint32 out_dim; /**< Output dimensionality */
} feat_t;

/**
 * Name of feature type.
 */
#define feat_name(f)		((f)->name)
/**
 * Input dimensionality of feature.
 */
#define feat_cepsize(f)		((f)->cepsize)
/**
 * Size of dynamic feature window.
 */
#define feat_window_size(f)	((f)->window_size)
/**
 * Number of feature streams.
 *
 * @deprecated Do not use this, use feat_dimension1() instead.
 */
#define feat_n_stream(f)	((f)->n_stream)
/**
 * Length of feature stream i.
 *
 * @deprecated Do not use this, use feat_dimension2() instead.
 */
#define feat_stream_len(f,i)	((f)->stream_len[i])
/**
 * Number of streams or subvectors in feature output.
 */
#define feat_dimension1(f)	((f)->n_sv ? (f)->n_sv : f->n_stream)
/**
 * Dimensionality of stream/subvector i in feature output.
 */
#define feat_dimension2(f,i)	((f)->lda ? (f)->out_dim : ((f)->sv_len ? (f)->sv_len[i] : f->stream_len[i]))
/**
 * Total dimensionality of feature output.
 */
#define feat_dimension(f)	((f)->out_dim)
/**
 * Array with stream/subvector lengths
 */
#define feat_stream_lengths(f)  ((f)->lda ? (&(f)->out_dim) : (f)->sv_len ? (f)->sv_len : f->stream_len)

/**
 * Parse subvector specification string.
 *
 * Format of specification:
 *   \li '/' separated list of subvectors
 *   \li each subvector is a ',' separated list of subranges
 *   \li each subrange is a single \verbatim <number> \endverbatim or
 *       \verbatim <number>-<number> \endverbatim (inclusive), where
 *       \verbatim <number> \endverbatim is a feature vector dimension
 *       specifier.
 *
 * E.g., "24,0-11/25,12-23/26,27-38" has:
 *   \li 3 subvectors
 *   \li the 1st subvector has feature dims: 24, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, and 11.
 *   \li etc.
 *
 * @param str subvector specification string.
 * @return allocated 2-D array of subvector specs (free with
 * subvecs_free()).  If there are N subvectors specified, subvec[N] =
 * NULL; and each subvec[0]..subvec[N-1] is -1 terminated vector of
 * feature dims.
 */
SPHINXBASE_EXPORT
int32 **parse_subvecs(char const *str);

/**
 * Free array of subvector specs.
 */
SPHINXBASE_EXPORT
void subvecs_free(int32 **subvecs);


/**
 * Allocate an array to hold several frames worth of feature vectors.  The returned value
 * is the mfcc_t ***data array, organized as follows:
 *
 * - data[0][0] = frame 0 stream 0 vector, data[0][1] = frame 0 stream 1 vector, ...
 * - data[1][0] = frame 1 stream 0 vector, data[0][1] = frame 1 stream 1 vector, ...
 * - data[2][0] = frame 2 stream 0 vector, data[0][1] = frame 2 stream 1 vector, ...
 * - ...
 *
 * NOTE: For I/O convenience, the entire data area is allocated as one contiguous block.
 * @return pointer to the allocated space if successful, NULL if any error.
 */
SPHINXBASE_EXPORT
mfcc_t ***feat_array_alloc(feat_t *fcb,	/**< In: Descriptor from feat_init(), used
					     to obtain number of streams and stream sizes */
                           int32 nfr	/**< In: Number of frames for which to allocate */
    );

/**
 * Realloate the array of features. Requires us to know the old size
 */
SPHINXBASE_EXPORT
mfcc_t ***feat_array_realloc(feat_t *fcb, /**< In: Descriptor from feat_init(), used
					      to obtain number of streams and stream sizes */
			     mfcc_t ***old_feat, /**< Feature array. Freed */
                             int32 ofr,	/**< In: Previous number of frames */
                             int32 nfr	/**< In: Number of frames for which to allocate */
    );

/**
 * Free a buffer allocated with feat_array_alloc()
 */
SPHINXBASE_EXPORT
void feat_array_free(mfcc_t ***feat);


/**
 * Initialize feature module to use the selected type of feature stream.  
 * One-time only initialization at the beginning of the program.  Input type 
 * is a string defining the  kind of input->feature conversion desired:
 *
 * - "s2_4x":     s2mfc->Sphinx-II 4-feature stream,
 * - "1s_c_d_dd": s2mfc->Sphinx 3.x single feature stream,
 * - "s3_1x39":   s2mfc->Sphinx 3.0 single feature stream,
 * - "n1,n2,n3,...": Explicit feature vector layout spec. with comma-separated 
 *   feature stream lengths.  In this case, the input data is already in the 
 *   feature format and there is no conversion necessary.
 *
 * @return (feat_t *) descriptor if successful, NULL if error.  Caller 
 * must not directly modify the contents of the returned value.
 */
SPHINXBASE_EXPORT
feat_t *feat_init(char const *type,/**< In: Type of feature stream */
                  cmn_type_t cmn, /**< In: Type of cepstram mean normalization to 
                                     be done before feature computation; can be 
                                     CMN_NONE (for none) */
                  int32 varnorm,  /**< In: (boolean) Whether variance 
                                     normalization done on each utt; only 
                                     applicable if CMN also done */
                  agc_type_t agc, /**< In: Type of automatic gain control to be 
                                     done before feature computation */
                  int32 breport, /**< In: Whether to show a report for feat_t */
                  int32 cepsize  /**< Number of components in the input vector
                                    (or 0 for the default for this feature type,
                                    which is usually 13) */
    );

/**
 * Add an LDA transformation to the feature module from a file.
 * @return 0 for success or -1 if reading the LDA file failed.
 **/
SPHINXBASE_EXPORT
int32 feat_read_lda(feat_t *feat,	 /**< In: Descriptor from feat_init() */
                    const char *ldafile, /**< In: File to read the LDA matrix from. */
                    int32 dim		 /**< In: Dimensionality of LDA output. */
    );

/**
 * Transform a block of features using the feature module's LDA transform.
 **/
SPHINXBASE_EXPORT
void feat_lda_transform(feat_t *fcb,		/**< In: Descriptor from feat_init() */
                        mfcc_t ***inout_feat,	/**< Feature block to transform. */
                        uint32 nfr		/**< In: Number of frames in inout_feat. */
    );

/**
 * Add a subvector specification to the feature module.
 *
 * The subvector splitting will be performed after dynamic feature
 * computation, CMN, AGC, and any LDA transformation.  The number of
 * streams in the dynamic feature type must be one, as with LDA.
 *
 * After adding a subvector specification, the output of feature
 * computation will be split into multiple subvectors, and
 * feat_array_alloc() will allocate pointers accordingly.  The number
 * of <em>streams</em> will remain the 
 *
 * @param fcb the feature descriptor.
 * @param subvecs subvector specification.  This pointer is retained
 * by the feat_t and should not be freed manually.
 * @return 0 for success or -1 if the subvector specification was
 * invalid.
 */
SPHINXBASE_EXPORT
int feat_set_subvecs(feat_t *fcb, int32 **subvecs);

/**
 * Print the given block of feature vectors to the given FILE.
 */
SPHINXBASE_EXPORT
void feat_print(feat_t *fcb,		/**< In: Descriptor from feat_init() */
		mfcc_t ***feat,		/**< In: Feature data to be printed */
		int32 nfr,		/**< In: Number of frames of feature data above */
		FILE *fp		/**< In: Output file pointer */
    );

  
/**
 * Read a specified MFC file (or given segment within it), perform
 * CMN/AGC as indicated by <code>fcb</code>, and compute feature
 * vectors.  Feature vectors are computed for the entire segment
 * specified, by including additional surrounding or padding frames to
 * accommodate the feature windows.
 *
 * @return Number of frames of feature vectors computed if successful;
 * -1 if any error.  <code>If</code> feat is NULL, then no actual
 * computation will be done, and the number of frames which must be
 * allocated will be returned.
 * 
 * A note on how the file path is constructed: If the control file
 * already specifies extension or absolute path, then these are not
 * applied. The default extension is defined by the application.
 */
SPHINXBASE_EXPORT
int32 feat_s2mfc2feat(feat_t *fcb,	/**< In: Descriptor from feat_init() */
		      const char *file,	/**< In: File to be read */
		      const char *dir,	/**< In: Directory prefix for file, 
					   if needed; can be NULL */
		      const char *cepext,/**< In: Extension of the
					   cepstrum file.It cannot be
					   NULL */
		      int32 sf, int32 ef,   /* Start/End frames
                                               within file to be read. Use
                                               0,-1 to process entire
                                               file */
		      mfcc_t ***feat,	/**< Out: Computed feature vectors; 
					   caller must allocate this space */
		      int32 maxfr	/**< In: Available space (number of frames) in 
					   above feat array; it must be 
					   sufficient to hold the result.
                                           Pass -1 for no limit. */
    );


/**
 * Feature computation routine for live mode decoder.
 *
 * This function computes features for blocks of incoming data. It
 * retains an internal buffer for computing deltas, which means that
 * the number of output frames will not necessarily equal the number
 * of input frames.
 *
 * <strong>It is very important</strong> to realize that the number of
 * output frames can be <strong>greater than</strong> the number of
 * input frames, specifically when <code>endutt</code> is true.  It is
 * guaranteed to never exceed <code>*inout_ncep +
 * feat_window_size(fcb)</code>.  You <strong>MUST</strong> have
 * allocated at least that many frames in <code>ofeat</code>, or you
 * will experience a buffer overflow.
 *
 * If beginutt and endutt are both true, CMN_CURRENT and AGC_MAX will
 * be done.  Otherwise only CMN_PRIOR and AGC_EMAX will be done.
 *
 * If beginutt is false, endutt is true, and the number of input
 * frames exceeds the input size, then end-of-utterance processing
 * won't actually be done.  This condition can easily be checked,
 * because <code>*inout_ncep</code> will equal the return value on
 * exit, and will also be smaller than the value of
 * <code>*inout_ncep</code> on entry.
 *
 * @return The number of output frames actually computed.
 **/
SPHINXBASE_EXPORT
int32 feat_s2mfc2feat_live(feat_t  *fcb,     /**< In: Descriptor from feat_init() */
                           mfcc_t **uttcep,  /**< In: Incoming cepstral buffer */
                           int32 *inout_ncep,/**< In: Size of incoming buffer.
                                                Out: Number of incoming frames consumed. */
                           int32 beginutt,   /**< In: Begining of utterance flag */
                           int32 endutt,     /**< In: End of utterance flag */
                           mfcc_t ***ofeat   /**< In: Output feature buffer.  See
                                                <strong>VERY IMPORTANT</strong> note
                                                about the size of this buffer above. */
    );


/**
 * Update the normalization stats, possibly in the end of utterance
 *
 */
SPHINXBASE_EXPORT
void feat_update_stats(feat_t *fcb);


/**
 * Retain ownership of feat_t.
 *
 * @return pointer to retained feat_t.
 */
SPHINXBASE_EXPORT
feat_t *feat_retain(feat_t *f);

/**
 * Release resource associated with feat_t
 *
 * @return new reference count (0 if freed)
 */
SPHINXBASE_EXPORT
int feat_free(feat_t *f /**< In: feat_t */
    );

/**
 * Report the feat_t data structure 
 */
SPHINXBASE_EXPORT
void feat_report(feat_t *f /**< In: feat_t */
    );
#ifdef __cplusplus
}
#endif


#endif
