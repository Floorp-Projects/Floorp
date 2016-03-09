/**
 * File: omxSP.h
 * Brief: OpenMAX DL v1.0.2 - Signal Processing library
 *
 * Copyright (c) 2005-2008,2015 The Khronos Group Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * MODIFICATIONS TO THIS FILE MAY MEAN IT NO LONGER ACCURATELY REFLECTS
 * KHRONOS STANDARDS. THE UNMODIFIED, NORMATIVE VERSIONS OF KHRONOS
 * SPECIFICATIONS AND HEADER INFORMATION ARE LOCATED AT
 *    https://www.khronos.org/registry/
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 */

/* *****************************************************************************************/

#ifndef _OMXSP_H_
#define _OMXSP_H_

#include "dl/api/omxtypes.h"

#ifdef __cplusplus
extern "C" {
#endif


/* 2.1 Vendor-Specific FFT Data Structures */
 typedef void OMXFFTSpec_C_SC16;
 typedef void OMXFFTSpec_C_SC32;
 typedef void OMXFFTSpec_R_S16S32;
 typedef void OMXFFTSpec_R_S16;
 typedef void OMXFFTSpec_R_S32;
 typedef void OMXFFTSpec_R_F32;
 typedef void OMXFFTSpec_C_FC32;

/**
 * Function:  omxSP_Copy_S16   (2.2.1.1.1)
 *
 * Description:
 * Copies the len elements of the vector pointed to by pSrcinto the len 
 * elements of the vector pointed to by pDst. That is: 
 *     pDst[i] = pSrc[i], for (i=0, 1, ..., len-1)
 *
 * Input Arguments:
 *   
 *   pSrc - pointer to the source vector 
 *   len - number of elements contained in the source and destination vectors 
 *
 * Output Arguments:
 *   
 *   pDst - pointer to the destination vector 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments detected; returned if one or more of 
 *              the following is true: 
 *    -   pSrc or pDst is NULL 
 *    -   len < 0 
 *
 */
OMXResult omxSP_Copy_S16 (
    const OMX_S16 *pSrc,
    OMX_S16 *pDst,
    OMX_INT len
);



/**
 * Function:  omxSP_DotProd_S16   (2.2.2.1.1)
 *
 * Description:
 * Calculates the dot product of the two input vectors.  This function does 
 * not perform scaling. The internal accumulator width must be at least 32 
 * bits.  If any of the partially accumulated values exceeds the range of a 
 * signed 32-bit integer then the result is undefined. 
 *
 * Input Arguments:
 *   
 *   pSrc1 - pointer to the first input vector; must be aligned on an 8-byte 
 *            boundary. 
 *   pSrc2 - pointer to the second input vector; must be aligned on an 8-byte 
 *            boundary. 
 *   len - length of the vectors in pSrc1 and pSrc2 
 *
 * Output Arguments:
 *
 * Return Value:
 *    
 *    The dot product result  Note: this function returns the actual result 
 *              rather than the standard OMXError. 
 *
 */
OMX_S32 omxSP_DotProd_S16 (
    const OMX_S16 *pSrc1,
    const OMX_S16 *pSrc2,
    OMX_INT len
);



/**
 * Function:  omxSP_DotProd_S16_Sfs   (2.2.2.1.2)
 *
 * Description:
 * Calculates the dot product of the two input signals with output scaling 
 * and saturation, i.e., the result is multiplied by two to the power of the 
 * negative (-)scalefactor (scaled) prior to return.  The result is saturated 
 * with rounding if the scaling operation produces a value outside the range 
 * of a signed 32-bit integer. Rounding behavior is defined in section 1.6.7 
 * Integer Scaling and Rounding Conventions. The internal accumulator width 
 * must be at least 32 bits. The result is undefined if any of the partially 
 * accumulated values exceeds the range of a signed 32-bit integer. 
 *
 * Input Arguments:
 *   
 *   pSrc1 - pointer to the first input vector; must be aligned on an 8-byte 
 *            boundary. 
 *   pSrc2 - pointer to the second input vector; must be aligned on an 8-byte 
 *            boundary. 
 *   len - length of the vectors in pSrc1 and pSrc2 
 *   scaleFactor - integer scalefactor 
 *
 * Output Arguments:
 *
 * Return Value:
 *    
 *    The dot product result  Note: This function returns the actual result 
 *              rather than the standard OMXError. 
 *
 */
OMX_S32 omxSP_DotProd_S16_Sfs (
    const OMX_S16 *pSrc1,
    const OMX_S16 *pSrc2,
    OMX_INT len,
    OMX_INT scaleFactor
);



/**
 * Function:  omxSP_BlockExp_S16   (2.2.2.2.2)
 *
 * Description:
 * Block exponent calculation for 16-bit and 32-bit signals (count leading 
 * sign bits). These functions compute the number of extra sign bits of all 
 * values in the 16-bit and 32-bit input vector pSrc and return the minimum 
 * sign bit count. This is also the maximum shift value that could be used in 
 * scaling the block of data.  The functions BlockExp_S16 and 
 * BlockExp_S32 return the values 15 and 31, respectively, for input vectors in 
 * which all entries are equal to zero.  
 *
 * Note: These functions differ from other DL functions by not returning the 
 *       standard OMXError but the actual result. 
 *
 * Input Arguments:
 *   
 *   pSrc - pointer to the input vector 
 *   len - number of elements contained in the input and output 
 *         vectors (0 < len < 65536) 
 *
 * Output Arguments:
 *   
 *   none 
 *
 * Return Value:
 *    
 *    Maximum exponent that may be used in scaling 
 *
 */
OMX_INT omxSP_BlockExp_S16 (
    const OMX_S16 *pSrc,
    OMX_INT len
);



/**
 * Function:  omxSP_BlockExp_S32   (2.2.2.2.2)
 *
 * Description:
 * Block exponent calculation for 16-bit and 32-bit signals (count leading 
 * sign bits). These functions compute the number of extra sign bits of all 
 * values in the 16-bit and 32-bit input vector pSrc and return the minimum 
 * sign bit count. This is also the maximum shift value that could be used in 
 * scaling the block of data.  The functions BlockExp_S16 and 
 * BlockExp_S32 return the values 15 and 31, respectively, for input vectors in 
 * which all entries are equal to zero.  
 * 
 * Note: These functions differ from other DL functions by not returning the 
 *       standard OMXError but the actual result. 
 *
 * Input Arguments:
 *   
 *   pSrc - pointer to the input vector 
 *   len - number of elements contained in the input and output 
 *         vectors (0 < len < 65536) 
 *
 * Output Arguments:
 *   
 *   none 
 *
 * Return Value:
 *    
 *    Maximum exponent that may be used in scaling 
 *
 */
OMX_INT omxSP_BlockExp_S32 (
    const OMX_S32 *pSrc,
    OMX_INT len
);



/**
 * Function:  omxSP_FIR_Direct_S16   (2.2.3.1.1)
 *
 * Description:
 * Block FIR filtering for 16-bit data type.  This function applies the 
 * FIR filter defined by the coefficient vector pTapsQ15 to a vector of 
 * input data.  The result is saturated with rounding if the operation 
 * produces a value outside the range of a signed 16-bit integer.  
 * Rounding behavior is defined in:
 *     section 1.6.7 "Integer Scaling and Rounding Conventions".  
 * The internal accumulator width must be at least 32 bits.  The result 
 * is undefined if any of the partially accumulated values exceeds the 
 * range of a signed 32-bit integer. 
 *
 *
 * Input Arguments:
 *   
 *   pSrc   - pointer to the vector of input samples to which the 
 *            filter is applied 
 *   sampLen - the number of samples contained in the input and output 
 *            vectors 
 *   pTapsQ15 - pointer to the vector that contains the filter coefficients, 
 *            represented in Q0.15 format (defined in section 1.6.5). Given 
 *            that:
 *                    -32768 = pTapsQ15(k) < 32768, 
 *                     0 = k <tapsLen, 
 *            the range on the actual filter coefficients is -1 = bK <1, and 
 *            therefore coefficient normalization may be required during the 
 *            filter design process. 
 *   tapsLen - the number of taps, or, equivalently, the filter order + 1 
 *   pDelayLine - pointer to the 2.tapsLen -element filter memory buffer 
 *            (state). The user is responsible for allocation, initialization, 
 *            and de-allocation. The filter memory elements are initialized to 
 *            zero in most applications. 
 *   pDelayLineIndex - pointer to the filter memory index that is maintained 
 *            internally by the function. The user should initialize the value 
 *            of this index to zero. 
 *
 * Output Arguments:
 *   
 *   pDst   - pointer to the vector of filtered output samples 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -   One or more of the following pointers is NULL: 
 *          -  pSrc, 
 *          -  pDst, 
 *          -  pSrcDst, 
 *          -  pTapsQ15, 
 *          -  pDelayLine, or 
 *          -  pDelayLineIndex 
 *    -   samplen < 0 
 *    -   tapslen < 1 
 *    -   *pDelayLineIndex < 0 or *pDelayLineIndex >= (2 * tapslen). 
 *
 */
OMXResult omxSP_FIR_Direct_S16 (
    const OMX_S16 *pSrc,
    OMX_S16 *pDst,
    OMX_INT sampLen,
    const OMX_S16 *pTapsQ15,
    OMX_INT tapsLen,
    OMX_S16 *pDelayLine,
    OMX_INT *pDelayLineIndex
);



/**
 * Function:  omxSP_FIR_Direct_S16_I   (2.2.3.1.1)
 *
 * Description:
 * Block FIR filtering for 16-bit data type.  This function applies the 
 * FIR filter defined by the coefficient vector pTapsQ15 to a vector of 
 * input data.  The result is saturated with rounding if the operation 
 * produces a value outside the range of a signed 16-bit integer.  
 * Rounding behavior is defined in:
 *     section 1.6.7 "Integer Scaling and Rounding Conventions".  
 * The internal accumulator width must be at least 32 bits.  The result 
 * is undefined if any of the partially accumulated values exceeds the 
 * range of a signed 32-bit integer. 
 *
 * Input Arguments:
 *   
 *   pSrcDst - pointer to the vector of input samples to which the 
 *            filter is applied 
 *   sampLen - the number of samples contained in the input and output 
 *            vectors 
 *   pTapsQ15 - pointer to the vector that contains the filter coefficients, 
 *            represented in Q0.15 format (defined in section 1.6.5). Given 
 *            that:
 *                    -32768 = pTapsQ15(k) < 32768, 
 *                     0 = k <tapsLen, 
 *            the range on the actual filter coefficients is -1 = bK <1, and 
 *            therefore coefficient normalization may be required during the 
 *            filter design process. 
 *   tapsLen - the number of taps, or, equivalently, the filter order + 1 
 *   pDelayLine - pointer to the 2.tapsLen -element filter memory buffer 
 *            (state). The user is responsible for allocation, initialization, 
 *            and de-allocation. The filter memory elements are initialized to 
 *            zero in most applications. 
 *   pDelayLineIndex - pointer to the filter memory index that is maintained 
 *            internally by the function. The user should initialize the value 
 *            of this index to zero. 
 *
 * Output Arguments:
 *   
 *   pSrcDst - pointer to the vector of filtered output samples 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -   One or more of the following pointers is NULL: 
 *          -  pSrc, 
 *          -  pDst, 
 *          -  pSrcDst, 
 *          -  pTapsQ15, 
 *          -  pDelayLine, or 
 *          -  pDelayLineIndex 
 *    -   samplen < 0 
 *    -   tapslen < 1 
 *    -   *pDelayLineIndex < 0 or *pDelayLineIndex >= (2 * tapslen). 
 *
 */
OMXResult omxSP_FIR_Direct_S16_I (
    OMX_S16 *pSrcDst,
    OMX_INT sampLen,
    const OMX_S16 *pTapsQ15,
    OMX_INT tapsLen,
    OMX_S16 *pDelayLine,
    OMX_INT *pDelayLineIndex
);



/**
 * Function:  omxSP_FIR_Direct_S16_Sfs   (2.2.3.1.1)
 *
 * Description:
 * Block FIR filtering for 16-bit data type. This function applies 
 * the FIR filter defined by the coefficient vector pTapsQ15 to a 
 * vector of input data.  The output is multiplied by 2 to the negative 
 * power of scalefactor (i.e., 2^-scalefactor) before returning to the caller.
 * Scaling and rounding conventions are defined in section 1.6.7.  
 * The internal accumulator width must be at least 32 bits.  
 * The result is undefined if any of the partially accumulated 
 * values exceeds the range of a signed 32-bit integer. 
 *
 * Input Arguments:
 *   
 *   pSrc    - pointer to the vector of input samples to which the 
 *            filter is applied 
 *   sampLen - the number of samples contained in the input and output 
 *            vectors 
 *   pTapsQ15 - pointer to the vector that contains the filter coefficients, 
 *            represented in Q0.15 format (defined in section 1.6.5). Given 
 *            that:
 *                    -32768 = pTapsQ15(k) < 32768, 
 *                     0 = k <tapsLen, 
 *            the range on the actual filter coefficients is -1 = bK <1, and 
 *            therefore coefficient normalization may be required during the 
 *            filter design process. 
 *   tapsLen - the number of taps, or, equivalently, the filter order + 1 
 *   pDelayLine - pointer to the 2.tapsLen -element filter memory buffer 
 *            (state). The user is responsible for allocation, initialization, 
 *            and de-allocation. The filter memory elements are initialized to 
 *            zero in most applications. 
 *   pDelayLineIndex - pointer to the filter memory index that is maintained 
 *            internally by the function. The user should initialize the value 
 *            of this index to zero. 
 *   scaleFactor - saturation fixed scalefactor
 *
 * Output Arguments:
 *   
 *   pDst  - pointer to the vector of filtered output samples 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -   One or more of the following pointers is NULL: 
 *          -  pSrc, 
 *          -  pDst, 
 *          -  pSrcDst, 
 *          -  pTapsQ15, 
 *          -  pDelayLine, or 
 *          -  pDelayLineIndex 
 *    -   samplen < 0 
 *    -   tapslen < 1 
 *    -   scaleFactor < 0 
 *    -   *pDelayLineIndex < 0 or *pDelayLineIndex >= (2 * tapslen). 
 *
 */
OMXResult omxSP_FIR_Direct_S16_Sfs (
    const OMX_S16 *pSrc,
    OMX_S16 *pDst,
    OMX_INT sampLen,
    const OMX_S16 *pTapsQ15,
    OMX_INT tapsLen,
    OMX_S16 *pDelayLine,
    OMX_INT *pDelayLineIndex,
    OMX_INT scaleFactor
);



/**
 * Function:  omxSP_FIR_Direct_S16_ISfs   (2.2.3.1.1)
 *
 * Description:
 * Block FIR filtering for 16-bit data type. This function applies 
 * the FIR filter defined by the coefficient vector pTapsQ15 to a 
 * vector of input data.  The output is multiplied by 2 to the negative 
 * power of scalefactor (i.e., 2^-scalefactor) before returning to the caller.
 * Scaling and rounding conventions are defined in section 1.6.7.  
 * The internal accumulator width must be at least 32 bits.  
 * The result is undefined if any of the partially accumulated 
 * values exceeds the range of a signed 32-bit integer. 
 *
 * Input Arguments:
 *   
 *   pSrcDst - pointer to the vector of input samples to which the 
 *            filter is applied 
 *   sampLen - the number of samples contained in the input and output 
 *            vectors 
 *   pTapsQ15 - pointer to the vector that contains the filter coefficients, 
 *            represented in Q0.15 format (defined in section 1.6.5). Given 
 *            that:
 *                    -32768 = pTapsQ15(k) < 32768, 
 *                     0 = k <tapsLen, 
 *            the range on the actual filter coefficients is -1 = bK <1, and 
 *            therefore coefficient normalization may be required during the 
 *            filter design process. 
 *   tapsLen - the number of taps, or, equivalently, the filter order + 1 
 *   pDelayLine - pointer to the 2.tapsLen -element filter memory buffer 
 *            (state). The user is responsible for allocation, initialization, 
 *            and de-allocation. The filter memory elements are initialized to 
 *            zero in most applications. 
 *   pDelayLineIndex - pointer to the filter memory index that is maintained 
 *            internally by the function. The user should initialize the value 
 *            of this index to zero. 
 *   scaleFactor - saturation fixed scalefactor
 *
 * Output Arguments:
 *   
 *   pSrcDst - pointer to the vector of filtered output samples 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -   One or more of the following pointers is NULL: 
 *          -  pSrc, 
 *          -  pDst, 
 *          -  pSrcDst, 
 *          -  pTapsQ15, 
 *          -  pDelayLine, or 
 *          -  pDelayLineIndex 
 *    -   samplen < 0 
 *    -   tapslen < 1 
 *    -   scaleFactor < 0 
 *    -   *pDelayLineIndex < 0 or *pDelayLineIndex >= (2 * tapslen). 
 *
 */
OMXResult omxSP_FIR_Direct_S16_ISfs (
    OMX_S16 *pSrcDst,
    OMX_INT sampLen,
    const OMX_S16 *pTapsQ15,
    OMX_INT tapsLen,
    OMX_S16 *pDelayLine,
    OMX_INT *pDelayLineIndex,
    OMX_INT scaleFactor
);



/**
 * Function:  omxSP_FIROne_Direct_S16   (2.2.3.1.2)
 *
 * Description:
 * Single-sample FIR filtering for 16-bit data type. These functions apply 
 * the FIR filter defined by the coefficient vector pTapsQ15 to a single 
 * sample of input data. The result is saturated with rounding if the 
 * operation produces a value outside the range of a signed 16-bit integer.  
 * Rounding behavior is defined in:
 *       section 1.6.7 "Integer Scaling and Rounding Conventions".  
 * The internal accumulator width must be at least 32 bits.  The result is 
 * undefined if any of the partially accumulated values exceeds the range of a 
 * signed 32-bit integer.
 *
 * Input Arguments:
 *   
 *   val      - the single input sample to which the filter is 
 *            applied.
 *   pTapsQ15 - pointer to the vector that contains the filter coefficients, 
 *            represented in Q0.15 format (as defined in section 1.6.5). Given 
 *            that:
 *                    -32768 = pTapsQ15(k) < 32768, 
 *                         0 = k < tapsLen, 
 *            the range on the actual filter coefficients is -1 = bK <1, and 
 *            therefore coefficient normalization may be required during the 
 *            filter design process. 
 *   tapsLen - the number of taps, or, equivalently, the filter order + 1 
 *   pDelayLine - pointer to the 2.tapsLen -element filter memory buffer 
 *            (state). The user is responsible for allocation, initialization, 
 *            and de-allocation. The filter memory elements are initialized to 
 *            zero in most applications. 
 *   pDelayLineIndex - pointer to the filter memory index that is maintained 
 *            internally by the function. The user should initialize the value 
 *            of this index to zero. 
 *
 * Output Arguments:
 *   
 *   pResult - pointer to the filtered output sample 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -    One or more of the following pointers is NULL: 
 *            -  pResult, 
 *            -  pTapsQ15, 
 *            -  pDelayLine, or 
 *            -  pDelayLineIndex 
 *    -    tapslen < 1 
 *    -    *pDelayLineIndex < 0 or *pDelayLineIndex >= (2 * tapslen) 
 *
 */
OMXResult omxSP_FIROne_Direct_S16 (
    OMX_S16 val,
    OMX_S16 *pResult,
    const OMX_S16 *pTapsQ15,
    OMX_INT tapsLen,
    OMX_S16 *pDelayLine,
    OMX_INT *pDelayLineIndex
);



/**
 * Function:  omxSP_FIROne_Direct_S16_I   (2.2.3.1.2)
 *
 * Description:
 * Single-sample FIR filtering for 16-bit data type. These functions apply 
 * the FIR filter defined by the coefficient vector pTapsQ15 to a single 
 * sample of input data. The result is saturated with rounding if the 
 * operation produces a value outside the range of a signed 16-bit integer.  
 * Rounding behavior is defined in:
 *       section 1.6.7 "Integer Scaling and Rounding Conventions".  
 * The internal accumulator width must be at least 32 bits.  The result is 
 * undefined if any of the partially accumulated values exceeds the range of a 
 * signed 32-bit integer.
 *
 * Input Arguments:
 *   
 *   pValResult - pointer to the single input sample to which the filter is 
 *            applied. 
 *   pTapsQ15 - pointer to the vector that contains the filter coefficients, 
 *            represented in Q0.15 format (as defined in section 1.6.5). Given 
 *            that:
 *                    -32768 = pTapsQ15(k) < 32768, 
 *                         0 = k < tapsLen, 
 *            the range on the actual filter coefficients is -1 = bK <1, and 
 *            therefore coefficient normalization may be required during the 
 *            filter design process. 
 *   tapsLen - the number of taps, or, equivalently, the filter order + 1 
 *   pDelayLine - pointer to the 2.tapsLen -element filter memory buffer 
 *            (state). The user is responsible for allocation, initialization, 
 *            and de-allocation. The filter memory elements are initialized to 
 *            zero in most applications. 
 *   pDelayLineIndex - pointer to the filter memory index that is maintained 
 *            internally by the function. The user should initialize the value 
 *            of this index to zero. 
 *
 * Output Arguments:
 *   
 *   pValResult - pointer to the filtered output sample 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -    One or more of the following pointers is NULL: 
 *            -  pValResult, 
 *            -  pTapsQ15, 
 *            -  pDelayLine, or 
 *            -  pDelayLineIndex 
 *    -    tapslen < 1 
 *    -    *pDelayLineIndex < 0 or *pDelayLineIndex >= (2 * tapslen) 
 *
 */
OMXResult omxSP_FIROne_Direct_S16_I (
    OMX_S16 *pValResult,
    const OMX_S16 *pTapsQ15,
    OMX_INT tapsLen,
    OMX_S16 *pDelayLine,
    OMX_INT *pDelayLineIndex
);



/**
 * Function:  omxSP_FIROne_Direct_S16_Sfs   (2.2.3.1.2)
 *
 * Description:
 * Single-sample FIR filtering for 16-bit data type. These functions apply 
 * the FIR filter defined by the coefficient vector pTapsQ15 to a single 
 * sample of input data. The output is multiplied by 2 to the negative power 
 * of scalefactor (i.e., 2^-scalefactor) before returning to the user.  
 * Scaling and rounding conventions are defined in section 1.6.7.  
 * The internal accumulator width must be at least 32 bits.  
 * The result is undefined if any of the partially accumulated values exceeds 
 * the range of a signed 32-bit integer. 
 *
 * Input Arguments:
 *   
 *   val      - the single input sample to which the filter is 
 *            applied.  
 *   pTapsQ15 - pointer to the vector that contains the filter coefficients, 
 *            represented in Q0.15 format (as defined in section 1.6.5). Given 
 *            that:
 *                    -32768 = pTapsQ15(k) < 32768, 
 *                         0 = k < tapsLen, 
 *            the range on the actual filter coefficients is -1 = bK <1, and 
 *            therefore coefficient normalization may be required during the 
 *            filter design process. 
 *   tapsLen - the number of taps, or, equivalently, the filter order + 1 
 *   pDelayLine - pointer to the 2.tapsLen -element filter memory buffer 
 *            (state). The user is responsible for allocation, initialization, 
 *            and de-allocation. The filter memory elements are initialized to 
 *            zero in most applications. 
 *   pDelayLineIndex - pointer to the filter memory index that is maintained 
 *            internally by the function. The user should initialize the value 
 *            of this index to zero. 
 *   scaleFactor - saturation fixed scaleFactor 
 *
 * Output Arguments:
 *   
 *   pResult - pointer to the filtered output sample 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -    One or more of the following pointers is NULL: 
 *            -  pResult, 
 *            -  pTapsQ15, 
 *            -  pDelayLine, or 
 *            -  pDelayLineIndex 
 *    -    tapslen < 1 
 *    -    scaleFactor < 0 
 *    -    *pDelayLineIndex < 0 or *pDelayLineIndex >= (2 * tapslen) 
 *
 */
OMXResult omxSP_FIROne_Direct_S16_Sfs (
    OMX_S16 val,
    OMX_S16 *pResult,
    const OMX_S16 *pTapsQ15,
    OMX_INT tapsLen,
    OMX_S16 *pDelayLine,
    OMX_INT *pDelayLineIndex,
    OMX_INT scaleFactor
);



/**
 * Function:  omxSP_FIROne_Direct_S16_ISfs   (2.2.3.1.2)
 *
 * Description:
 * Single-sample FIR filtering for 16-bit data type. These functions apply 
 * the FIR filter defined by the coefficient vector pTapsQ15 to a single 
 * sample of input data. The output is multiplied by 2 to the negative power 
 * of scalefactor (i.e., 2^-scalefactor) before returning to the user.  
 * Scaling and rounding conventions are defined in section 1.6.7.  
 * The internal accumulator width must be at least 32 bits.  
 * The result is undefined if any of the partially accumulated values exceeds 
 * the range of a signed 32-bit integer. 
 *
 * Input Arguments:
 *   
 *   pValResult - the pointer to a single input sample to which the filter is 
 *            applied. 
 *   pTapsQ15 - pointer to the vector that contains the filter coefficients, 
 *            represented in Q0.15 format (as defined in section 1.6.5). Given 
 *            that:
 *                    -32768 = pTapsQ15(k) < 32768, 
 *                         0 = k < tapsLen, 
 *            the range on the actual filter coefficients is -1 = bK <1, and 
 *            therefore coefficient normalization may be required during the 
 *            filter design process. 
 *   tapsLen - the number of taps, or, equivalently, the filter order + 1 
 *   pDelayLine - pointer to the 2.tapsLen -element filter memory buffer 
 *            (state). The user is responsible for allocation, initialization, 
 *            and de-allocation. The filter memory elements are initialized to 
 *            zero in most applications. 
 *   pDelayLineIndex - pointer to the filter memory index that is maintained 
 *            internally by the function. The user should initialize the value 
 *            of this index to zero. 
 *   scaleFactor - saturation fixed scaleFactor 
 *
 * Output Arguments:
 *   
 *   pValResult - pointer to the filtered output sample 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -    One or more of the following pointers is NULL: 
 *            -  pValResult, 
 *            -  pTapsQ15, 
 *            -  pDelayLine, or 
 *            -  pDelayLineIndex 
 *    -    tapslen < 1 
 *    -    scaleFactor < 0 
 *    -    *pDelayLineIndex < 0 or *pDelayLineIndex >= (2 * tapslen) 
 *
 */
OMXResult omxSP_FIROne_Direct_S16_ISfs (
    OMX_S16 *pValResult,
    const OMX_S16 *pTapsQ15,
    OMX_INT tapsLen,
    OMX_S16 *pDelayLine,
    OMX_INT *pDelayLineIndex,
    OMX_INT scaleFactor
);



/**
 * Function:  omxSP_IIR_Direct_S16   (2.2.3.2.1)
 *
 * Description:
 * Block IIR filtering for 16-bit data. This function applies the direct form 
 * II IIR filter defined by the coefficient vector pTaps to a vector of input 
 * data.  The internal accumulator width must be at least 32 bits, and the 
 * result is saturated if the operation produces a value outside the range of 
 * a signed 16-bit integer, i.e., the output will saturate to 0x8000 (-32768) 
 * for a negative overflow or 0x7fff (32767) for a positive overflow.  The 
 * result is undefined if any of the partially accumulated values exceeds the 
 * range of a signed 32-bit integer. 
 *
 * Input Arguments:
 *   
 *   pSrc  - pointer to the vector of input samples to which the 
 *            filter is applied 
 *   len - the number of samples contained in both the input and output 
 *            vectors 
 *   pTaps - pointer to the 2L+2-element vector that contains the combined 
 *            numerator and denominator filter coefficients from the system 
 *            transfer function, H(z). Coefficient scaling and coefficient 
 *            vector organization should follow the conventions described 
 *            above. The value of the coefficient scaleFactor exponent must be 
 *            non-negative (sf=0). 
 *   order - the maximum of the degrees of the numerator and denominator 
 *            coefficient polynomials from the system transfer function, H(z). 
 *            In the notation of section 2.2.3.2, the parameter 
 *            order=max(K,M)=L gives the maximum delay, in samples, used to 
 *            compute each output sample. 
 *   pDelayLine - pointer to the L-element filter memory buffer (state). The 
 *            user is responsible for allocation, initialization, and 
 *            deallocation. The filter memory elements are initialized to zero 
 *            in most applications. 
 *
 * Output Arguments:
 *   
 *   pDst - pointer to the vector of filtered output samples 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -    one or more of the following pointers is NULL: 
 *             -  pSrc, 
 *             -  pDst, 
 *             -  pTaps, or 
 *             -  pDelayLine. 
 *    -    len < 0 
 *    -    pTaps[order+1] < 0 (negative scaling) 
 *    -    order < 1 
 *
 */
OMXResult omxSP_IIR_Direct_S16 (
    const OMX_S16 *pSrc,
    OMX_S16 *pDst,
    OMX_INT len,
    const OMX_S16 *pTaps,
    OMX_INT order,
    OMX_S32 *pDelayLine
);



/**
 * Function:  omxSP_IIR_Direct_S16_I   (2.2.3.2.1)
 *
 * Description:
 * Block IIR filtering for 16-bit data. This function applies the direct form 
 * II IIR filter defined by the coefficient vector pTaps to a vector of input 
 * data.  The internal accumulator width must be at least 32 bits, and the 
 * result is saturated if the operation produces a value outside the range of 
 * a signed 16-bit integer, i.e., the output will saturate to 0x8000 (-32768) 
 * for a negative overflow or 0x7fff (32767) for a positive overflow.  The 
 * result is undefined if any of the partially accumulated values exceeds the 
 * range of a signed 32-bit integer. 
 *
 * Input Arguments:
 *   
 *   pSrcDst - pointer to the vector of input samples to which the 
 *            filter is applied 
 *   len - the number of samples contained in both the input and output 
 *            vectors 
 *   pTaps - pointer to the 2L+2-element vector that contains the combined 
 *            numerator and denominator filter coefficients from the system 
 *            transfer function, H(z). Coefficient scaling and coefficient 
 *            vector organization should follow the conventions described 
 *            above. The value of the coefficient scaleFactor exponent must be 
 *            non-negative (sf>=0). 
 *   order - the maximum of the degrees of the numerator and denominator 
 *            coefficient polynomials from the system transfer function, H(z). 
 *            In the notation of section 2.2.3.2, the parameter 
 *            order=max(K,M)=L gives the maximum delay, in samples, used to 
 *            compute each output sample. 
 *   pDelayLine - pointer to the L-element filter memory buffer (state). The 
 *            user is responsible for allocation, initialization, and 
 *            deallocation. The filter memory elements are initialized to zero 
 *            in most applications. 
 *
 * Output Arguments:
 *   
 *   pSrcDst - pointer to the vector of filtered output samples 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -    one or more of the following pointers is NULL: 
 *             -  pSrcDst, 
 *             -  pTaps, or 
 *             -  pDelayLine. 
 *    -    len < 0 
 *    -    pTaps[order+1] < 0 (negative scaling) 
 *    -    order < 1 
 *
 */
OMXResult omxSP_IIR_Direct_S16_I (
    OMX_S16 *pSrcDst,
    OMX_INT len,
    const OMX_S16 *pTaps,
    OMX_INT order,
    OMX_S32 *pDelayLine
);



/**
 * Function:  omxSP_IIROne_Direct_S16   (2.2.3.2.2)
 *
 * Description:
 * Single sample IIR filtering for 16-bit data.  This function applies the 
 * direct form II IIR filter defined by the coefficient vector pTaps to a 
 * single sample of input data. The internal accumulator width must be at 
 * least 32 bits, and the result is saturated if the operation produces a 
 * value outside the range of a signed 16-bit integer, i.e., the output will 
 * saturate to 0x8000 (-32768) for a negative overflow or 0x7fff (32767) for a 
 * positive overflow.  The result is undefined if any of the partially 
 * accumulated values exceeds the range of a signed 32-bit integer. 
 *
 * Input Arguments:
 *   
 *   val - the single input sample to which the filter is 
 *            applied.  
 *   pTaps - pointer to the 2L+2 -element vector that contains the combined 
 *            numerator and denominator filter coefficients from the system 
 *            transfer function, H(z). Coefficient scaling and coefficient 
 *            vector organization should follow the conventions described 
 *            above. The value of the coefficient scaleFactor exponent must be 
 *            non-negative (sf>=0). 
 *   order - the maximum of the degrees of the numerator and denominator 
 *            coefficient polynomials from the system transfer function, H(z). 
 *            In the notation of section 2.2.3.2, the parameter 
 *            order=max(K,M)=L gives the maximum delay, in samples, used to 
 *            compute each output sample. 
 *   pDelayLine - pointer to the L-element filter memory buffer (state). The 
 *            user is responsible for allocation, initialization, and 
 *            deallocation. The filter memory elements are initialized to zero 
 *            in most applications. 
 *
 * Output Arguments:
 *   
 *   pResult - pointer to the filtered output sample 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -    one or more of the following pointers is NULL: pResult, 
 *              pTaps, or pDelayLine. 
 *    -    order < 1 
 *    -    pTaps[order+1] < 0 (negative scaling) 
 *
 */
OMXResult omxSP_IIROne_Direct_S16 (
    OMX_S16 val,
    OMX_S16 *pResult,
    const OMX_S16 *pTaps,
    OMX_INT order,
    OMX_S32 *pDelayLine
);



/**
 * Function:  omxSP_IIROne_Direct_S16_I   (2.2.3.2.2)
 *
 * Description:
 * Single sample IIR filtering for 16-bit data.  This function applies the 
 * direct form II IIR filter defined by the coefficient vector pTaps to a 
 * single sample of input data. The internal accumulator width must be at 
 * least 32 bits, and the result is saturated if the operation produces a 
 * value outside the range of a signed 16-bit integer, i.e., the output will 
 * saturate to 0x8000 (-32768) for a negative overflow or 0x7fff (32767) for a 
 * positive overflow.  The result is undefined if any of the partially 
 * accumulated values exceeds the range of a signed 32-bit integer. 
 *
 * Input Arguments:
 *   
 *   pValResult - pointer to the single input sample to which the filter is 
 *            applied.
 *   pTaps - pointer to the 2L+2 -element vector that contains the combined 
 *            numerator and denominator filter coefficients from the system 
 *            transfer function, H(z). Coefficient scaling and coefficient 
 *            vector organization should follow the conventions described 
 *            above. The value of the coefficient scaleFactor exponent must be 
 *            non-negative (sf>=0). 
 *   order - the maximum of the degrees of the numerator and denominator 
 *            coefficient polynomials from the system transfer function, H(z). 
 *            In the notation of section 2.2.3.2, the parameter 
 *            order=max(K,M)=L gives the maximum delay, in samples, used to 
 *            compute each output sample. 
 *   pDelayLine - pointer to the L-element filter memory buffer (state). The 
 *            user is responsible for allocation, initialization, and 
 *            deallocation. The filter memory elements are initialized to zero 
 *            in most applications. 
 *
 * Output Arguments:
 *   
 *   pValResult - pointer to the filtered output sample 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -    one or more of the following pointers is NULL:  
 *              pValResult, pTaps, or pDelayLine. 
 *    -    order < 1 
 *    -    pTaps[order+1] < 0 (negative scaling) 
 *
 */
OMXResult omxSP_IIROne_Direct_S16_I (
    OMX_S16 *pValResult,
    const OMX_S16 *pTaps,
    OMX_INT order,
    OMX_S32 *pDelayLine
);



/**
 * Function:  omxSP_IIR_BiQuadDirect_S16   (2.2.3.3.1)
 *
 * Description:
 * Block biquad IIR filtering for 16-bit data type. This function applies the 
 * direct form II biquad IIR cascade defined by the coefficient vector pTaps 
 * to a vector of input data.  The internal accumulator width must be at least 
 * 32 bits, and the result is saturated if the operation produces a value 
 * outside the range of a signed 16-bit integer, i.e., the output will 
 * saturate to 0x8000 (-32768) for a negative overflow or 0x7fff (32767) for a 
 * positive overflow.  The result is undefined if any of the partially 
 * accumulated values exceeds the range of a signed 32-bit integer. 
 *
 * Input Arguments:
 *   
 *   pSrc - pointer to the vector of input samples to which the 
 *            filter is applied 
 *   len - the number of samples contained in both the input and output 
 *            vectors 
 *   pTaps - pointer to the 6P -element vector that contains the combined 
 *            numerator and denominator filter coefficients from the biquad 
 *            cascade. Coefficient scaling and coefficient vector organization 
 *            should follow the conventions described above. The value of the 
 *            coefficient scaleFactor exponent must be non-negative. (sfp>=0). 
 *   numBiquad - the number of biquads contained in the IIR filter cascade: 
 *            (P) 
 *   pDelayLine - pointer to the 2P -element filter memory buffer (state). 
 *            The user is responsible for allocation, initialization, and 
 *            de-allocation. The filter memory elements are initialized to 
 *            zero in most applications. 
 *
 * Output Arguments:
 *   
 *   pDst - pointer to the vector of filtered output samples 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -    one or more of the following pointers is NULL: pSrc, pDst, 
 *              pTaps, or pDelayLine. 
 *    -    len < 0 
 *    -    numBiquad < 1 
 *    -    pTaps[3+n*6] < 0, for 0 <= n < numBiquad (negative scaling) 
 *
 */
OMXResult omxSP_IIR_BiQuadDirect_S16 (
    const OMX_S16 *pSrc,
    OMX_S16 *pDst,
    OMX_INT len,
    const OMX_S16 *pTaps,
    OMX_INT numBiquad,
    OMX_S32 *pDelayLine
);



/**
 * Function:  omxSP_IIR_BiQuadDirect_S16_I   (2.2.3.3.1)
 *
 * Description:
 * Block biquad IIR filtering for 16-bit data type. This function applies the 
 * direct form II biquad IIR cascade defined by the coefficient vector pTaps 
 * to a vector of input data.  The internal accumulator width must be at least 
 * 32 bits, and the result is saturated if the operation produces a value 
 * outside the range of a signed 16-bit integer, i.e., the output will 
 * saturate to 0x8000 (-32768) for a negative overflow or 0x7fff (32767) for a 
 * positive overflow.  The result is undefined if any of the partially 
 * accumulated values exceeds the range of a signed 32-bit integer. 
 *
 * Input Arguments:
 *   
 *   pSrcDst - pointer to the vector of input samples to which the 
 *            filter is applied 
 *   len - the number of samples contained in both the input and output 
 *            vectors 
 *   pTaps - pointer to the 6P -element vector that contains the combined 
 *            numerator and denominator filter coefficients from the biquad 
 *            cascade. Coefficient scaling and coefficient vector organization 
 *            should follow the conventions described above. The value of the 
 *            coefficient scaleFactor exponent must be non-negative. (sfp>=0). 
 *   numBiquad - the number of biquads contained in the IIR filter cascade: 
 *            (P) 
 *   pDelayLine - pointer to the 2P -element filter memory buffer (state). 
 *            The user is responsible for allocation, initialization, and 
 *            de-allocation. The filter memory elements are initialized to 
 *            zero in most applications. 
 *
 * Output Arguments:
 *   
 *   pSrcDst - pointer to the vector of filtered output samples 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -    one or more of the following pointers is NULL: 
 *              pSrcDst, pTaps, or pDelayLine. 
 *    -    len < 0 
 *    -    numBiquad < 1 
 *    -    pTaps[3+n*6] < 0, for 0 <= n < numBiquad (negative scaling) 
 *
 */
OMXResult omxSP_IIR_BiQuadDirect_S16_I (
    OMX_S16 *pSrcDst,
    OMX_INT len,
    const OMX_S16 *pTaps,
    OMX_INT numBiquad,
    OMX_S32 *pDelayLine
);



/**
 * Function:  omxSP_IIROne_BiQuadDirect_S16   (2.2.3.3.2)
 *
 * Description:
 * Single-sample biquad IIR filtering for 16-bit data type. This function 
 * applies the direct form II biquad IIR cascade defined by the coefficient 
 * vector pTaps to a single sample of input data.  The internal accumulator 
 * width must be at least 32 bits, and the result is saturated if the 
 * operation produces a value outside the range of a signed 16-bit integer, 
 * i.e., the output will saturate to 0x8000 (-32768) for a negative overflow 
 * or 0x7fff (32767) for a positive overflow.  The result is undefined if any 
 * of the partially accumulated values exceeds the range of a signed 32-bit 
 * integer. 
 *
 * Input Arguments:
 *   
 *   val   - the single input sample to which the filter is 
 *            applied. 
 *   pTaps - pointer to the 6P-element vector that contains the combined 
 *            numerator and denominator filter coefficients from the biquad 
 *            cascade. Coefficient scaling and coefficient vector organization 
 *            should follow the conventions described above. The value of the 
 *            coefficient scalefactor exponent must be non-negative: (sfp>=0). 
 *   numBiquad - the number of biquads contained in the IIR filter cascade: 
 *            (P) 
 *   pDelayLine - pointer to the 2p-element filter memory buffer (state). The 
 *            user is responsible for allocation, initialization, and 
 *            deallocation. The filter memory elements are initialized to zero 
 *            in most applications. 
 *
 * Output Arguments:
 *   
 *   pResult - pointer to the filtered output sample 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -    one or more of the following pointers is NULL: pResult, 
 *              pValResult, pTaps, or pDelayLine. 
 *    -    numBiquad < 1 
 *    -    pTaps[3+n*6] < 0, for 0 <= n < numBiquad (negative scaling) 
 *
 */
OMXResult omxSP_IIROne_BiQuadDirect_S16 (
    OMX_S16 val,
    OMX_S16 *pResult,
    const OMX_S16 *pTaps,
    OMX_INT numBiquad,
    OMX_S32 *pDelayLine
);



/**
 * Function:  omxSP_IIROne_BiQuadDirect_S16_I   (2.2.3.3.2)
 *
 * Description:
 * Single-sample biquad IIR filtering for 16-bit data type. This function 
 * applies the direct form II biquad IIR cascade defined by the coefficient 
 * vector pTaps to a single sample of input data.  The internal accumulator 
 * width must be at least 32 bits, and the result is saturated if the 
 * operation produces a value outside the range of a signed 16-bit integer, 
 * i.e., the output will saturate to 0x8000 (-32768) for a negative overflow 
 * or 0x7fff (32767) for a positive overflow.  The result is undefined if any 
 * of the partially accumulated values exceeds the range of a signed 32-bit 
 * integer. 
 *
 * Input Arguments:
 *   
 *   pValResult - pointer to the single input sample to which the filter is 
 *            applied. 
 *   pTaps - pointer to the 6P-element vector that contains the combined 
 *            numerator and denominator filter coefficients from the biquad 
 *            cascade. Coefficient scaling and coefficient vector organization 
 *            should follow the conventions described above. The value of the 
 *            coefficient scalefactor exponent must be non-negative: (sfp>=0). 
 *   numBiquad - the number of biquads contained in the IIR filter cascade: 
 *            (P) 
 *   pDelayLine - pointer to the 2p-element filter memory buffer (state). The 
 *            user is responsible for allocation, initialization, and 
 *            deallocation. The filter memory elements are initialized to zero 
 *            in most applications. 
 *
 * Output Arguments:
 *   
 *   pValResult - pointer to the filtered output sample 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -    one or more of the following pointers is NULL:
 *              pValResult, pTaps, or pDelayLine. 
 *    -    numBiquad < 1 
 *    -    pTaps[3+n*6] < 0, for 0 <= n < numBiquad (negative scaling) 
 *
 */
OMXResult omxSP_IIROne_BiQuadDirect_S16_I (
    OMX_S16 *pValResult,
    const OMX_S16 *pTaps,
    OMX_INT numBiquad,
    OMX_S32 *pDelayLine
);



/**
 * Function:  omxSP_FilterMedian_S32   (2.2.3.4.1)
 *
 * Description:
 * This function computes the median over the region specified by the median 
 * mask for the every element of the input array. The median outputs are 
 * stored in the corresponding elements of the output vector. 
 *
 * Input Arguments:
 *   
 *   pSrc - pointer to the input vector 
 *   len - number of elements contained in the input and output vectors (0 < 
 *            len < 65536) 
 *   maskSize - median mask size; if an even value is specified, the function 
 *            subtracts 1 and uses the odd value of the filter mask for median 
 *            filtering (0 < maskSize < 256) 
 *
 * Output Arguments:
 *   
 *   pDst - pointer to the median-filtered output vector 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -   one or more of the following pointers is NULL: pSrc, pDst. 
 *    -    len < 0 
 *    -    maskSize < 1 or maskSize> 255 
 *    OMX_StsSP_EvenMedianMaskSizeErr - even mask size replaced by odd mask 
 *              size 
 *
 */
OMXResult omxSP_FilterMedian_S32 (
    const OMX_S32 *pSrc,
    OMX_S32 *pDst,
    OMX_INT len,
    OMX_INT maskSize
);



/**
 * Function:  omxSP_FilterMedian_S32_I   (2.2.3.4.1)
 *
 * Description:
 * This function computes the median over the region specified by the median 
 * mask for the every element of the input array. The median outputs are 
 * stored in the corresponding elements of the output vector. 
 *
 * Input Arguments:
 *   
 *   pSrcDst - pointer to the input vector 
 *   len - number of elements contained in the input and output vectors (0 < 
 *            len < 65536) 
 *   maskSize - median mask size; if an even value is specified, the function 
 *            subtracts 1 and uses the odd value of the filter mask for median 
 *            filtering (0 < maskSize < 256) 
 *
 * Output Arguments:
 *   
 *   pSrcDst - pointer to the median-filtered output vector 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -    pSrcDst is NULL. 
 *    -    len < 0 
 *    -    maskSize < 1 or maskSize> 255 
 *    OMX_StsSP_EvenMedianMaskSizeErr - even mask size replaced by odd mask 
 *              size 
 *
 */
OMXResult omxSP_FilterMedian_S32_I (
    OMX_S32 *pSrcDst,
    OMX_INT len,
    OMX_INT maskSize
);



/**
 * Function:  omxSP_FFTInit_C_SC16   (2.2.4.1.2)
 *
 * Description:
 * These functions initialize the specification structures required for the 
 * complex FFT and IFFT functions. Desired block length is specified as an 
 * input. The function <FFTInit_C_SC16> is used to initialize the 
 * specification structures for functions <FFTFwd_CToC_SC16_Sfs> and 
 * <FFTInv_CToC_SC16_Sfs>.
 *
 * Memory for the specification structure *pFFTSpec 
 * must be allocated prior to calling these functions and should be 4-byte 
 * aligned for omxSP_FFTInit_C_SC16. 
 *
 * The space required for *pFFTSpec, in bytes, can be 
 * determined using <FFTGetBufSize_C_SC16>. 
 *
 * Input Arguments:
 *   
 *   order - base-2 logarithm of the desired block length; 
 *           valid in the range [0,12] 
 *
 * Output Arguments:
 *   
 *   pFFTSpec - pointer to initialized specification structure 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr -no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -   pFFTSpec is either NULL or violates the 4-byte alignment 
 *              restrictions 
 *    -   order < 0 or order > 12 
 *
 */
OMXResult omxSP_FFTInit_C_SC16 (
    OMXFFTSpec_C_SC16 *pFFTSpec,
    OMX_INT order
);



/**
 * Function:  omxSP_FFTInit_C_SC32   (2.2.4.1.2)
 *
 * Description:
 * These functions initialize the specification structures required for the 
 * complex FFT and IFFT functions. Desired block length is specified as an 
 * input. The function <FFTInit_C_SC32> is used to initialize 
 * the specification structures for the functions <FFTFwd_CToC_SC32_Sfs> and 
 * <FFTInv_CToC_SC32_Sfs>.
 *
 * Memory for the specification structure *pFFTSpec must be allocated prior 
 * to calling these functions and should be 8-byte aligned for 
 * omxSP_FFTInit_C_SC32. 
 *
 * The space required for *pFFTSpec, in bytes, can be 
 * determined using <FFTGetBufSize_C_SC32>. 
 *
 * Input Arguments:
 *   
 *   order - base-2 logarithm of the desired block length; valid in the range 
 *            [0,12] 
 *
 * Output Arguments:
 *   
 *   pFFTSpec - pointer to initialized specification structure 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr -no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -   pFFTSpec is either NULL or violates the 8-byte alignment 
 *              restrictions 
 *    -   order < 0 or order > 12 
 *
 */
OMXResult omxSP_FFTInit_C_SC32 (
    OMXFFTSpec_C_SC32 *pFFTSpec,
    OMX_INT order
);

/**
 * Function:  omxSP_FFTInit_C_FC32   (2.2.4.1.2)
 *
 * Description:
 * These functions initialize the specification structures required for the 
 * complex FFT and IFFT functions. Desired block length is specified as an 
 * input. The function <FFTInit_C_FC32> is used to initialize 
 * the specification structures for the functions <FFTFwd_CToC_FC32_Sfs> and 
 * <FFTInv_CToC_FC32_Sfs>.
 *
 * Memory for the specification structure *pFFTSpec must be allocated prior 
 * to calling these functions and should be 8-byte aligned for 
 * omxSP_FFTInit_C_FC32. 
 *
 * The space required for *pFFTSpec, in bytes, can be 
 * determined using <FFTGetBufSize_C_FC32>. 
 *
 * Input Arguments:
 *   
 *   order - base-2 logarithm of the desired block length; valid in the range 
 *            [1,15] 
 *
 * Output Arguments:
 *   
 *   pFFTSpec - pointer to initialized specification structure 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr -no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -   pFFTSpec is either NULL or violates the 8-byte alignment 
 *              restrictions 
 *    -   order < 1 or order > 15
 *
 */
OMXResult omxSP_FFTInit_C_FC32(
    OMXFFTSpec_C_FC32* pFFTSpec,
    OMX_INT order
);



/**
 * Function:  omxSP_FFTInit_R_S16S32   (2.2.4.1.4)
 *
 * Description:
 * These functions initialize specification structures required for the real 
 * FFT and IFFT functions. The function <FFTInit_R_S16S32> is used to 
 * initialize the specification structures for functions 
 * <FFTFwd_RToCCS_S16S32_Sfs> and <FFTInv_CCSToR_S32S16_Sfs>.
 * 
 * Memory for 
 * *pFFTFwdSpec must be allocated before calling these functions and should be 
 * 8-byte aligned. The number of bytes required for *pFFTFwdSpec can be 
 * determined using <FFTGetBufSize_R_S16S32>. 
 *
 * Input Arguments:
 *   
 *   order - base-2 logarithm of the desired block length; valid in the range 
 *            [0,12] 
 *
 * Output Arguments:
 *   
 *   pFFTFwdSpec - pointer to the initialized specification structure 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -   pFFTFwdSpec is either NULL or violates the 8-byte alignment 
 *              restrictions 
 *    -   order < 0 or order > 12 
 *
 */
OMXResult omxSP_FFTInit_R_S16S32(
    OMXFFTSpec_R_S16S32* pFFTFwdSpec,
    OMX_INT order
);



/**
 * Function:  omxSP_FFTInit_R_S16
 *
 * Description:
 * These functions initialize specification structures required for the real 
 * FFT and IFFT functions. The function <FFTInit_R_S16> is used 
 * to initialize the specification structures for functions
 * <FFTFwd_RToCCS_S16_Sfs> and <FFTInv_CCSToR_S16_Sfs>.
 *
 * Memory for *pFFTFwdSpec must be allocated before calling these functions
 * and should be 8-byte aligned. 
 *
 * The number of bytes required for *pFFTFwdSpec can be 
 * determined using <FFTGetBufSize_R_S16>. 
 *
 * Input Arguments:
 *   
 *   order - base-2 logarithm of the desired block length; valid in the range 
 *            [1,12] 
 *
 * Output Arguments:
 *   
 *   pFFTFwdSpec - pointer to the initialized specification structure 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -   pFFTFwdSpec is either NULL or violates the 8-byte alignment 
 *              restrictions 
 *    -   order < 1 or order > 12 
 *
 */
OMXResult omxSP_FFTInit_R_S16 (
    OMXFFTSpec_R_S32*pFFTFwdSpec,
    OMX_INT order
);

/**
 * Function:  omxSP_FFTInit_R_S32   (2.2.4.1.4)
 *
 * Description:
 * These functions initialize specification structures required for the real 
 * FFT and IFFT functions. The function <FFTInit_R_S32> is used to initialize 
 * the specification structures for functions <FFTFwd_RToCCS_S32_Sfs> 
 * and <FFTInv_CCSToR_S32_Sfs>. 
 *
 * Memory for *pFFTFwdSpec must be allocated before calling these functions
 * and should be 8-byte aligned. 
 *
 * The number of bytes required for *pFFTFwdSpec can be 
 * determined using <FFTGetBufSize_R_S32>. 
 *
 * Input Arguments:
 *   
 *   order - base-2 logarithm of the desired block length; valid in the range 
 *            [0,12] 
 *
 * Output Arguments:
 *   
 *   pFFTFwdSpec - pointer to the initialized specification structure 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -   pFFTFwdSpec is either NULL or violates the 8-byte alignment 
 *              restrictions 
 *    -   order < 0 or order > 12 
 *
 */
OMXResult omxSP_FFTInit_R_S32 (
    OMXFFTSpec_R_S32*pFFTFwdSpec,
    OMX_INT order
);

/**
 * Function:  omxSP_FFTInit_R_F32
 *
 * Description:
 * These functions initialize specification structures required for the real 
 * FFT and IFFT functions. The function <FFTInit_R_F32> is used to initialize 
 * the specification structures for functions <FFTFwd_RToCCS_F32_Sfs> 
 * and <FFTInv_CCSToR_F32_Sfs>. 
 *
 * Memory for *pFFTFwdSpec must be allocated before calling these functions
 * and should be 8-byte aligned. 
 *
 * The number of bytes required for *pFFTFwdSpec can be 
 * determined using <FFTGetBufSize_R_F32>. 
 *
 * Input Arguments:
 *   
 *   order - base-2 logarithm of the desired block length; valid in the range 
 *            [1,15] 
 *
 * Output Arguments:
 *   
 *   pFFTFwdSpec - pointer to the initialized specification structure 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -   pFFTFwdSpec is either NULL or violates the 8-byte alignment 
 *              restrictions 
 *    -   order < 1 or order > 15
 *
 */
OMXResult omxSP_FFTInit_R_F32(
    OMXFFTSpec_R_F32* pFFTFwdSpec,
    OMX_INT order
);

/**
 * Function:  omxSP_FFTGetBufSize_C_SC16   (2.2.4.1.6)
 *
 * Description:
 * These functions compute the size of the specification structure 
 * required for the length 2^order complex FFT and IFFT functions. The function 
 * <FFTGetBufSize_C_SC16> is used in conjunction with the 16-bit functions 
 * <FFTFwd_CToC_SC16_Sfs> and <FFTInv_CToC_SC16_Sfs>. 
 *
 * Input Arguments:
 *   
 *   order - base-2 logarithm of the desired block length; valid in the range 
 *            [0,12] 
 *
 * Output Arguments:
 *   
 *   pSize - pointer to the number of bytes required for the specification 
 *            structure 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -    pSize is NULL 
 *    -    order < 0 or order > 12 
 *
 */
OMXResult omxSP_FFTGetBufSize_C_SC16 (
    OMX_INT order,
    OMX_INT *pSize
);



/**
 * Function:  omxSP_FFTGetBufSize_C_SC32   (2.2.4.1.6)
 *
 * Description:
 * These functions compute the size of the specification structure 
 * required for the length 2^order complex FFT and IFFT functions. The function 
 * <FFTGetBufSize_C_SC32> is used in conjunction with the 32-bit functions 
 * <FFTFwd_CToC_SC32_Sfs> and <FFTInv_CToC_SC32_Sfs>. 
 *
 * Input Arguments:
 *   
 *   order - base-2 logarithm of the desired block length; valid in the range 
 *            [0,12] 
 *
 * Output Arguments:
 *   
 *   pSize - pointer to the number of bytes required for the specification 
 *            structure 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -    pSize is NULL 
 *    -    order < 0 or order > 12 
 *
 */
OMXResult omxSP_FFTGetBufSize_C_SC32 (
    OMX_INT order,
    OMX_INT *pSize
);

/**
 * Function:  omxSP_FFTGetBufSize_C_FC32
 *
 * Description:
 * These functions compute the size of the specification structure 
 * required for the length 2^order complex FFT and IFFT functions. The function 
 * <FFTGetBufSize_C_FC32> is used in conjunction with the 32-bit functions 
 * <FFTFwd_CToC_FC32_Sfs> and <FFTInv_CToC_FC32_Sfs>. 
 *
 * Input Arguments:
 *   
 *   order - base-2 logarithm of the desired block length; valid in the range 
 *            [1,15] 
 *
 * Output Arguments:
 *   
 *   pSize - pointer to the number of bytes required for the specification 
 *            structure 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the 
 *              following is true: 
 *    -    pSize is NULL 
 *    -    order < 1 or order > 15 
 *
 */
OMXResult omxSP_FFTGetBufSize_C_FC32(
    OMX_INT order,
    OMX_INT* pSize
);


/**
 * Function:  omxSP_FFTGetBufSize_R_S16S32   (2.2.4.1.8)
 *
 * Description:
 * order These functions compute the size of the specification structure 
 * required for the length 2^order real FFT and IFFT functions. The function 
 * <FFTGetBufSize_R_S16S32> is used in conjunction with the 16-bit functions 
 * <FFTFwd_RToCCS_S16S32_Sfs> and <FFTInv_CCSToR_S32S16_Sfs>. 
 *
 * Input Arguments:
 *   
 *   order - base-2 logarithm of the length; valid in the range [0,12] 
 *
 * Output Arguments:
 *   
 *   pSize - pointer to the number of bytes required for the specification 
 *            structure 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments The function returns 
 *              OMX_Sts_BadArgErr if one or more of the following is true: 
 *    pSize is NULL 
 *    order < 0 or order > 12 
 *
 */
OMXResult omxSP_FFTGetBufSize_R_S16S32(
    OMX_INT order,
    OMX_INT* pSize
);


/**
 * Function:  omxSP_FFTGetBufSize_R_S16
 *
 * Description:
 * These functions compute the size of the specification structure 
 * required for the length 2^order real FFT and IFFT functions.  The function 
 * <FFTGetBufSize_R_S16> is used in conjunction with the 16-bit 
 * functions <FFTFwd_RToCCS_S16_Sfs> and <FFTInv_CCSToR_S16_Sfs>. 
 *
 * Input Arguments:
 *   
 *   order - base-2 logarithm of the length; valid in the range
 *   [1,12]
 *
 * Output Arguments:
 *   
 *   pSize - pointer to the number of bytes required for the specification 
 *            structure 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments The function returns 
 *              OMX_Sts_BadArgErr if one or more of the following is true: 
 *    pSize is NULL 
 *    order < 1 or order > 12 
 *
 */
OMXResult omxSP_FFTGetBufSize_R_S16 (
    OMX_INT order,
    OMX_INT *pSize
);

/**
 * Function:  omxSP_FFTGetBufSize_R_S32   (2.2.4.1.8)
 *
 * Description:
 * These functions compute the size of the specification structure 
 * required for the length 2^order real FFT and IFFT functions.  The function 
 * <FFTGetBufSize_R_S32> is used in conjunction with the 32-bit functions 
 * <FFTFwd_RToCCS_S32_Sfs> and <FFTInv_CCSToR_S32_Sfs>. 
 *
 * Input Arguments:
 *   
 *   order - base-2 logarithm of the length; valid in the range [0,12] 
 *
 * Output Arguments:
 *   
 *   pSize - pointer to the number of bytes required for the specification 
 *            structure 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments The function returns 
 *              OMX_Sts_BadArgErr if one or more of the following is true: 
 *    pSize is NULL 
 *    order < 0 or order > 12 
 *
 */
OMXResult omxSP_FFTGetBufSize_R_S32 (
    OMX_INT order,
    OMX_INT *pSize
);

/**
 * Function:  omxSP_FFTGetBufSize_R_F32
 *
 * Description:
 * These functions compute the size of the specification structure 
 * required for the length 2^order real FFT and IFFT functions.  The function 
 * <FFTGetBufSize_R_F32> is used in conjunction with the 32-bit functions 
 * <FFTFwd_RToCCS_F32_Sfs> and <FFTInv_CCSToR_F32_Sfs>. 
 *
 * Input Arguments:
 *   
 *   order - base-2 logarithm of the length; valid in the range [1,15] 
 *
 * Output Arguments:
 *   
 *   pSize - pointer to the number of bytes required for the specification 
 *            structure 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments The function returns 
 *              OMX_Sts_BadArgErr if one or more of the following is true: 
 *    pSize is NULL 
 *    order < 1 or order > 15
 *
 */
OMXResult omxSP_FFTGetBufSize_R_F32(
    OMX_INT order,
    OMX_INT* pSize
);



/**
 * Function:  omxSP_FFTFwd_CToC_SC16_Sfs   (2.2.4.2.2)
 *
 * Description:
 * Compute an FFT for a complex signal of length of 2^order, 
 * where 0 <= order <= 12. 
 * Transform length is determined by the specification structure, which 
 * must be initialized prior to calling the FFT function using the appropriate 
 * helper, i.e., <FFTInit_C_sc32> or <FFTInit_C_SC16>. The relationship 
 * between the input and output sequences can be expressed in terms of the 
 * DFT, i.e., 
 *
 *      X[k] = 2^(-scaleFactor) . SUM[n=0...N-1]x[n].e^(-jnk.2.pi/N)
 *      k = 0,1,2,..., N-1
 *      N = 2^order
 *
 * Input Arguments:
 *   pSrc - pointer to the input signal, a complex-valued vector of length 2^order; 
 *            must be aligned on a 32 byte boundary. 
 *   pFFTSpec - pointer to the preallocated and initialized specification 
 *            structure 
 *   scaleFactor - output scale factor; the range for is [0,16] 
 *
 * Output Arguments:
 *   pDst - pointer to the complex-valued output vector, of length 2^order; 
 *          must be aligned on an 32-byte boundary. 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - returned if one or more of the following conditions 
 *              is true: 
 *    -   one or more of the following pointers is NULL: pSrc, pDst, or 
 *              pFFTSpec. 
 *    -    pSrc or pDst is not 32-byte aligned 
 *    -    scaleFactor<0 or scaleFactor>16
 *
 */

OMXResult omxSP_FFTFwd_CToC_SC16_Sfs (
    const OMX_SC16 *pSrc,
    OMX_SC16 *pDst,
    const OMXFFTSpec_C_SC16 *pFFTSpec,
    OMX_INT scaleFactor
);

OMXResult omxSP_FFTFwd_CToC_SC16_Sfs_neon (
    const OMX_SC16 *pSrc,
    OMX_SC16 *pDst,
    const OMXFFTSpec_C_SC16 *pFFTSpec,
    OMX_INT scaleFactor
);

/**
 * Function:  omxSP_FFTFwd_CToC_SC32_Sfs   (2.2.4.2.2)
 *
 * Description:
 * Compute an FFT for a complex signal of length of 2^order, 
 * where 0 <= order <= 12. 
 * Transform length is determined by the specification structure, which 
 * must be initialized prior to calling the FFT function using the appropriate 
 * helper, i.e., <FFTInit_C_sc32> or <FFTInit_C_SC16>. The relationship 
 * between the input and output sequences can be expressed in terms of the 
 * DFT, i.e., 
 *
 *      X[k] = 2^(-scaleFactor) . SUM[n=0...N-1]x[n].e^(-jnk.2.pi/N)
 *      k = 0,1,2,..., N-1
 *      N = 2^order
 *
 * Input Arguments:
 *   pSrc - pointer to the input signal, a complex-valued vector of length 2^order; 
 *            must be aligned on a 32 byte boundary. 
 *   pFFTSpec - pointer to the preallocated and initialized specification 
 *            structure 
 *   scaleFactor - output scale factor; the range is [0,32] 
 *
 * Output Arguments:
 *   pDst - pointer to the complex-valued output vector, of length 2^order; must be 
 *            aligned on an 32-byte boundary. 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - returned if one or more of the following conditions 
 *              is true: 
 *    -   one or more of the following pointers is NULL: pSrc, pDst, or 
 *              pFFTSpec. 
 *    -    pSrc or pDst is not 32-byte aligned 
 *    -    scaleFactor<0 or scaleFactor >32 
 *
 */
OMXResult omxSP_FFTFwd_CToC_SC32_Sfs (
    const OMX_SC32 *pSrc,
    OMX_SC32 *pDst,
    const OMXFFTSpec_C_SC32 *pFFTSpec,
    OMX_INT scaleFactor
);



/**
 * Function:  omxSP_FFTInv_CToC_SC16_Sfs   (2.2.4.2.4)
 *
 * Description:
 * These functions compute an inverse FFT for a complex signal of  length
 * of 2^order, where 0 <= order <= 12. Transform length is determined by the 
 * specification structure, which must be initialized prior to calling the FFT 
 * function using the appropriate helper, i.e., <FFTInit_C_sc32> or 
 * <FFTInit_C_SC16>. The relationship between the input and output sequences 
 * can be expressed in terms of the IDFT, i.e.: 
 *
 *     x[n] = (2^(-scalefactor)/N)  . SUM[k=0,...,N-1] X[k].e^(jnk.2.pi/N)
 *     n=0,1,2,...N-1
 *     N=2^order.
 *
 * Input Arguments:
 *   pSrc - pointer to the complex-valued input signal, of length 2^order ; 
 *          must be aligned on a 32-byte boundary. 
 *   pFFTSpec - pointer to the preallocated and initialized specification 
 *            structure 
 *   scaleFactor - scale factor of the output. Valid range is [0,16].
 *
 * Output Arguments:
 *   order 
 *   pDst - pointer to the complex-valued output signal, of length 2^order; 
 *          must be aligned on a 32-byte boundary. 
 *
 * Return Value:
 *  
 *    Positive value - the shift scale that was performed inside
 *    OMX_Sts_NoErr - no error
 *    OMX_Sts_BadArgErr - returned if one or more of the following conditions 
 *              is true: 
 *    -   one or more of the following pointers is NULL: pSrc, pDst, or 
 *              pFFTSpec. 
 *    -   pSrc or pDst is not 32-byte aligned 
 *    -   scaleFactor<0 or scaleFactor>16 
 *
 */
OMXResult omxSP_FFTInv_CToC_SC16_Sfs (
    const OMX_SC16 *pSrc,
    OMX_SC16 *pDst,
    const OMXFFTSpec_C_SC16 *pFFTSpec,
    OMX_INT scaleFactor
);

OMXResult omxSP_FFTInv_CToC_SC16_Sfs_neon (
    const OMX_SC16 *pSrc,
    OMX_SC16 *pDst,
    const OMXFFTSpec_C_SC16 *pFFTSpec,
    OMX_INT scaleFactor
);




/**
 * Function:  omxSP_FFTInv_CToC_SC32_Sfs   (2.2.4.2.4)
 *
 * Description:
 * These functions compute an inverse FFT for a complex signal of length
 * of 2^order, where 0 <= order <= 12. Transform length is determined by the 
 * specification structure, which must be initialized prior to calling the FFT 
 * function using the appropriate helper, i.e., <FFTInit_C_sc32> or 
 * <FFTInit_C_SC16>. The relationship between the input and output sequences 
 * can be expressed in terms of the IDFT, i.e.: 
 *
 *     x[n] = (2^(-scalefactor)/N)  . SUM[k=0,...,N-1] X[k].e^(jnk.2.pi/N)
 *     n=0,1,2,...N-1
 *     N=2^order.
 *
 * Input Arguments:
 *   pSrc - pointer to the complex-valued input signal, of length 2^order ; 
 *          must be aligned on a 32-byte boundary. 
 *   pFFTSpec - pointer to the preallocated and initialized specification 
 *            structure 
 *   scaleFactor - scale factor of the output. Valid range is [0,32]. 
 *
 * Output Arguments:
 *   order 
 *   pDst - pointer to the complex-valued output signal, of length 2^order; 
 *          must be aligned on a 32-byte boundary. 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - returned if one or more of the following conditions 
 *              is true: 
 *    -   one or more of the following pointers is NULL: pSrc, pDst, or 
 *              pFFTSpec. 
 *    -   pSrc or pDst is not 32-byte aligned 
 *    -   scaleFactor<0 or scaleFactor>32
 *
 */
OMXResult omxSP_FFTInv_CToC_SC32_Sfs (
    const OMX_SC32 *pSrc,
    OMX_SC32 *pDst,
    const OMXFFTSpec_C_SC32 *pFFTSpec,
    OMX_INT scaleFactor
);



/**
 * Function:  omxSP_FFTFwd_RToCCS_S16S32_Sfs   (2.2.4.4.2)
 *
 * Description:
 * These functions compute an FFT for a real-valued signal of length of 2^order,
 * where 0 <= order <= 12. Transform length is determined by the 
 * specification structure, which must be initialized prior to calling the FFT 
 * function using the appropriate helper, i.e., <FFTInit_R_S16S32>. 
 * The relationship between the input and output sequences 
 * can be expressed in terms of the DFT, i.e.:
 *
 *     x[n] = (2^(-scalefactor)/N)  . SUM[k=0,...,N-1] X[k].e^(jnk.2.pi/N)
 *     n=0,1,2,...N-1
 *     N=2^order.
 *
 * The conjugate-symmetric output sequence is represented using a CCS vector, 
 * which is of length N+2, and is organized as follows: 
 *
 *   Index:      0  1  2  3  4  5   . . .   N-2       N-1       N       N+1 
 *   Component:  R0 0  R1 I1 R2 I2  . . .   R[N/2-1]  I[N/2-1]  R[N/2]  0 
 *
 * where R[n] and I[n], respectively, denote the real and imaginary components 
 * for FFT bin 'n'. Bins  are numbered from 0 to N/2, where N is the FFT length. 
 * Bin index 0 corresponds to the DC component, and bin index N/2 corresponds to the 
 * foldover frequency. 
 *
 * Input Arguments:
 *   pSrc - pointer to the real-valued input sequence, of length 2^order; 
 *          must be aligned on a 32-byte boundary. 
 *   pFFTSpec - pointer to the preallocated and initialized specification 
 *            structure 
 *   scaleFactor - output scale factor; valid range is [0, 32] 
 *
 * Output Arguments:
 *   pDst - pointer to output sequence, represented using CCS format, of 
 *            length (2^order)+2; must be aligned on a 32-byte boundary. 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments, if one or more of the following is true: 
 *    -    one of the pointers pSrc, pDst, or pFFTSpec is NULL 
 *    -    pSrc or pDst is not aligned on a 32-byte boundary 
 *    -    scaleFactor<0 or scaleFactor >32 
 *
 */
OMXResult omxSP_FFTFwd_RToCCS_S16S32_Sfs (
    const OMX_S16 *pSrc,
    OMX_S32 *pDst,
    const OMXFFTSpec_R_S16S32 *pFFTSpec,
    OMX_INT scaleFactor
);


/**
 * Function:  omxSP_FFTFwd_RToCCS_S16_Sfs
 *
 * Description:
 * These functions compute an FFT for a real-valued signal of length of 2^order,
 * where 0 < order <= 12. Transform length is determined by the
 * specification structure, which must be initialized prior to calling the FFT
 * function using the appropriate helper, i.e., <FFTInit_R_S16>.
 * The relationship between the input and output sequences can
 * be expressed in terms of the DFT, i.e.:
 *
 *     x[n] = (2^(-scalefactor)/N)  . SUM[k=0,...,N-1] X[k].e^(jnk.2.pi/N)
 *     n=0,1,2,...N-1
 *     N=2^order.
 *
 * The conjugate-symmetric output sequence is represented using a CCS vector,
 * which is of length N+2, and is organized as follows:
 *
 *   Index:      0  1  2  3  4  5   . . .   N-2       N-1       N       N+1
 *   Component:  R0 0  R1 I1 R2 I2  . . .   R[N/2-1]  I[N/2-1]  R[N/2]  0
 *
 * where R[n] and I[n], respectively, denote the real and imaginary components
 * for FFT bin 'n'. Bins  are numbered from 0 to N/2, where N is the FFT length.
 * Bin index 0 corresponds to the DC component, and bin index N/2 corresponds to
 * the foldover frequency.
 *
 * Input Arguments:
 *   pSrc - pointer to the real-valued input sequence, of length 2^order;
 *          must be aligned on a 32-byte boundary.
 *   pFFTSpec - pointer to the preallocated and initialized specification
 *            structure
 *   scaleFactor - output scale factor; valid range is [0, 16]
 *
 * Output Arguments:
 *   pDst - pointer to output sequence, represented using CCS format, of
 *            length (2^order)+2; must be aligned on a 32-byte boundary.
 *
 * Return Value:
 *
 *    OMX_Sts_NoErr - no error
 *    OMX_Sts_BadArgErr - bad arguments, if one or more of followings is true:
 *    -    one of the pointers pSrc, pDst, or pFFTSpec is NULL
 *    -    pSrc or pDst is not aligned on a 32-byte boundary
 *    -    scaleFactor<0 or scaleFactor >16
 *
 */
OMXResult omxSP_FFTFwd_RToCCS_S16_Sfs (
    const OMX_S16* pSrc,
    OMX_S16* pDst,
    const OMXFFTSpec_R_S16* pFFTSpec,
    OMX_INT scaleFactor
);


/**
 * Function:  omxSP_FFTFwd_RToCCS_S32_Sfs   (2.2.4.4.2)
 *
 * Description:
 * These functions compute an FFT for a real-valued signal of length of 2^order,
 * where 0 <= order <= 12. Transform length is determined by the 
 * specification structure, which must be initialized prior to calling the FFT 
 * function using the appropriate helper, i.e., <FFTInit_R_S32>. 
 * The relationship between the input and output sequences 
 * can be expressed in terms of the DFT, i.e.:
 *
 *     x[n] = (2^(-scalefactor)/N)  . SUM[k=0,...,N-1] X[k].e^(jnk.2.pi/N)
 *     n=0,1,2,...N-1
 *     N=2^order.
 *
 * The conjugate-symmetric output sequence is represented using a CCS vector, 
 * which is of length N+2, and is organized as follows: 
 *
 *   Index:      0  1  2  3  4  5   . . .   N-2       N-1       N       N+1 
 *   Component:  R0 0  R1 I1 R2 I2  . . .   R[N/2-1]  I[N/2-1]  R[N/2]  0 
 *
 * where R[n] and I[n], respectively, denote the real and imaginary components 
 * for FFT bin 'n'. Bins  are numbered from 0 to N/2, where N is the FFT length. 
 * Bin index 0 corresponds to the DC component, and bin index N/2 corresponds to the 
 * foldover frequency. 
 *
 * Input Arguments:
 *   pSrc - pointer to the real-valued input sequence, of length 2^order; 
 *          must be aligned on a 32-byte boundary. 
 *   pFFTSpec - pointer to the preallocated and initialized specification 
 *            structure 
 *   scaleFactor - output scale factor; valid range is [0, 32] 
 *
 * Output Arguments:
 *   pDst - pointer to output sequence, represented using CCS format, of 
 *            length (2^order)+2; must be aligned on a 32-byte boundary. 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments, if one or more of the following is true: 
 *    -    one of the pointers pSrc, pDst, or pFFTSpec is NULL 
 *    -    pSrc or pDst is not aligned on a 32-byte boundary 
 *    -    scaleFactor<0 or scaleFactor >32 
 *
 */
OMXResult omxSP_FFTFwd_RToCCS_S32_Sfs (
    const OMX_S32 *pSrc,
    OMX_S32 *pDst,
    const OMXFFTSpec_R_S32 *pFFTSpec,
    OMX_INT scaleFactor
);

/**
 * Function:  omxSP_FFTFwd_CToC_FC32_Sfs   (2.2.4.2.2)
 *
 * Description:
 * Compute an FFT for a complex signal of length of 2^order, 
 * where 0 <= order <= 15. 
 * Transform length is determined by the specification structure, which 
 * must be initialized prior to calling the FFT function using the appropriate 
 * helper, i.e., <FFTInit_C_sc32> or <FFTInit_C_SC16>. The relationship 
 * between the input and output sequences can be expressed in terms of the 
 * DFT, i.e., 
 *
 *      X[k] = SUM[n=0...N-1]x[n].e^(-jnk.2.pi/N)
 *      k = 0,1,2,..., N-1
 *      N = 2^order
 *
 * Input Arguments:
 *   pSrc - pointer to the input signal, a complex-valued vector of length
 *            2^order; must be aligned on a 32 byte boundary. 
 *   pFFTSpec - pointer to the preallocated and initialized specification 
 *            structure 
 *
 * Output Arguments:
 *   pDst - pointer to the complex-valued output vector, of length 2^order;
 *            must be aligned on an 32-byte boundary. 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - returned if one or more of the following conditions 
 *              is true: 
 *    -   one or more of the following pointers is NULL: pSrc, pDst, or 
 *              pFFTSpec. 
 *    -    pSrc or pDst is not 32-byte aligned 
 *    -    scaleFactor<0 or scaleFactor >32 
 *
 */
OMXResult omxSP_FFTFwd_CToC_FC32_Sfs (
    const OMX_FC32 *pSrc,
    OMX_FC32 *pDst,
    const OMXFFTSpec_C_FC32 *pFFTSpec
);
#ifdef __arm__
/*
 * Non-NEON version
 */    
OMXResult omxSP_FFTFwd_CToC_FC32_Sfs_vfp (
    const OMX_FC32 *pSrc,
    OMX_FC32 *pDst,
    const OMXFFTSpec_C_FC32 *pFFTSpec
);
#endif

/**
 * Function:  omxSP_FFTFwd_RToCCS_F32_Sfs
 *
 * Description:
 * These functions compute an FFT for a real-valued signal of length
 * of 2^order, where 0 <= order <= 12. Transform length is determined
 * by the specification structure, which must be initialized prior to
 * calling the FFT function using the appropriate helper, i.e.,
 * <FFTInit_R_F32>. The relationship between the input and output
 * sequences can be expressed in terms of the DFT, i.e.:
 *
 *     x[n] = (2^(-scalefactor)/N)  . SUM[k=0,...,N-1] X[k].e^(jnk.2.pi/N)
 *     n=0,1,2,...N-1
 *     N=2^order.
 *
 * The conjugate-symmetric output sequence is represented using a CCS vector, 
 * which is of length N+2, and is organized as follows: 
 *
 *   Index:      0  1  2  3  4  5   . . .   N-2       N-1       N       N+1 
 *   Component:  R0 0  R1 I1 R2 I2  . . .   R[N/2-1]  I[N/2-1]  R[N/2]  0 
 *
 * where R[n] and I[n], respectively, denote the real and imaginary
 * components for FFT bin 'n'. Bins are numbered from 0 to N/2, where
 * N is the FFT length. Bin index 0 corresponds to the DC component,
 * and bin index N/2 corresponds to the foldover frequency.
 *
 * Input Arguments:
 *   pSrc - pointer to the real-valued input sequence, of length 2^order; 
 *          must be aligned on a 32-byte boundary. 
 *   pFFTSpec - pointer to the preallocated and initialized specification 
 *            structure 
 *
 * Output Arguments:
 *   pDst - pointer to output sequence, represented using CCS format, of 
 *            length (2^order)+2; must be aligned on a 32-byte boundary. 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 

 *    OMX_Sts_BadArgErr - bad arguments, if one or more of the
 *    following is true: - one of the pointers pSrc, pDst, or pFFTSpec
 *    is NULL - pSrc or pDst is not aligned on a 32-byte boundary
 *
 */
OMXResult omxSP_FFTFwd_RToCCS_F32_Sfs(
    const OMX_F32* pSrc,
    OMX_F32* pDst,
    const OMXFFTSpec_R_F32* pFFTSpec
);

#ifdef __arm__
/*
 * Non-NEON version of omxSP_FFTFwd_RToCCS_F32_Sfs
 */    
OMXResult omxSP_FFTFwd_RToCCS_F32_Sfs_vfp(
    const OMX_F32* pSrc,
    OMX_F32* pDst,
    const OMXFFTSpec_R_F32* pFFTSpec
);

/*
 * Just like omxSP_FFTFwd_RToCCS_F32_Sfs, but automatically detects
 * whether NEON is available or not and chooses the appropriate
 * routine.
 */    
extern OMXResult (*omxSP_FFTFwd_RToCCS_F32)(
    const OMX_F32* pSrc,
    OMX_F32* pDst,
    const OMXFFTSpec_R_F32* pFFTSpec
);
#else
#define omxSP_FFTFwd_RToCCS_F32 omxSP_FFTFwd_RToCCS_F32_Sfs
#endif

/**
 * Function:  omxSP_FFTInv_CCSToR_S32S16_Sfs   (2.2.4.4.4)
 *
 * Description:
 * These functions compute the inverse FFT for a conjugate-symmetric input 
 * sequence.  Transform length is determined by the specification structure, 
 * which must be initialized prior to calling the FFT function using 
 * <FFTInit_R_S16S32>. For a transform of length M, the input sequence is 
 * represented using a packed CCS vector of length M+2, and is organized 
 * as follows: 
 *
 *   Index:     0    1  2    3    4    5    . . .  M-2       M-1      M      M+1 
 *   Component  R[0] 0  R[1] I[1] R[2] I[2] . . .  R[M/2-1]  I[M/2-1] R[M/2] 0 
 *
 * where R[n] and I[n], respectively, denote the real and imaginary components for FFT bin n. 
 * Bins are numbered from 0 to M/2, where M is the FFT length.  Bin index 0 
 * corresponds to the DC component, and bin index M/2 corresponds to the 
 * foldover frequency. 
 *
 * Input Arguments:
 *   pSrc - pointer to the complex-valued input sequence represented using 
 *            CCS format, of length (2^order) + 2; must be aligned on a 32-byte 
 *            boundary. 
 *   pFFTSpec - pointer to the preallocated and initialized specification 
 *            structure 
 *   scaleFactor - output scalefactor; range is [0,16]
 *
 * Output Arguments:
 *   pDst - pointer to the real-valued output sequence, of length 2^order ; must be 
 *            aligned on a 32-byte boundary. 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments if one or more of the following is true: 
 *    -    pSrc, pDst, or pFFTSpec is NULL 
 *    -    pSrc or pDst is not aligned on a 32-byte boundary 
 *    -    scaleFactor<0 or scaleFactor >16
 *
 */
OMXResult omxSP_FFTInv_CCSToR_S32S16_Sfs (
    const OMX_S32 *pSrc,
    OMX_S16 *pDst,
    const OMXFFTSpec_R_S16S32 *pFFTSpec,
    OMX_INT scaleFactor
);


/**
 * Function:  omxSP_FFTInv_CCSToR_S16_Sfs
 *
 * Description:
 * These functions compute the inverse FFT for a conjugate-symmetric input
 * sequence.  Transform length is determined by the specification structure,
 * which must be initialized prior to calling the FFT function using
 * <FFTInit_R_S16>. For a transform of length M, the input
 * sequence is represented using a packed CCS vector of length
 * M+2, and is organized as follows:
 *
 *   Index:     0    1  2    3    4    5    . . .  M-2       M-1      M      M+1
 *   Component  R[0] 0  R[1] I[1] R[2] I[2] . . .  R[M/2-1]  I[M/2-1] R[M/2] 0
 *
 * where R[n] and I[n], respectively, denote the real and imaginary components
 * for FFT bin n.
 * Bins are numbered from 0 to M/2, where M is the FFT length.  Bin index 0
 * corresponds to the DC component, and bin index M/2 corresponds to the
 * foldover frequency.
 *
 * Input Arguments:
 *   pSrc - pointer to the complex-valued input sequence represented using
 *            CCS format, of length (2^order) + 2; must be aligned on a 32-byte
 *            boundary.
 *   pFFTSpec - pointer to the preallocated and initialized specification
 *            structure
 *   scaleFactor - output scalefactor; range is [0,16]
 *
 * Output Arguments:
 *   pDst - pointer to the real-valued output sequence, of length 2^order ; must
 *            be aligned on a 32-byte boundary.
 *
 * Return Value:
 *
 *    OMX_Sts_NoErr - no error
 *    OMX_Sts_BadArgErr - bad arguments if one or more of the following is true:
 *    -    pSrc, pDst, or pFFTSpec is NULL
 *    -    pSrc or pDst is not aligned on a 32-byte boundary
 *    -    scaleFactor<0 or scaleFactor >16
 *
 */
OMXResult omxSP_FFTInv_CCSToR_S16_Sfs (
    const OMX_S16* pSrc,
    OMX_S16* pDst,
    const OMXFFTSpec_R_S16* pFFTSpec,
    OMX_INT scaleFactor
);

/**
 * Function:  omxSP_FFTInv_CCSToR_S32_Sfs   (2.2.4.4.4)
 *
 * Description:
 * These functions compute the inverse FFT for a conjugate-symmetric input 
 * sequence.  Transform length is determined by the specification structure, 
 * which must be initialized prior to calling the FFT function using 
 * <FFTInit_R_S32>. For a transform of length M, the input sequence is 
 * represented using a packed CCS vector of length M+2, and is organized 
 * as follows: 
 *
 *   Index:     0    1  2    3    4    5    . . .  M-2       M-1      M      M+1 
 *   Component  R[0] 0  R[1] I[1] R[2] I[2] . . .  R[M/2-1]  I[M/2-1] R[M/2] 0 
 *
 * where R[n] and I[n], respectively, denote the real and imaginary components for FFT bin n. 
 * Bins are numbered from 0 to M/2, where M is the FFT length.  Bin index 0 
 * corresponds to the DC component, and bin index M/2 corresponds to the 
 * foldover frequency. 
 *
 * Input Arguments:
 *   pSrc - pointer to the complex-valued input sequence represented using 
 *            CCS format, of length (2^order) + 2; must be aligned on a 32-byte 
 *            boundary. 
 *   pFFTSpec - pointer to the preallocated and initialized specification 
 *            structure 
 *   scaleFactor - output scalefactor; range is [0,32] 
 *
 * Output Arguments:
 *   pDst - pointer to the real-valued output sequence, of length 2^order ; must be 
 *            aligned on a 32-byte boundary. 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - bad arguments if one or more of the following is true: 
 *    -    pSrc, pDst, or pFFTSpec is NULL 
 *    -    pSrc or pDst is not aligned on a 32-byte boundary 
 *    -    scaleFactor<0 or scaleFactor >32
 *
 */
OMXResult omxSP_FFTInv_CCSToR_S32_Sfs (
    const OMX_S32 *pSrc,
    OMX_S32 *pDst,
    const OMXFFTSpec_R_S32 *pFFTSpec,
    OMX_INT scaleFactor
);

/**
 * Function:  omxSP_FFTInv_CToC_FC32_Sfs   (2.2.4.2.4)
 *
 * Description:
 * These functions compute an inverse FFT for a complex signal of
 * length of 2^order, where 0 <= order <= 15. Transform length is
 * determined by the specification structure, which must be
 * initialized prior to calling the FFT function using the appropriate
 * helper, i.e., <FFTInit_C_FC32>. The relationship between the input
 * and output sequences can be expressed in terms of the IDFT, i.e.:
 *
 *     x[n] = SUM[k=0,...,N-1] X[k].e^(jnk.2.pi/N)
 *     n=0,1,2,...N-1
 *     N=2^order.
 *
 * Input Arguments:
 *   pSrc - pointer to the complex-valued input signal, of length 2^order ; 
 *          must be aligned on a 32-byte boundary. 
 *   pFFTSpec - pointer to the preallocated and initialized specification 
 *            structure 
 *
 * Output Arguments:
 *   order 
 *   pDst - pointer to the complex-valued output signal, of length 2^order; 
 *          must be aligned on a 32-byte boundary. 
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 
 *    OMX_Sts_BadArgErr - returned if one or more of the following conditions 
 *              is true: 
 *    -   one or more of the following pointers is NULL: pSrc, pDst, or 
 *              pFFTSpec. 
 *    -   pSrc or pDst is not 32-byte aligned 
 *    -   scaleFactor<0 or scaleFactor>32
 *
 */
OMXResult omxSP_FFTInv_CToC_FC32_Sfs (
    const OMX_FC32 *pSrc,
    OMX_FC32 *pDst,
    const OMXFFTSpec_C_FC32 *pFFTSpec
);
#ifdef __arm__
/*
 * Non-NEON version
 */    
OMXResult omxSP_FFTInv_CToC_FC32_Sfs_vfp (
    const OMX_FC32 *pSrc,
    OMX_FC32 *pDst,
    const OMXFFTSpec_C_FC32 *pFFTSpec
);
#endif

/**
 * Function:  omxSP_FFTInv_CCSToR_F32_Sfs
 *
 * Description:
 * These functions compute the inverse FFT for a conjugate-symmetric input 
 * sequence.  Transform length is determined by the specification structure, 
 * which must be initialized prior to calling the FFT function using 
 * <FFTInit_R_F32>. For a transform of length M, the input sequence is 
 * represented using a packed CCS vector of length M+2, and is organized 
 * as follows: 
 *
 *   Index:   0  1  2    3    4    5    . . .  M-2       M-1      M      M+1 
 *   Comp:  R[0] 0  R[1] I[1] R[2] I[2] . . .  R[M/2-1]  I[M/2-1] R[M/2] 0 
 *
 * where R[n] and I[n], respectively, denote the real and imaginary
 * components for FFT bin n. Bins are numbered from 0 to M/2, where M
 * is the FFT length.  Bin index 0 corresponds to the DC component,
 * and bin index M/2 corresponds to the foldover frequency.
 *
 * Input Arguments:
 *   pSrc - pointer to the complex-valued input sequence represented
 *          using CCS format, of length (2^order) + 2; must be aligned on a
 *          32-byte boundary.
 *   pFFTSpec - pointer to the preallocated and initialized
 *              specification structure
 *
 * Output Arguments:
 *   pDst - pointer to the real-valued output sequence, of length
 *          2^order ; must be aligned on a 32-byte boundary.
 *
 * Return Value:
 *    
 *    OMX_Sts_NoErr - no error 

 *    OMX_Sts_BadArgErr - bad arguments if one or more of the
 *      following is true:
 *    -    pSrc, pDst, or pFFTSpec is NULL 
 *    -    pSrc or pDst is not aligned on a 32-byte boundary 
 *    -    scaleFactor<0 or scaleFactor >32
 *
 */
OMXResult omxSP_FFTInv_CCSToR_F32_Sfs(
    const OMX_F32* pSrc,
    OMX_F32* pDst,
    const OMXFFTSpec_R_F32* pFFTSpec
);

#ifdef __arm__
/*
 * Non-NEON version of omxSP_FFTInv_CCSToR_F32_Sfs
 */    
OMXResult omxSP_FFTInv_CCSToR_F32_Sfs_vfp(
    const OMX_F32* pSrc,
    OMX_F32* pDst,
    const OMXFFTSpec_R_F32* pFFTSpec
);

/*
 * Just like omxSP_FFTInv_CCSToR_F32_Sfs, but automatically detects
 * whether NEON is available or not and chooses the appropriate
 * routine.
 */    
extern OMXResult (*omxSP_FFTInv_CCSToR_F32)(
    const OMX_F32* pSrc,
    OMX_F32* pDst,
    const OMXFFTSpec_R_F32* pFFTSpec);
#else
#define omxSP_FFTInv_CCSToR_F32 omxSP_FFTInv_CCSToR_F32_Sfs    
#endif

/*
 * Just like omxSP_FFTInv_CCSToR_F32_Sfs, but does not scale the result.
 * (Actually, we multiple by two for consistency with other FFT routines in
 * use.)
 */
OMXResult omxSP_FFTInv_CCSToR_F32_Sfs_unscaled(
    const OMX_F32* pSrc,
    OMX_F32* pDst,
    const OMXFFTSpec_R_F32* pFFTSpec
);


#ifdef __cplusplus
}
#endif

#endif /** end of #define _OMXSP_H_ */

/** EOF */

