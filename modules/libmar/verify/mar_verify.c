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

int mar_extract_and_verify_signatures_fp(FILE *fp,
                                         CryptoX_ProviderHandle provider,
                                         CryptoX_PublicKey *keys,
                                         uint32_t keyCount);
int mar_verify_signatures_for_fp(FILE *fp,
                                 CryptoX_ProviderHandle provider,
                                 CryptoX_PublicKey *keys,
                                 const uint8_t * const *extractedSignatures,
                                 uint32_t keyCount,
                                 uint32_t *numVerified);

/**
 * Reads the specified number of bytes from the file pointer and
 * stores them in the passed buffer.
 *
 * @param  fp     The file pointer to read from.
 * @param  buffer The buffer to store the read results.
 * @param  size   The number of bytes to read, buffer must be 
 *                at least of this size.
 * @param  ctxs   Pointer to the first element in an array of verify context.
 * @param  count  The number of elements in ctxs
 * @param  err    The name of what is being written to in case of error.
 * @return  0 on success
 *         -1 on read error
 *         -2 on verify update error
*/
int
ReadAndUpdateVerifyContext(FILE *fp, 
                           void *buffer,
                           uint32_t size, 
                           CryptoX_SignatureHandle *ctxs,
                           uint32_t count,
                           const char *err) 
{
  uint32_t k;
  if (!fp || !buffer || !ctxs || count == 0 || !err) {
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

  for (k = 0; k < count; k++) {
    if (CryptoX_Failed(CryptoX_VerifyUpdate(&ctxs[k], buffer, size))) {
      fprintf(stderr, "ERROR: Could not update verify context for %s\n", err);
      return -2;
    }
  }
  return CryptoX_Success;
}

/**
 * Verifies a MAR file by verifying each signature with the corresponding
 * certificate. That is, the first signature will be verified using the first
 * certificate given, the second signature will be verified using the second
 * certificate given, etc. The signature count must exactly match the number of
 * certificates given, and all signature verifications must succeed.
 * This is only used by the signmar program when used with arguments to verify 
 * a MAR. This should not be used to verify a MAR that will be extracted in the 
 * same operation by updater code. This function prints the error message if 
 * verification fails.
 * 
 * @param pathToMARFile The path of the MAR file to verify.
 * @param certData      Pointer to the first element in an array of certificate
 *                      file data.
 * @param certDataSizes Pointer to the first element in an array for size of the
 *                      cert data.
 * @param certNames     Pointer to the first element in an array of certificate names.
 *                      Used only if compiled as NSS, specifies the certificate names
 * @param certCount     The number of elements in certData, certDataSizes, and certNames
 * @return 0 on success
 *         a negative number if there was an error
 *         a positive number if the signature does not verify
 */
int
mar_verify_signatures(const char *pathToMARFile,
                      const uint8_t * const *certData,
                      const uint32_t *certDataSizes,
                      const char * const *certNames,
                      uint32_t certCount) {
  int rv;
  CryptoX_ProviderHandle provider = CryptoX_InvalidHandleValue;
  CryptoX_Certificate certs[MAX_SIGNATURES];
  CryptoX_PublicKey keys[MAX_SIGNATURES];
  FILE *fp;
  uint32_t k;
  
  memset(certs, 0, sizeof(certs));
  memset(keys, 0, sizeof(keys));

  if (!pathToMARFile || certCount == 0) {
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

  /* Load the certs and keys */
  for (k = 0; k < certCount; k++) {
    if (CryptoX_Failed(CryptoX_LoadPublicKey(provider, certData[k], certDataSizes[k],
                                             &keys[k], certNames[k], &certs[k]))) {
      fclose(fp);
      fprintf(stderr, "ERROR: Could not load public key.\n");
      return CryptoX_Error;
    }
  }

  rv = mar_extract_and_verify_signatures_fp(fp, provider, keys, certCount);
  fclose(fp);

  /* Cleanup the allocated keys and certs */
  for (k = 0; k < certCount; k++) {
    if (keys[k]) {
      CryptoX_FreePublicKey(&keys[k]);
    }

    if (certs[k]) {
      CryptoX_FreeCertificate(&certs[k]);
    }
  }
  return rv;
}

#ifdef XP_WIN
/**
 * Verifies a MAR file by verifying each signature with the corresponding
 * certificate. That is, the first signature will be verified using the first
 * certificate given, the second signature will be verified using the second
 * certificate given, etc. The signature count must exactly match the number of
 * certificates given, and all signature verifications must succeed.
 * 
 * @param  pathToMARFile  The path of the MAR file who's signature
 *                        should be calculated
 * @param  certData       Pointer to the first element in an array of
 *                        certificate data
 * @param  certDataSizes  Pointer to the first element in an array for size of
 *                        the data stored
 * @param  certCount      The number of elements in certData and certDataSizes
 * @return 0 on success
*/
int
mar_verify_signaturesW(MarFile *mar,
                       const uint8_t * const *certData,
                       const uint32_t *certDataSizes,
                       uint32_t certCount) {
  int rv = -1;
  CryptoX_ProviderHandle provider = CryptoX_InvalidHandleValue;
  CryptoX_Certificate certs[MAX_SIGNATURES];
  CryptoX_PublicKey keys[MAX_SIGNATURES];
  uint32_t k;
  
  memset(certs, 0, sizeof(certs));
  memset(keys, 0, sizeof(keys));

  if (!mar || !certData || !certDataSizes || certCount == 0) {
    fprintf(stderr, "ERROR: Invalid parameter specified.\n");
    goto failure;
  }

  if (!mar->fp) {
    fprintf(stderr, "ERROR: MAR file is not open.\n");
    goto failure;
  }

  if (CryptoX_Failed(CryptoX_InitCryptoProvider(&provider))) { 
    fprintf(stderr, "ERROR: Could not init crytpo library.\n");
    goto failure;
  }

  for (k = 0; k < certCount; ++k) {
    if (CryptoX_Failed(CryptoX_LoadPublicKey(provider, certData[k], certDataSizes[k],
                                             &keys[k], "", &certs[k]))) {
      fprintf(stderr, "ERROR: Could not load public key.\n");
      goto failure;
    }
  }

  rv = mar_extract_and_verify_signatures_fp(mar->fp, provider, keys, certCount);

failure:

  for (k = 0; k < certCount; ++k) {
    if (keys[k]) {
      CryptoX_FreePublicKey(&keys[k]);
    }

    if (certs[k]) {
      CryptoX_FreeCertificate(&certs[k]);
    }
  }

  return rv;
}
#endif

/**
 * Extracts each signature from the specified MAR file,
 * then calls mar_verify_signatures_for_fp to verify each signature.
 *
 * @param  fp       An opened MAR file handle
 * @param  provider A library provider
 * @param  keys     The public keys to use to verify the MAR
 * @param  keyCount The number of keys pointed to by keys
 * @return 0 on success
*/
int
mar_extract_and_verify_signatures_fp(FILE *fp,
                                     CryptoX_ProviderHandle provider,
                                     CryptoX_PublicKey *keys,
                                     uint32_t keyCount) {
  char buf[5] = {0};
  uint32_t signatureCount, signatureLen, numVerified = 0;
  uint32_t signatureAlgorithmIDs[MAX_SIGNATURES];
  int rv = -1;
  int64_t curPos;
  uint8_t *extractedSignatures[MAX_SIGNATURES];
  uint32_t i;

  memset(signatureAlgorithmIDs, 0, sizeof(signatureAlgorithmIDs));
  memset(extractedSignatures, 0, sizeof(extractedSignatures));

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

  for (i = 0; i < signatureCount; i++) {
    /* Get the signature algorithm ID */
    if (fread(&signatureAlgorithmIDs[i], sizeof(uint32_t), 1, fp) != 1) {
      fprintf(stderr, "ERROR: Could not read signatures algorithm ID.\n");
      return CryptoX_Error;
    }
    signatureAlgorithmIDs[i] = ntohl(signatureAlgorithmIDs[i]);
  
    if (fread(&signatureLen, sizeof(uint32_t), 1, fp) != 1) {
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

    extractedSignatures[i] = malloc(signatureLen);
    if (!extractedSignatures[i]) {
      fprintf(stderr, "ERROR: Could allocate buffer for signature.\n");
      return CryptoX_Error;
    }
    if (fread(extractedSignatures[i], signatureLen, 1, fp) != 1) {
      fprintf(stderr, "ERROR: Could not read extracted signature.\n");
      for (i = 0; i < signatureCount; ++i) {
        free(extractedSignatures[i]);
      }
      return CryptoX_Error;
    }

    /* We don't try to verify signatures we don't know about */
    if (signatureAlgorithmIDs[i] != 1) {
      fprintf(stderr, "ERROR: Unknown signature algorithm ID.\n");
      for (i = 0; i < signatureCount; ++i) {
        free(extractedSignatures[i]);
      }
      return CryptoX_Error;
    }
  }

  curPos = ftello(fp);
  rv = mar_verify_signatures_for_fp(fp,
                                    provider,
                                    keys,
                                    (const uint8_t * const *)extractedSignatures,
                                    signatureCount,
                                    &numVerified);
  for (i = 0; i < signatureCount; ++i) {
    free(extractedSignatures[i]);
  }

  /* If we reached here and we verified every
     signature, return success. */
  if (numVerified == signatureCount && keyCount == numVerified) {
    return CryptoX_Success;
  }

  if (numVerified == 0) {
    fprintf(stderr, "ERROR: Not all signatures were verified.\n");
  } else {
    fprintf(stderr, "ERROR: Only %d of %d signatures were verified.\n",
            numVerified, signatureCount);
  }
  return CryptoX_Error;
}

/**
 * Verifies a MAR file by verifying each signature with the corresponding
 * certificate. That is, the first signature will be verified using the first
 * certificate given, the second signature will be verified using the second
 * certificate given, etc. The signature count must exactly match the number of
 * certificates given, and all signature verifications must succeed.
 * 
 * @param  fp                   An opened MAR file handle
 * @param  provider             A library provider
 * @param  keys                 A pointer to the first element in an
 *                              array of keys.
 * @param  extractedSignatures  Pointer to the first element in an array
 *                              of extracted signatures.
 * @param  signatureCount       The number of signatures in the MAR file
 * @param numVerified           Out parameter which will be filled with
 *                              the number of verified signatures.
 *                              This information can be useful for printing
 *                              error messages.
 * @return 0 on success, *numVerified == signatureCount.
*/
int
mar_verify_signatures_for_fp(FILE *fp,
                             CryptoX_ProviderHandle provider,
                             CryptoX_PublicKey *keys,
                             const uint8_t * const *extractedSignatures,
                             uint32_t signatureCount,
                             uint32_t *numVerified)
{
  CryptoX_SignatureHandle signatureHandles[MAX_SIGNATURES];
  char buf[BLOCKSIZE];
  uint32_t signatureLengths[MAX_SIGNATURES];
  uint32_t i;
  int rv = CryptoX_Error;

  memset(signatureHandles, 0, sizeof(signatureHandles));
  memset(signatureLengths, 0, sizeof(signatureLengths));

  if (!extractedSignatures || !numVerified) {
    fprintf(stderr, "ERROR: Invalid parameter specified.\n");
    goto failure;
  }

  *numVerified = 0;

  /* This function is only called when we have at least one signature,
     but to protected against future people who call this function we
     make sure a non zero value is passed in. 
   */
  if (!signatureCount) {
    fprintf(stderr, "ERROR: There must be at least one signature.\n");
    goto failure;
  }

  for (i = 0; i < signatureCount; i++) {
    if (CryptoX_Failed(CryptoX_VerifyBegin(provider,
                                           &signatureHandles[i], &keys[i]))) {
      fprintf(stderr, "ERROR: Could not initialize signature handle.\n");
      goto failure;
    }
  }

  /* Skip to the start of the file */
  if (fseeko(fp, 0, SEEK_SET)) {
    fprintf(stderr, "ERROR: Could not seek to start of the file\n");
    goto failure;
  }

  /* Bytes 0-3: MAR1
     Bytes 4-7: index offset 
     Bytes 8-15: size of entire MAR
   */
  if (CryptoX_Failed(ReadAndUpdateVerifyContext(fp, buf, 
                                                SIGNATURE_BLOCK_OFFSET +
                                                sizeof(uint32_t),
                                                signatureHandles,
                                                signatureCount,
                                                "signature block"))) {
    goto failure;
  }

  /* Read the signature block */
  for (i = 0; i < signatureCount; i++) {
    /* Get the signature algorithm ID */
    if (CryptoX_Failed(ReadAndUpdateVerifyContext(fp,
                                                  &buf, 
                                                  sizeof(uint32_t),
                                                  signatureHandles,
                                                  signatureCount,
                                                  "signature algorithm ID"))) {
      goto failure;
    }

    if (CryptoX_Failed(ReadAndUpdateVerifyContext(fp, 
                                                  &signatureLengths[i],
                                                  sizeof(uint32_t), 
                                                  signatureHandles,
                                                  signatureCount,
                                                  "signature length"))) {
      goto failure;
    }
    signatureLengths[i] = ntohl(signatureLengths[i]);
    if (signatureLengths[i] > MAX_SIGNATURE_LENGTH) {
      fprintf(stderr, "ERROR: Embedded signature length is too large.\n");
      goto failure;
    }

    /* Skip past the signature itself as those are not included */
    if (fseeko(fp, signatureLengths[i], SEEK_CUR)) {
      fprintf(stderr, "ERROR: Could not seek past signature.\n");
      goto failure;
    }
  }

  /* Read the rest of the file after the signature block */
  while (!feof(fp)) {
    int numRead = fread(buf, 1, BLOCKSIZE , fp);
    if (ferror(fp)) {
      fprintf(stderr, "ERROR: Error reading data block.\n");
      goto failure;
    }

    for (i = 0; i < signatureCount; i++) {
      if (CryptoX_Failed(CryptoX_VerifyUpdate(&signatureHandles[i],
                                              buf, numRead))) {
        fprintf(stderr, "ERROR: Error updating verify context with"
                        " data block.\n");
        goto failure;
      }
    }
  }

  /* Verify the signatures */
  for (i = 0; i < signatureCount; i++) {
    if (CryptoX_Failed(CryptoX_VerifySignature(&signatureHandles[i],
                                               &keys[i],
                                               extractedSignatures[i],
                                               signatureLengths[i]))) {
      fprintf(stderr, "ERROR: Error verifying signature.\n");
      goto failure;
    }
    ++*numVerified;
  }

  rv = CryptoX_Success;
failure:
  for (i = 0; i < signatureCount; i++) {
    CryptoX_FreeSignatureHandle(signatureHandles[i]);
  }

  return rv;
}
