/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1996-2004 Carnegie Mellon University.  All rights 
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
 * fe.h
 * 
 * $Log: fe.h,v $
 * Revision 1.11  2005/02/05 02:15:02  egouvea
 * Removed fe_process(), never used
 *
 * Revision 1.10  2004/12/10 16:48:55  rkm
 * Added continuous density acoustic model handling
 *
 *
 */

#if defined(_WIN32) && !defined(GNUWINCE)
#define srand48(x) srand(x)
#define lrand48() rand()
#endif

#ifndef _NEW_FE_H_
#define _NEW_FE_H_

/* Win32/WinCE DLL gunk */
#include <sphinxbase/sphinxbase_export.h>

#include <sphinxbase/cmd_ln.h>
#include <sphinxbase/fixpoint.h>

#ifdef __cplusplus
extern "C" {
#endif
#if 0
/* Fool Emacs. */
}
#endif

#ifdef WORDS_BIGENDIAN
#define NATIVE_ENDIAN "big"
#else
#define NATIVE_ENDIAN "little"
#endif

/** Default number of samples per second. */
#define DEFAULT_SAMPLING_RATE 16000
/** Default number of frames per second. */
#define DEFAULT_FRAME_RATE 100
/** Default spacing between frame starts (equal to
 * DEFAULT_SAMPLING_RATE/DEFAULT_FRAME_RATE) */
#define DEFAULT_FRAME_SHIFT 160
/** Default size of each frame (410 samples @ 16000Hz). */
#define DEFAULT_WINDOW_LENGTH 0.025625 
/** Default number of FFT points. */
#define DEFAULT_FFT_SIZE 512
/** Default number of MFCC coefficients in output. */
#define DEFAULT_NUM_CEPSTRA 13
/** Default number of filter bands used to generate MFCCs. */
#define DEFAULT_NUM_FILTERS 40
/** Default prespeech state length */
#define DEFAULT_PRESPCH_STATE_LEN 10
/** Default postspeech state length */
#define DEFAULT_POSTSPCH_STATE_LEN 50
/** Default lower edge of mel filter bank. */
#define DEFAULT_LOWER_FILT_FREQ 133.33334
/** Default upper edge of mel filter bank. */
#define DEFAULT_UPPER_FILT_FREQ 6855.4976
/** Default pre-emphasis filter coefficient. */
#define DEFAULT_PRE_EMPHASIS_ALPHA 0.97
/** Default type of frequency warping to use for VTLN. */
#define DEFAULT_WARP_TYPE "inverse_linear"
/** Default random number seed to use for dithering. */
#define SEED  -1

#define waveform_to_cepstral_command_line_macro() \
  { "-logspec", \
    ARG_BOOLEAN, \
    "no", \
    "Write out logspectral files instead of cepstra" }, \
   \
  { "-smoothspec", \
    ARG_BOOLEAN, \
    "no", \
    "Write out cepstral-smoothed logspectral files" }, \
   \
  { "-transform", \
    ARG_STRING, \
    "legacy", \
    "Which type of transform to use to calculate cepstra (legacy, dct, or htk)" }, \
   \
  { "-alpha", \
    ARG_FLOAT32, \
    ARG_STRINGIFY(DEFAULT_PRE_EMPHASIS_ALPHA), \
    "Preemphasis parameter" }, \
   \
  { "-samprate", \
    ARG_FLOAT32, \
    ARG_STRINGIFY(DEFAULT_SAMPLING_RATE), \
    "Sampling rate" }, \
   \
  { "-frate", \
    ARG_INT32, \
    ARG_STRINGIFY(DEFAULT_FRAME_RATE), \
    "Frame rate" }, \
   \
  { "-wlen", \
    ARG_FLOAT32, \
    ARG_STRINGIFY(DEFAULT_WINDOW_LENGTH), \
    "Hamming window length" }, \
   \
  { "-nfft", \
    ARG_INT32, \
    ARG_STRINGIFY(DEFAULT_FFT_SIZE), \
    "Size of FFT" }, \
   \
  { "-nfilt", \
    ARG_INT32, \
    ARG_STRINGIFY(DEFAULT_NUM_FILTERS), \
    "Number of filter banks" }, \
   \
  { "-lowerf", \
    ARG_FLOAT32, \
    ARG_STRINGIFY(DEFAULT_LOWER_FILT_FREQ), \
    "Lower edge of filters" }, \
   \
  { "-upperf", \
    ARG_FLOAT32, \
    ARG_STRINGIFY(DEFAULT_UPPER_FILT_FREQ), \
    "Upper edge of filters" }, \
   \
  { "-unit_area", \
    ARG_BOOLEAN, \
    "yes", \
    "Normalize mel filters to unit area" }, \
   \
  { "-round_filters", \
    ARG_BOOLEAN, \
    "yes", \
    "Round mel filter frequencies to DFT points" }, \
   \
  { "-ncep", \
    ARG_INT32, \
    ARG_STRINGIFY(DEFAULT_NUM_CEPSTRA), \
    "Number of cep coefficients" }, \
   \
  { "-doublebw", \
    ARG_BOOLEAN, \
    "no", \
    "Use double bandwidth filters (same center freq)" }, \
   \
  { "-lifter", \
    ARG_INT32, \
    "0", \
    "Length of sin-curve for liftering, or 0 for no liftering." }, \
   \
  { "-vad_prespeech", \
    ARG_INT32, \
    ARG_STRINGIFY(DEFAULT_PRESPCH_STATE_LEN), \
    "Num of speech frames to trigger vad from silence to speech." }, \
   \
  { "-vad_postspeech", \
    ARG_INT32, \
    ARG_STRINGIFY(DEFAULT_POSTSPCH_STATE_LEN), \
    "Num of silence frames to trigger vad from speech to silence." }, \
   \
  { "-vad_threshold", \
    ARG_FLOAT32, \
    "2.0", \
    "Threshold for decision between noise and silence frames. Log-ratio between signal level and noise level." }, \
   \
  { "-input_endian", \
    ARG_STRING, \
    NATIVE_ENDIAN, \
    "Endianness of input data, big or little, ignored if NIST or MS Wav" }, \
   \
  { "-warp_type", \
    ARG_STRING, \
    DEFAULT_WARP_TYPE, \
    "Warping function type (or shape)" }, \
   \
  { "-warp_params", \
    ARG_STRING, \
    NULL, \
    "Parameters defining the warping function" }, \
   \
  { "-dither", \
    ARG_BOOLEAN, \
    "no", \
    "Add 1/2-bit noise" }, \
   \
  { "-seed", \
    ARG_INT32, \
    ARG_STRINGIFY(SEED), \
    "Seed for random number generator; if less than zero, pick our own" }, \
   \
  { "-remove_dc", \
    ARG_BOOLEAN, \
    "no", \
    "Remove DC offset from each frame" }, \
                                          \
  { "-remove_noise", \
    ARG_BOOLEAN, \
    "yes", \
    "Remove noise with spectral subtraction in mel-energies" }, \
                                                                \
  { "-remove_silence", \
    ARG_BOOLEAN, \
    "yes", \
    "Enables VAD, removes silence frames from processing" }, \
                                                             \
  { "-verbose", \
    ARG_BOOLEAN, \
    "no", \
    "Show input filenames" } \
  
  
#ifdef FIXED_POINT
/** MFCC computation type. */
typedef fixed32 mfcc_t;

/** Convert a floating-point value to mfcc_t. */
#define FLOAT2MFCC(x) FLOAT2FIX(x)
/** Convert a mfcc_t value to floating-point. */
#define MFCC2FLOAT(x) FIX2FLOAT(x)
/** Multiply two mfcc_t values. */
#define MFCCMUL(a,b) FIXMUL(a,b)
#define MFCCLN(x,in,out) FIXLN_ANY(x,in,out)
#else /* !FIXED_POINT */

/** MFCC computation type. */
typedef float32 mfcc_t;
/** Convert a floating-point value to mfcc_t. */
#define FLOAT2MFCC(x) (x)
/** Convert a mfcc_t value to floating-point. */
#define MFCC2FLOAT(x) (x)
/** Multiply two mfcc_t values. */
#define MFCCMUL(a,b) ((a)*(b))
#define MFCCLN(x,in,out) log(x)
#endif /* !FIXED_POINT */

/**
 * Structure for the front-end computation.
 */
typedef struct fe_s fe_t;

/**
 * Error codes returned by stuff.
 */
enum fe_error_e {
	FE_SUCCESS = 0,
	FE_OUTPUT_FILE_SUCCESS  = 0,
	FE_CONTROL_FILE_ERROR = -1,
	FE_START_ERROR = -2,
	FE_UNKNOWN_SINGLE_OR_BATCH = -3,
	FE_INPUT_FILE_OPEN_ERROR = -4,
	FE_INPUT_FILE_READ_ERROR = -5,
	FE_MEM_ALLOC_ERROR = -6,
	FE_OUTPUT_FILE_WRITE_ERROR = -7,
	FE_OUTPUT_FILE_OPEN_ERROR = -8,
	FE_ZERO_ENERGY_ERROR = -9,
	FE_INVALID_PARAM_ERROR =  -10
};

/**
 * Initialize a front-end object from global command-line.
 *
 * This is equivalent to calling fe_init_auto_r(cmd_ln_get()).
 *
 * @return Newly created front-end object.
 */
SPHINXBASE_EXPORT
fe_t* fe_init_auto(void);

/**
 * Get the default set of arguments for fe_init_auto_r().
 *
 * @return Pointer to an argument structure which can be passed to
 * cmd_ln_init() in friends to create argument structures for
 * fe_init_auto_r().
 */
SPHINXBASE_EXPORT
arg_t const *fe_get_args(void);

/**
 * Initialize a front-end object from a command-line parse.
 *
 * @param config Command-line object, as returned by cmd_ln_parse_r()
 *               or cmd_ln_parse_file().  Ownership of this object is
 *               claimed by the fe_t, so you must not attempt to free
 *               it manually.  Use cmd_ln_retain() if you wish to
 *               reuse it.
 * @return Newly created front-end object.
 */
SPHINXBASE_EXPORT
fe_t *fe_init_auto_r(cmd_ln_t *config);

/**
 * Retrieve the command-line object used to initialize this front-end.
 *
 * @return command-line object for this front-end.  This pointer is
 *         retained by the fe_t, so you should not attempt to free it
 *         manually.
 */
SPHINXBASE_EXPORT
const cmd_ln_t *fe_get_config(fe_t *fe);

/**
 * Start processing of the stream, resets processed frame counter
 */
SPHINXBASE_EXPORT
void fe_start_stream(fe_t *fe);

/**
 * Start processing an utterance.
 * @return 0 for success, <0 for error (see enum fe_error_e)
 */
SPHINXBASE_EXPORT
int fe_start_utt(fe_t *fe);

/**
 * Get the dimensionality of the output of this front-end object.
 *
 * This is guaranteed to be the number of values in one frame of
 * output from fe_end_utt() and fe_process_frames().  
 * It is usually the number of MFCC
 * coefficients, but it might be the number of log-spectrum bins, if
 * the <tt>-logspec</tt> or <tt>-smoothspec</tt> options to
 * fe_init_auto() were true.
 *
 * @return Dimensionality of front-end output.
 */
SPHINXBASE_EXPORT
int fe_get_output_size(fe_t *fe);

/**
 * Get the dimensionality of the input to this front-end object.
 *
 * This function retrieves the number of input samples consumed by one
 * frame of processing.  To obtain one frame of output, you must have
 * at least <code>*out_frame_size</code> samples.  To obtain <i>N</i>
 * frames of output, you must have at least <code>(N-1) *
 * *out_frame_shift + *out_frame_size</code> input samples.
 *
 * @param out_frame_shift Output: Number of samples between each frame start.
 * @param out_frame_size Output: Number of samples in each frame.
 */
SPHINXBASE_EXPORT
void fe_get_input_size(fe_t *fe, int *out_frame_shift,
                       int *out_frame_size);

/**
 * Get vad state for the last processed frame
 *
 * @return 1 if speech, 0 if silence
 */
SPHINXBASE_EXPORT
uint8 fe_get_vad_state(fe_t *fe);

/**
 * Finish processing an utterance.
 *
 * This function also collects any remaining samples and calculates a
 * final cepstral vector.  If there are overflow samples remaining, it
 * will pad with zeros to make a complete frame.
 *
 * @param fe Front-end object.
 * @param out_cepvector Buffer to hold a residual cepstral vector, or NULL
 *                      if you wish to ignore it.  Must be large enough
 * @param out_nframes Number of frames of residual cepstra created
 *                    (either 0 or 1).
 * @return 0 for success, <0 for error (see enum fe_error_e)
 */
SPHINXBASE_EXPORT
int fe_end_utt(fe_t *fe, mfcc_t *out_cepvector, int32 *out_nframes);

/**
 * Retain ownership of a front end object.
 *
 * @return pointer to the retained front end.
 */
SPHINXBASE_EXPORT
fe_t *fe_retain(fe_t *fe);

/**
 * Free the front end. 
 *
 * Releases resources associated with the front-end object.
 *
 * @return new reference count (0 if freed completely)
 */
SPHINXBASE_EXPORT
int fe_free(fe_t *fe);

/*
 * Do same as fe_process_frames, but also returns
 * voiced audio. Output audio is valid till next
 * fe_process_frames call.
 *
 * DO NOT MIX fe_process_frames calls
 *
 * @param voiced_spch Output: obtain voiced audio samples here
 *
 * @param voiced_spch_nsamps Output: shows voiced_spch length
 *
 * @param out_frameidx Output: index of the utterance start
 */
SPHINXBASE_EXPORT
int fe_process_frames_ext(fe_t *fe,
                      int16 const **inout_spch,
                      size_t *inout_nsamps,
                      mfcc_t **buf_cep,
                      int32 *inout_nframes,
                      int16 **voiced_spch,
                      int32 *voiced_spch_nsamps,
                      int32 *out_frameidx);

/** 
 * Process a block of samples.
 *
 * This function generates up to <code>*inout_nframes</code> of
 * features, or as many as can be generated from
 * <code>*inout_nsamps</code> samples.
 *
 * On exit, the <code>inout_spch</code>, <code>inout_nsamps</code>,
 * and <code>inout_nframes</code> parameters are updated to point to
 * the remaining sample data, the number of remaining samples, and the
 * number of frames processed, respectively.  This allows you to call
 * this repeatedly to process a large block of audio in small (say,
 * 5-frame) chunks:
 *
 *  int16 *bigbuf, *p;
 *  mfcc_t **cepstra;
 *  int32 nsamps;
 *  int32 nframes = 5;
 *
 *  cepstra = (mfcc_t **)
 *      ckd_calloc_2d(nframes, fe_get_output_size(fe), sizeof(**cepstra));
 *  p = bigbuf;
 *  while (nsamps) {
 *      nframes = 5;
 *      fe_process_frames(fe, &p, &nsamps, cepstra, &nframes);
 *      // Now do something with these frames...
 *      if (nframes)
 *          do_some_stuff(cepstra, nframes);
 *  }
 *
 * @param inout_spch Input: Pointer to pointer to speech samples
 *                   (signed 16-bit linear PCM).
 *                   Output: Pointer to remaining samples.
 * @param inout_nsamps Input: Pointer to maximum number of samples to
 *                     process.
 *                     Output: Number of samples remaining in input buffer.
 * @param buf_cep Two-dimensional buffer (allocated with
 *                ckd_calloc_2d()) which will receive frames of output
 *                data.  If NULL, no actual processing will be done,
 *                and the maximum number of output frames which would
 *                be generated is returned in
 *                <code>*inout_nframes</code>.
 * @param inout_nframes Input: Pointer to maximum number of frames to
 *                      generate.
 *                      Output: Number of frames actually generated.
 * @param out_frameidx Index of the first frame returned in a stream
 *
 * @return 0 for success, <0 for failure (see enum fe_error_e)
 */
SPHINXBASE_EXPORT
int fe_process_frames(fe_t *fe,
                      int16 const **inout_spch,
                      size_t *inout_nsamps,
                      mfcc_t **buf_cep,
                      int32 *inout_nframes,
                      int32 *out_frameidx);

/** 
 * Process a block of samples, returning as many frames as possible.
 *
 * This function processes all the samples in a block of data and
 * returns a newly allocated block of feature vectors.  This block
 * needs to be freed with fe_free_2d() after use.
 *
 * It is possible for there to be some left-over data which could not
 * fit in a complete frame.  This data can be processed with
 * fe_end_utt().
 *
 * This function is deprecated in favor of fe_process_frames().
 *
 * @return 0 for success, <0 for failure (see enum fe_error_e)
 */
SPHINXBASE_EXPORT
int fe_process_utt(fe_t *fe,  /**< A front end object */
                   int16 const *spch, /**< The speech samples */
                   size_t nsamps, /**< number of samples*/
                   mfcc_t ***cep_block, /**< Output pointer to cepstra */
                   int32 *nframes /**< Number of frames processed */
	);

/**
 * Free the output pointer returned by fe_process_utt().
 **/
SPHINXBASE_EXPORT
void fe_free_2d(void *arr);

/**
 * Convert a block of mfcc_t to float32 (can be done in-place)
 **/
SPHINXBASE_EXPORT
int fe_mfcc_to_float(fe_t *fe,
                     mfcc_t **input,
                     float32 **output,
                     int32 nframes);

/**
 * Convert a block of float32 to mfcc_t (can be done in-place)
 **/
SPHINXBASE_EXPORT
int fe_float_to_mfcc(fe_t *fe,
                     float32 **input,
                     mfcc_t **output,
                     int32 nframes);

/**
 * Process one frame of log spectra into MFCC using discrete cosine
 * transform.
 *
 * This uses a variant of the DCT-II where the first frequency bin is
 * scaled by 0.5.  Unless somebody misunderstood the DCT-III equations
 * and thought that's what they were implementing here, this is
 * ostensibly done to account for the symmetry properties of the
 * DCT-II versus the DFT - the first coefficient of the input is
 * assumed to be repeated in the negative frequencies, which is not
 * the case for the DFT.  (This begs the question, why not just use
 * the DCT-I, since it has the appropriate symmetry properties...)
 * Moreover, this is bogus since the mel-frequency bins on which we
 * are doing the DCT don't extend to the edge of the DFT anyway.
 *
 * This also means that the matrix used in computing this DCT can not
 * be made orthogonal, and thus inverting the transform is difficult.
 * Therefore if you want to do cepstral smoothing or have some other
 * reason to invert your MFCCs, use fe_logspec_dct2() and its inverse
 * fe_logspec_dct3() instead.
 *
 * Also, it normalizes by 1/nfilt rather than 2/nfilt, for some reason.
 **/
SPHINXBASE_EXPORT
int fe_logspec_to_mfcc(fe_t *fe,  /**< A fe structure */
                       const mfcc_t *fr_spec, /**< One frame of spectrum */
                       mfcc_t *fr_cep /**< One frame of cepstrum */
        );

/**
 * Convert log spectra to MFCC using DCT-II.
 *
 * This uses the "unitary" form of the DCT-II, i.e. with a scaling
 * factor of sqrt(2/N) and a "beta" factor of sqrt(1/2) applied to the
 * cos(0) basis vector (i.e. the one corresponding to the DC
 * coefficient in the output).
 **/
SPHINXBASE_EXPORT
int fe_logspec_dct2(fe_t *fe,  /**< A fe structure */
                    const mfcc_t *fr_spec, /**< One frame of spectrum */
                    mfcc_t *fr_cep /**< One frame of cepstrum */
        );

/**
 * Convert MFCC to log spectra using DCT-III.
 *
 * This uses the "unitary" form of the DCT-III, i.e. with a scaling
 * factor of sqrt(2/N) and a "beta" factor of sqrt(1/2) applied to the
 * cos(0) basis vector (i.e. the one corresponding to the DC
 * coefficient in the input).
 **/
SPHINXBASE_EXPORT
int fe_mfcc_dct3(fe_t *fe,  /**< A fe structure */
                 const mfcc_t *fr_cep, /**< One frame of cepstrum */
                 mfcc_t *fr_spec /**< One frame of spectrum */
        );

#ifdef __cplusplus
}
#endif


#endif
