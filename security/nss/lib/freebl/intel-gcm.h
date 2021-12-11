/******************************************************************************/
/* LICENSE:                                                                   */
/* This submission to NSS is to be made available under the terms of the      */
/* Mozilla Public License, v. 2.0. You can obtain one at http:                */
/* //mozilla.org/MPL/2.0/.                                                    */
/******************************************************************************/
/* Copyright(c) 2013, Intel Corp.                                             */
/******************************************************************************/
/* Reference:                                                                 */
/* [1] Shay Gueron, Michael E. Kounavis: Intel(R) Carry-Less Multiplication   */
/*     Instruction and its Usage for Computing the GCM Mode (Rev. 2.01)       */
/*     http://software.intel.com/sites/default/files/article/165685/clmul-wp-r*/
/*ev-2.01-2012-09-21.pdf                                                      */
/* [2] S. Gueron, M. E. Kounavis: Efficient Implementation of the Galois      */
/*     Counter Mode Using a Carry-less Multiplier and a Fast Reduction        */
/*     Algorithm. Information Processing Letters 110: 549-553 (2010).         */
/* [3] S. Gueron: AES Performance on the 2nd Generation Intel(R) Core(TM)     */
/*     Processor Family (to be posted) (2012).                                */
/* [4] S. Gueron: Fast GHASH computations for speeding up AES-GCM (to be      */
/*     published) (2012).                                                     */

#ifndef INTEL_GCM_H
#define INTEL_GCM_H 1

#include "blapii.h"

typedef struct intel_AES_GCMContextStr intel_AES_GCMContext;

intel_AES_GCMContext *intel_AES_GCM_CreateContext(void *context, freeblCipherFunc cipher,
                                                  const unsigned char *params);

void intel_AES_GCM_DestroyContext(intel_AES_GCMContext *gcm, PRBool freeit);

SECStatus intel_AES_GCM_EncryptUpdate(intel_AES_GCMContext *gcm, unsigned char *outbuf,
                                      unsigned int *outlen, unsigned int maxout,
                                      const unsigned char *inbuf, unsigned int inlen,
                                      unsigned int blocksize);

SECStatus intel_AES_GCM_DecryptUpdate(intel_AES_GCMContext *gcm, unsigned char *outbuf,
                                      unsigned int *outlen, unsigned int maxout,
                                      const unsigned char *inbuf, unsigned int inlen,
                                      unsigned int blocksize);
SECStatus intel_AES_GCM_EncryptAEAD(intel_AES_GCMContext *gcm,
                                    unsigned char *outbuf,
                                    unsigned int *outlen, unsigned int maxout,
                                    const unsigned char *inbuf, unsigned int inlen,
                                    void *params, unsigned int paramLen,
                                    const unsigned char *aad, unsigned int aadLen,
                                    unsigned int blocksize);
SECStatus intel_AES_GCM_DecryptAEAD(intel_AES_GCMContext *gcm,
                                    unsigned char *outbuf,
                                    unsigned int *outlen, unsigned int maxout,
                                    const unsigned char *inbuf, unsigned int inlen,
                                    void *params, unsigned int paramLen,
                                    const unsigned char *aad, unsigned int aadLen,
                                    unsigned int blocksize);

/* Prototypes of functions in the assembler file for fast AES-GCM, using
   Intel AES-NI and CLMUL-NI, as described in [1]
   [1] Shay Gueron, Michael E. Kounavis: Intel(R) Carry-Less Multiplication
       Instruction and its Usage for Computing the GCM Mode                */

/* Prepares the constants used in the aggregated reduction method */
void intel_aes_gcmINIT(unsigned char Htbl[16 * 16],
                       unsigned char *KS,
                       int NR);

/* Produces the final GHASH value */
void intel_aes_gcmTAG(unsigned char Htbl[16 * 16],
                      unsigned char *Tp,
                      unsigned long Mlen,
                      unsigned long Alen,
                      unsigned char *X0,
                      unsigned char *TAG);

/* Hashes the Additional Authenticated Data, should be used before enc/dec.
   Operates on whole blocks only. Partial blocks should be padded externally. */
void intel_aes_gcmAAD(unsigned char Htbl[16 * 16],
                      unsigned char *AAD,
                      unsigned long Alen,
                      unsigned char *Tp);

/* Encrypts and hashes the Plaintext.
   Operates on any length of data, however partial block should only be encrypted
   at the last call, otherwise the result will be incorrect. */
void intel_aes_gcmENC(const unsigned char *PT,
                      unsigned char *CT,
                      void *Gctx,
                      unsigned long len);

/* Similar to ENC, but decrypts the Ciphertext. */
void intel_aes_gcmDEC(const unsigned char *CT,
                      unsigned char *PT,
                      void *Gctx,
                      unsigned long len);

#endif
