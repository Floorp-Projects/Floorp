/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1997-2000 Carnegie Mellon University.  All rights 
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
/*********************************************************************
 *
 * File: matrix.h
 * 
 * Description: Matrix and linear algebra functions
 * 
 * Author: 
 * 
 *********************************************************************/

#ifndef MATRIX_H
#define MATRIX_H

/** \file matrix.h
 * \brief Matrix and linear algebra functions.
 *
 * This file contains some basic matrix and linear algebra operations.
 * In general these operate on positive definite matrices ONLY,
 * because all matrices we're likely to encounter are either
 * covariance matrices or are derived from them, and therefore a
 * non-positive-definite matrix indicates some kind of pathological
 * condition.
 */
#ifdef __cplusplus
extern "C" {
#endif
#if 0
/* Fool Emacs. */
}
#endif

/* Win32/WinCE DLL gunk */
#include <sphinxbase/sphinxbase_export.h>
#include <sphinxbase/prim_type.h>


/**
 * Norm an array
 * @param arr array
 * @param d1 dimension
 * @param d2 dimension
 * @param d3 dimension
 **/
SPHINXBASE_EXPORT void norm_3d(float32 ***arr, uint32 d1, uint32 d2, uint32 d3);

/**
 * Floor 3-d array
 * @param out output array
 * @para in input array
 * @param d1 dimension
 * @param d2 dimension
 * @param d3 dimension
 **/
SPHINXBASE_EXPORT void
accum_3d(float32 ***out, float32 ***in, uint32 d1, uint32 d2, uint32 d3);

/** Ensures that non-zero values x such that -band < x < band, band > 0 are set to -band if x < 0 and band if x > 0.
 * @param v array
 * @param d1 array size
 * @param band band value
 */
SPHINXBASE_EXPORT void band_nz_1d(float32 *v, uint32 d1, float32 band);

/**
 * Floor 3-d array
 * @param m array
 * @param d1 dimension
 * @param d2 dimension
 * @param d3 dimension
 * @param floor floor value
 **/
SPHINXBASE_EXPORT void floor_nz_3d(float32 ***m, uint32 d1, uint32 d2, uint32 d3, float32 floor);

/**
 * Floor 1-d array
 * @param m array
 * @param d1 dimension
 * @param floor floor value
 **/
SPHINXBASE_EXPORT void floor_nz_1d(float32 *v, uint32 d1, float32 floor);

/**
 * Calculate the determinant of a positive definite matrix.
 * @param a The input matrix, must be positive definite.
 * @param len The dimension of the input matrix.
 * @return The determinant of the input matrix, or -1.0 if the matrix is
 * not positive definite.
 *
 * \note These can be vanishingly small hence the float64 return type.
 * Also note that only the upper triangular portion of a is
 * considered, therefore the check for positive-definiteness is not
 * reliable.
 **/
SPHINXBASE_EXPORT
float64 determinant(float32 **a, int32 len);

/**
 * Invert (if possible) a positive definite matrix.
 * @param out_ainv The inverse of a will be stored here.
 * @param a The input matrix, must be positive definite.
 * @param len The dimension of the input matrix.
 * @return 0 for success or -1 for a non-positive-definite matrix.
 *
 * \note Only the upper triangular portion of a is considered,
 * therefore the check for positive-definiteness is not reliable.
 **/
SPHINXBASE_EXPORT
int32 invert(float32 **out_ainv, float32 **a, int32 len);

/**
 * Solve (if possible) a positive-definite system of linear equations AX=B for X.
 * @param a The A matrix on the left-hand side of the equation, must be positive-definite.
 * @param b The B vector on the right-hand side of the equation.
 * @param out_x The X vector will be stored here.
 * @param n The dimension of the A matrix (n by n) and the B and X vectors.
 * @return 0 for success or -1 for a non-positive-definite matrix.
 *
 * \note Only the upper triangular portion of a is considered,
 * therefore the check for positive-definiteness is not reliable.
 **/
SPHINXBASE_EXPORT
int32 solve(float32 **a, float32 *b,
            float32 *out_x, int32 n);

/**
 * Calculate the outer product of two vectors.
 * @param out_a A (pre-allocated) len x len array. The outer product
 * will be stored here.
 * @param x A vector of length len.
 * @param y A vector of length len.
 * @param len The length of the input vectors.
 **/
SPHINXBASE_EXPORT
void outerproduct(float32 **out_a, float32 *x, float32 *y, int32 len);

/**
 * Multiply C=AB where A and B are symmetric matrices.
 * @param out_c The output matrix C.
 * @param a The input matrix A.
 * @param b The input matrix B.
 * @param n Dimensionality of A and B.
 **/
SPHINXBASE_EXPORT
void matrixmultiply(float32 **out_c, /* = */
                    float32 **a, /* * */ float32 **b,
                    int32 n);

/**
 * Multiply a symmetric matrix by a constant in-place.
 * @param inout_a The matrix to multiply.
 * @param x The constant to multiply it by.
 * @param n dimension of a.
 **/
SPHINXBASE_EXPORT
void scalarmultiply(float32 **inout_a, float32 x, int32 n);

/**
 * Add A += B.
 * @param inout_a The A matrix to add.
 * @param b The B matrix to add to A.
 * @param n dimension of a and b.
 **/
SPHINXBASE_EXPORT
void matrixadd(float32 **inout_a, float32 **b, int32 n);

#if 0
{ /* Fool indent. */
#endif
#ifdef __cplusplus
}
#endif

#endif /* MATRIX_H */ 

