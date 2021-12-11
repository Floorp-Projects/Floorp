/******************************************************************************/
/* LICENSE:                                                                   */
/* This submission to NSS is to be made available under the terms of the      */
/* Mozilla Public License, v. 2.0. You can obtain one at http:                */
/* //mozilla.org/MPL/2.0/.                                                    */
/******************************************************************************/

#ifndef PPC_GCM_H
#define PPC_GCM_H 1

#include "blapii.h"

typedef struct ppc_AES_GCMContextStr ppc_AES_GCMContext;

ppc_AES_GCMContext *ppc_AES_GCM_CreateContext(void *context, freeblCipherFunc cipher,
                                              const unsigned char *params);

void ppc_AES_GCM_DestroyContext(ppc_AES_GCMContext *gcm, PRBool freeit);

SECStatus ppc_AES_GCM_EncryptUpdate(ppc_AES_GCMContext *gcm, unsigned char *outbuf,
                                    unsigned int *outlen, unsigned int maxout,
                                    const unsigned char *inbuf, unsigned int inlen,
                                    unsigned int blocksize);

SECStatus ppc_AES_GCM_DecryptUpdate(ppc_AES_GCMContext *gcm, unsigned char *outbuf,
                                    unsigned int *outlen, unsigned int maxout,
                                    const unsigned char *inbuf, unsigned int inlen,
                                    unsigned int blocksize);
SECStatus ppc_AES_GCM_EncryptAEAD(ppc_AES_GCMContext *gcm,
                                  unsigned char *outbuf,
                                  unsigned int *outlen, unsigned int maxout,
                                  const unsigned char *inbuf, unsigned int inlen,
                                  void *params, unsigned int paramLen,
                                  const unsigned char *aad, unsigned int aadLen,
                                  unsigned int blocksize);
SECStatus ppc_AES_GCM_DecryptAEAD(ppc_AES_GCMContext *gcm,
                                  unsigned char *outbuf,
                                  unsigned int *outlen, unsigned int maxout,
                                  const unsigned char *inbuf, unsigned int inlen,
                                  void *params, unsigned int paramLen,
                                  const unsigned char *aad, unsigned int aadLen,
                                  unsigned int blocksize);

/* Prototypes of the functions defined in the assembler file.  */

/* Prepares the constants used in the aggregated reduction method */
void ppc_aes_gcmINIT(unsigned char Htbl[8 * 16],
                     PRUint32 *KS,
                     int NR);

/* Produces the final GHASH value */
void ppc_aes_gcmTAG(unsigned char Htbl[8 * 16],
                    unsigned char *Tp,
                    unsigned long Mlen,
                    unsigned long Alen,
                    unsigned char *X0,
                    unsigned char *TAG);

/* Hashes the Additional Authenticated Data, should be used before enc/dec.
   Operates on any length of data. Partial block is padded internally. */
void ppc_aes_gcmHASH(unsigned char Htbl[8 * 16],
                     const unsigned char *AAD,
                     unsigned long Alen,
                     unsigned char *Tp);

/* Crypt only, used in combination with ppc_aes_gcmAAD().
   Operates on any length of data, however partial block should only be encrypted
   at the last call, otherwise the result will be incorrect. */
void ppc_aes_gcmCRYPT(const unsigned char *PT,
                      unsigned char *CT,
                      unsigned long len,
                      unsigned char *CTRP,
                      PRUint32 *KS,
                      int NR);

#endif
