/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef XP_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "mar_private.h"
#include "mar.h"
#include "cryptox.h"

int mar_verify_signature_fp(FILE *fp, 
                            CryptoX_ProviderHandle provider, 
                            CryptoX_PublicKey key);
int mar_verify_signature_for_fp(FILE *fp, 
                                CryptoX_ProviderHandle provider, 
                                CryptoX_PublicKey key, 
                                PRUint32 signatureCount,
                                char *extractedSignature);

/**
 * Reads the specified number of bytes from the file pointer and
 * stores them in the passed buffer.
 *
 * @param  fp     The file pointer to read from.
 * @param  buffer The buffer to store the read results.
 * @param  size   The number of bytes to read, buffer must be 
 *                at least of this size.
 * @param  ctx    The verify context.
 * @param  err    The name of what is being written to in case of error.
 * @return  0 on success
 *         -1 on read error
 *         -2 on verify update error
*/
int
ReadAndUpdateVerifyContext(FILE *fp, 
                           void *buffer,
                           PRUint32 size, 
                           CryptoX_SignatureHandle *ctx,
                           const char *err) 
{
  if (!fp || !buffer || !ctx || !err) {
    fprintf(stderr, "ERROR: Invalid parameter specified.\n");
    return CryptoX_Error;
  }

  if (!size) { 
    return CryptoX_Success;
  }

  if (fread(buffer, size, 1, fp) != 1) {
    fprintf(stderr, "ERROR: Could not read %s\n", err);
    return CryptoX_Error;
  }

  if (CryptoX_Failed(CryptoX_VerifyUpdate(ctx, buffer, size))) {
    fprintf(stderr, "ERROR: Could not update verify context for %s\n", err);
    return -2;
  }
  return CryptoX_Success;
}

/**
 * Verifies the embedded signature of the specified file path.
 * This is only used by the signmar program when used with arguments to verify 
 * a MAR. This should not be used to verify a MAR that will be extracted in the 
 * same operation by updater code. This function prints the error message if 
 * verification fails.
 * 
 * @param pathToMAR  The path of the MAR file who's signature should be checked
 * @param certData       The certificate file data.
 * @param sizeOfCertData The size of the cert data.
 * @param certName   Used only if compiled as NSS, specifies the certName
 * @return 0 on success
 *         a negative number if there was an error
 *         a positive number if the signature does not verify
 */
int
mar_verify_signature(const char *pathToMARFile, 
                     const char *certData,
                     PRUint32 sizeOfCertData,
                     const char *certName) {
  int rv;
  CryptoX_ProviderHandle provider = CryptoX_InvalidHandleValue;
  CryptoX_Certificate cert;
  CryptoX_PublicKey key;
  FILE *fp;
  
  if (!pathToMARFile || (!certData && !certName)) {
    fprintf(stderr, "ERROR: Invalid parameter specified.\n");
    return CryptoX_Error;
  }

  fp = fopen(pathToMARFile, "rb");
  if (!fp) {
    fprintf(stderr, "ERROR: Could not open MAR file.\n");
    return CryptoX_Error;
  }

  if (CryptoX_Failed(CryptoX_InitCryptoProvider(&provider))) {
    fclose(fp);
    fprintf(stderr, "ERROR: Could not init crytpo library.\n");
    return CryptoX_Error;
  }

  if (CryptoX_Failed(CryptoX_LoadPublicKey(provider, certData, sizeOfCertData,
                                           &key, certName, &cert))) {
    fclose(fp);
    fprintf(stderr, "ERROR: Could not load public key.\n");
    return CryptoX_Error;
  }

  rv = mar_verify_signature_fp(fp, provider, key);
  fclose(fp);
  if (key) {
    CryptoX_FreePublicKey(&key);
  }

  if (cert) {
    CryptoX_FreeCertificate(&cert);
  }
  return rv;
}

#ifdef XP_WIN
/**
 * Verifies a MAR file's signature by making sure at least one 
 * signature verifies.
 * 
 * @param  pathToMARFile The path of the MAR file who's signature 
 *                       should be calculated
 * @param  certData      The certificate data
 * @param sizeOfCertData The size of the data stored in certData
 * @return 0 on success
*/
int
mar_verify_signatureW(MarFile *mar, 
                      const char *certData,
                      PRUint32 sizeOfCertData) {
  int rv;
  CryptoX_ProviderHandle provider = CryptoX_InvalidHandleValue;
  CryptoX_Certificate cert;
  CryptoX_PublicKey key;
  
  if (!mar || !certData) {
    fprintf(stderr, "ERROR: Invalid parameter specified.\n");
    return CryptoX_Error;
  }

  if (!mar->fp) {
    fprintf(stderr, "ERROR: MAR file is not open.\n");
    return CryptoX_Error;
  }

  if (CryptoX_Failed(CryptoX_InitCryptoProvider(&provider))) { 
    fprintf(stderr, "ERROR: Could not init crytpo library.\n");
    return CryptoX_Error;
  }

  if (CryptoX_Failed(CryptoX_LoadPublicKey(provider, certData, sizeOfCertData,
                                           &key, "", &cert))) {
    fprintf(stderr, "ERROR: Could not load public key.\n");
    return CryptoX_Error;
  }

  rv = mar_verify_signature_fp(mar->fp, provider, key);
  if (key) {
    CryptoX_FreePublicKey(&key);
  }

  if (cert) {
    CryptoX_FreeCertificate(&cert);
  }
  return rv;
}
#endif

/**
 * Verifies a MAR file's signature by making sure at least one 
 * signature verifies.
 * 
 * @param  fp       An opened MAR file handle
 * @param  provider A library provider
 * @param  key      The public key to use to verify the MAR
 * @return 0 on success
*/
int
mar_verify_signature_fp(FILE *fp,
                        CryptoX_ProviderHandle provider, 
                        CryptoX_PublicKey key) {
  char buf[5] = {0};
  PRUint32 signatureAlgorithmID, signatureCount, signatureLen, numVerified = 0;
  int rv = -1;
  PRInt64 curPos;
  char *extractedSignature;
  PRUint32 i;

  if (!fp) {
    fprintf(stderr, "ERROR: Invalid file pointer passed.\n");
    return CryptoX_Error;
  }
  
  /* To protect against invalid MAR files, we assumes that the MAR file 
     size is less than or equal to MAX_SIZE_OF_MAR_FILE. */
  if (fseeko(fp, 0, SEEK_END)) {
    fprintf(stderr, "ERROR: Could not seek to the end of the MAR file.\n");
    return CryptoX_Error;
  }
  if (ftello(fp) > MAX_SIZE_OF_MAR_FILE) {
    fprintf(stderr, "ERROR: MAR file is too large to be verified.\n");
    return CryptoX_Error;
  }

  /* Skip to the start of the signature block */
  if (fseeko(fp, SIGNATURE_BLOCK_OFFSET, SEEK_SET)) {
    fprintf(stderr, "ERROR: Could not seek to the signature block.\n");
    return CryptoX_Error;
  }

  /* Get the number of signatures */
  if (fread(&signatureCount, sizeof(signatureCount), 1, fp) != 1) {
    fprintf(stderr, "ERROR: Could not read number of signatures.\n");
    return CryptoX_Error;
  }
  signatureCount = ntohl(signatureCount);

  /* Check that we have less than the max amount of signatures so we don't
     waste too much of either updater's or signmar's time. */
  if (signatureCount > MAX_SIGNATURES) {
    fprintf(stderr, "ERROR: At most %d signatures can be specified.\n",
            MAX_SIGNATURES);
    return CryptoX_Error;
  }

  for (i = 0; i < signatureCount && numVerified == 0; i++) {
    /* Get the signature algorithm ID */
    if (fread(&signatureAlgorithmID, sizeof(PRUint32), 1, fp) != 1) {
      fprintf(stderr, "ERROR: Could not read signatures algorithm ID.\n");
      return CryptoX_Error;
    }
    signatureAlgorithmID = ntohl(signatureAlgorithmID);
  
    if (fread(&signatureLen, sizeof(PRUint32), 1, fp) != 1) {
      fprintf(stderr, "ERROR: Could not read signatures length.\n");
      return CryptoX_Error;
    }
    signatureLen = ntohl(signatureLen);

    /* To protected against invalid input make sure the signature length
       isn't too big. */
    if (signatureLen > MAX_SIGNATURE_LENGTH) {
      fprintf(stderr, "ERROR: Signature length is too large to verify.\n");
      return CryptoX_Error;
    }

    extractedSignature = malloc(signatureLen);
    if (!extractedSignature) {
      fprintf(stderr, "ERROR: Could allocate buffer for signature.\n");
      return CryptoX_Error;
    }
    if (fread(extractedSignature, signatureLen, 1, fp) != 1) {
      fprintf(stderr, "ERROR: Could not read extracted signature.\n");
      free(extractedSignature);
      return CryptoX_Error;
    }

    /* We don't try to verify signatures we don't know about */
    if (1 == signatureAlgorithmID) {
      curPos = ftello(fp);
      rv = mar_verify_signature_for_fp(fp, 
                                       provider, 
                                       key,
                                       signatureCount,
                                       extractedSignature);
      if (CryptoX_Succeeded(rv)) {
        numVerified++;
      }
      free(extractedSignature);
      if (fseeko(fp, curPos, SEEK_SET)) {
        fprintf(stderr, "ERROR: Could not seek back to last signature.\n");
        return CryptoX_Error;
      }
    } else {
      free(extractedSignature);
    }
  }

  /* If we reached here and we verified at least one 
     signature, return success. */
  if (numVerified > 0) {
    return CryptoX_Success;
  } else {
    fprintf(stderr, "ERROR: No signatures were verified.\n");
    return CryptoX_Error;
  }
}

/**
 * Verifies if a specific signature ID matches the extracted signature.
 * 
 * @param  fp                   An opened MAR file handle
 * @param  provider             A library provider
 * @param  key                  The public key to use to verify the MAR
 * @param  signatureCount        The number of signatures in the MAR file
 * @param  extractedSignature    The signature that should be verified
 * @return 0 on success
*/
int
mar_verify_signature_for_fp(FILE *fp, 
                            CryptoX_ProviderHandle provider, 
                            CryptoX_PublicKey key, 
                            PRUint32 signatureCount,
                            char *extractedSignature) {
  CryptoX_SignatureHandle signatureHandle;
  char buf[BLOCKSIZE];
  PRUint32 signatureLen;
  PRUint32 i;

  if (!extractedSignature) {
    fprintf(stderr, "ERROR: Invalid parameter specified.\n");
    return CryptoX_Error;
  }

  /* This function is only called when we have at least one signature,
     but to protected against future people who call this function we
     make sure a non zero value is passed in. 
   */
  if (!signatureCount) {
    fprintf(stderr, "ERROR: There must be at least one signature.\n");
    return CryptoX_Error;
  }

  CryptoX_VerifyBegin(provider, &signatureHandle, &key);

  /* Skip to the start of the file */
  if (fseeko(fp, 0, SEEK_SET)) {
    fprintf(stderr, "ERROR: Could not seek to start of the file\n");
    return CryptoX_Error;
  }

  /* Bytes 0-3: MAR1
     Bytes 4-7: index offset 
     Bytes 8-15: size of entire MAR
   */
  if (CryptoX_Failed(ReadAndUpdateVerifyContext(fp, buf, 
                                                SIGNATURE_BLOCK_OFFSET +
                                                sizeof(PRUint32),
                                                &signatureHandle,
                                                "signature block"))) {
    return CryptoX_Error;
  }

  for (i = 0; i < signatureCount; i++) {
    /* Get the signature algorithm ID */
    if (CryptoX_Failed(ReadAndUpdateVerifyContext(fp,
                                                  &buf, 
                                                  sizeof(PRUint32),
                                                  &signatureHandle, 
                                                  "signature algorithm ID"))) {
        return CryptoX_Error;
    }

    if (CryptoX_Failed(ReadAndUpdateVerifyContext(fp, 
                                                  &signatureLen, 
                                                  sizeof(PRUint32), 
                                                  &signatureHandle, 
                                                  "signature length"))) {
      return CryptoX_Error;
    }
    signatureLen = ntohl(signatureLen);

    /* Skip past the signature itself as those are not included */
    if (fseeko(fp, signatureLen, SEEK_CUR)) {
      fprintf(stderr, "ERROR: Could not seek past signature.\n");
      return CryptoX_Error;
    }
  }

  while (!feof(fp)) {
    int numRead = fread(buf, 1, BLOCKSIZE , fp);
    if (ferror(fp)) {
      fprintf(stderr, "ERROR: Error reading data block.\n");
      return CryptoX_Error;
    }

    if (CryptoX_Failed(CryptoX_VerifyUpdate(&signatureHandle, 
                                            buf, numRead))) {
      fprintf(stderr, "ERROR: Error updating verify context with"
                      " data block.\n");
      return CryptoX_Error;
    }
  }

  if (CryptoX_Failed(CryptoX_VerifySignature(&signatureHandle, 
                                             &key,
                                             extractedSignature, 
                                             signatureLen))) {
    fprintf(stderr, "ERROR: Error verifying signature.\n");
    return CryptoX_Error;
  }

  return CryptoX_Success;
}
