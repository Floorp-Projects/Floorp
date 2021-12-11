/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*--------------------------------*-C-*---------------------------------*
 * File:
 *       fftn.h
 * ---------------------------------------------------------------------*
 * Re[]:        real value array
 * Im[]:        imaginary value array
 * nTotal:      total number of complex values
 * nPass:       number of elements involved in this pass of transform
 * nSpan:       nspan/nPass = number of bytes to increment pointer
 *              in Re[] and Im[]
 * isign: exponent: +1 = forward  -1 = reverse
 * scaling: normalizing constant by which the final result is *divided*
 * scaling == -1, normalize by total dimension of the transform
 * scaling <  -1, normalize by the square-root of the total dimension
 *
 * ----------------------------------------------------------------------
 * See the comments in the code for correct usage!
 */

#ifndef MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_FFT_H_
#define MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_FFT_H_


#include "structs.h"


/* double precision routine */


int WebRtcIsac_Fftns (unsigned int ndim, const int dims[], double Re[], double Im[],
                     int isign, double scaling, FFTstr *fftstate);



#endif /* MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_FFT_H_ */
