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
#include "mar_cmdline.h"
#include "mar.h"
#include "cryptox.h"
#ifndef XP_WIN
#include <unistd.h>
#endif

#include "nss_secutil.h"

/**
 * Initializes the NSS context.
 * 
 * @param NSSConfigDir The config dir containing the private key to use
 * @return 0 on success
 *         -1 on error
*/
int
NSSInitCryptoContext(const char *NSSConfigDir)
{
  SECStatus status = NSS_Initialize(NSSConfigDir, 
                                    "", "", SECMOD_DB, NSS_INIT_READONLY);
  if (SECSuccess != status) {
    fprintf(stderr, "ERROR: Could not initialize NSS\n");
    return -1;
  }

  return 0;
}

/** 
 * Obtains a signing context.
 *
 * @param  ctx A pointer to the signing context to fill
 * @return 0 on success
 *         -1 on error
*/
int
NSSSignBegin(const char *certName, 
             SGNContext **ctx, 
             SECKEYPrivateKey **privKey, 
             CERTCertificate **cert,
             PRUint32 *signatureLength) 
{
  secuPWData pwdata = { PW_NONE, 0 };
  if (!certName || !ctx || !privKey || !cert || !signatureLength) {
    fprintf(stderr, "ERROR: Invalid parameter passed to NSSSignBegin\n");
    return -1;
  }

  /* Get the cert and embedded public key out of the database */
  *cert = PK11_FindCertFromNickname(certName, &pwdata);
  if (!*cert) {
    fprintf(stderr, "ERROR: Could not find cert from nickname\n");
    return -1;
  }

  /* Get the private key out of the database */
  *privKey = PK11_FindKeyByAnyCert(*cert, &pwdata);
  if (!*privKey) {
    fprintf(stderr, "ERROR: Could not find private key\n");
    return -1;
  }

  *signatureLength = PK11_SignatureLen(*privKey);

  if (*signatureLength > BLOCKSIZE) {
    fprintf(stderr, 
            "ERROR: Program must be compiled with a larger block size"
            " to support signing with signatures this large: %u.\n", 
            *signatureLength);
    return -1;
  }

  /* Check that the key length is large enough for our requirements */
  if (*signatureLength < XP_MIN_SIGNATURE_LEN_IN_BYTES) {
    fprintf(stderr, "ERROR: Key length must be >= %d bytes\n", 
            XP_MIN_SIGNATURE_LEN_IN_BYTES);
    return -1;
  }

  *ctx = SGN_NewContext (SEC_OID_ISO_SHA1_WITH_RSA_SIGNATURE, *privKey);
  if (!*ctx) {
    fprintf(stderr, "ERROR: Could not create signature context\n");
    return -1;
  }
  
  if (SGN_Begin(*ctx) != SECSuccess) {
    fprintf(stderr, "ERROR: Could not begin signature\n");
    return -1;
  }
  
  return 0;
}

/**
 * Writes the passed buffer to the file fp and updates the signature context.
 *
 * @param  fpDest The file pointer to write to.
 * @param  buffer The buffer to write.
 * @param  size   The size of the buffer to write.
 * @param  ctx    The signature context.
 * @param  err    The name of what is being written to in case of error.
 * @return  0 on success
 *         -2 on write error
 *         -3 on signature update error
*/
int
WriteAndUpdateSignature(FILE *fpDest, void *buffer, 
                        PRUint32 size, SGNContext *ctx,
                        const char *err) 
{
  if (!size) { 
    return 0;
  }

  if (fwrite(buffer, size, 1, fpDest) != 1) {
    fprintf(stderr, "ERROR: Could not write %s\n", err);
    return -2;
  }
  if (SGN_Update(ctx, (const unsigned char *)buffer, size) != SECSuccess) {
    fprintf(stderr, "ERROR: Could not update signature context for %s\n", err);
    return -3;
  }
  return 0;
}

/** 
 * Adjusts each entry's content offset in the the passed in index by the 
 * specified amount.
 *
 * @param indexBuf     A buffer containing the MAR index
 * @param indexLength  The length of the MAR index
 * @param offsetAmount The amount to adjust each index entry by
*/
void
AdjustIndexContentOffsets(char *indexBuf, PRUint32 indexLength, PRUint32 offsetAmount) 
{
  PRUint32 *offsetToContent;
  char *indexBufLoc = indexBuf;

  /* Consume the index and adjust each index by the specified amount */
  while (indexBufLoc != (indexBuf + indexLength)) {
    /* Adjust the offset */
    offsetToContent = (PRUint32 *)indexBufLoc; 
    *offsetToContent = ntohl(*offsetToContent);
    *offsetToContent += offsetAmount;
    *offsetToContent = htonl(*offsetToContent);
    /* Skip past the offset, length, and flags */
    indexBufLoc += 3 * sizeof(PRUint32);
    indexBufLoc += strlen(indexBufLoc) + 1;
  }
}

/**
 * Reads from fpSrc, writes it to fpDest, and updates the signature context.
 *
 * @param  fpSrc  The file pointer to read from.
 * @param  fpDest The file pointer to write to.
 * @param  buffer The buffer to write.
 * @param  size   The size of the buffer to write.
 * @param  ctx    The signature context.
 * @param  err    The name of what is being written to in case of error.
 * @return  0 on success
 *         -1 on read error
 *         -2 on write error
 *         -3 on signature update error
*/
int
ReadWriteAndUpdateSignature(FILE *fpSrc, FILE *fpDest, void *buffer, 
                            PRUint32 size, SGNContext *ctx,
                            const char *err) 
{
  if (!size) { 
    return 0;
  }

  if (fread(buffer, size, 1, fpSrc) != 1) {
    fprintf(stderr, "ERROR: Could not read %s\n", err);
    return -1;
  }

  return WriteAndUpdateSignature(fpDest, buffer, size, ctx, err);
}


/**
 * Reads from fpSrc, writes it to fpDest.
 *
 * @param  fpSrc  The file pointer to read from.
 * @param  fpDest The file pointer to write to.
 * @param  buffer The buffer to write.
 * @param  size   The size of the buffer to write.
 * @param  err    The name of what is being written to in case of error.
 * @return  0 on success
 *         -1 on read error
 *         -2 on write error
*/
int
ReadAndWrite(FILE *fpSrc, FILE *fpDest, void *buffer, 
             PRUint32 size, const char *err) 
{
  if (!size) { 
    return 0;
  }

  if (fread(buffer, size, 1, fpSrc) != 1) {
    fprintf(stderr, "ERROR: Could not read %s\n", err);
    return -1;
  }

  if (fwrite(buffer, size, 1, fpDest) != 1) {
    fprintf(stderr, "ERROR: Could not write %s\n", err);
    return -2;
  }

  return 0;
}

/**
 * Writes out a copy of the MAR at src but with the signature block stripped.
 *
 * @param  src  The path of the source MAR file
 * @param  dest The path of the MAR file to write out that 
                has no signature block
 * @return 0 on success
 *         -1 on error
*/
int
strip_signature_block(const char *src, const char * dest)
{
  PRUint32 offsetToIndex, dstOffsetToIndex, indexLength, 
    numSignatures = 0, leftOver;
  PRInt32 stripAmount = 0;
  PRInt64 oldPos, sizeOfEntireMAR = 0, realSizeOfSrcMAR, numBytesToCopy,
    numChunks, i;
  FILE *fpSrc = NULL, *fpDest = NULL;
  int rv = -1, hasSignatureBlock;
  char buf[BLOCKSIZE];
  char *indexBuf = NULL, *indexBufLoc;

  if (!src || !dest) {
    fprintf(stderr, "ERROR: Invalid parameter passed in.\n");
    return -1;
  }

  fpSrc = fopen(src, "rb");
  if (!fpSrc) {
    fprintf(stderr, "ERROR: could not open source file: %s\n", dest);
    goto failure;
  }

  fpDest = fopen(dest, "wb");
  if (!fpDest) {
    fprintf(stderr, "ERROR: could not create target file: %s\n", dest);
    goto failure;
  }

  /* Determine if the source MAR file has the new fields for signing or not */
  if (get_mar_file_info(src, &hasSignatureBlock, NULL, NULL, NULL, NULL)) {
    fprintf(stderr, "ERROR: could not determine if MAR is old or new.\n");
    goto failure;
  }

  /* MAR ID */
  if (ReadAndWrite(fpSrc, fpDest, buf, MAR_ID_SIZE, "MAR ID")) {
    goto failure;
  }

  /* Offset to index */
  if (fread(&offsetToIndex, sizeof(offsetToIndex), 1, fpSrc) != 1) {
    fprintf(stderr, "ERROR: Could not read offset\n");
    goto failure;
  }
  offsetToIndex = ntohl(offsetToIndex);

  /* Get the real size of the MAR */
  oldPos = ftello(fpSrc);
  if (fseeko(fpSrc, 0, SEEK_END)) {
    fprintf(stderr, "ERROR: Could not seek to end of file.\n");
    goto failure;
  }
  realSizeOfSrcMAR = ftello(fpSrc);
  if (fseeko(fpSrc, oldPos, SEEK_SET)) {
    fprintf(stderr, "ERROR: Could not seek back to current location.\n");
    goto failure;
  }

  if (hasSignatureBlock) {
    /* Get the MAR length and adjust its size */
    if (fread(&sizeOfEntireMAR, 
              sizeof(sizeOfEntireMAR), 1, fpSrc) != 1) {
      fprintf(stderr, "ERROR: Could read mar size\n");
      goto failure;
    }
    sizeOfEntireMAR = NETWORK_TO_HOST64(sizeOfEntireMAR);
    if (sizeOfEntireMAR != realSizeOfSrcMAR) {
      fprintf(stderr, "ERROR: Source MAR is not of the right size\n");
      goto failure;
    }
  
    /* Get the num signatures in the source file so we know what to strip */
    if (fread(&numSignatures, sizeof(numSignatures), 1, fpSrc) != 1) {
      fprintf(stderr, "ERROR: Could read num signatures\n");
      goto failure;
    }
    numSignatures = ntohl(numSignatures);

    for (i = 0; i < numSignatures; i++) {
      PRUint32 signatureLen;

      /* Skip past the signature algorithm ID */
      if (fseeko(fpSrc, sizeof(PRUint32), SEEK_CUR)) {
        fprintf(stderr, "ERROR: Could not skip past signature algorithm ID\n");
      }

      /* Read in the length of the signature so we know how far to skip */
      if (fread(&signatureLen, sizeof(PRUint32), 1, fpSrc) != 1) {
        fprintf(stderr, "ERROR: Could not read signatures length.\n");
        return CryptoX_Error;
      }
      signatureLen = ntohl(signatureLen);

      /* Skip past the signature */
      if (fseeko(fpSrc, signatureLen, SEEK_CUR)) {
        fprintf(stderr, "ERROR: Could not skip past signature algorithm ID\n");
      }

      stripAmount += sizeof(PRUint32) + sizeof(PRUint32) + signatureLen; 
    }

  } else {
    sizeOfEntireMAR = realSizeOfSrcMAR;
    numSignatures = 0;
  }

  if (((PRInt64)offsetToIndex) > sizeOfEntireMAR) {
    fprintf(stderr, "ERROR: Offset to index is larger than the file size.\n");
    goto failure;
  }

  dstOffsetToIndex = offsetToIndex;
  if (!hasSignatureBlock) {
    dstOffsetToIndex += sizeof(sizeOfEntireMAR) + sizeof(numSignatures);
  }
  dstOffsetToIndex -= stripAmount;

  /* Write out the index offset */
  dstOffsetToIndex = htonl(dstOffsetToIndex);
  if (fwrite(&dstOffsetToIndex, sizeof(dstOffsetToIndex), 1, fpDest) != 1) {
    fprintf(stderr, "ERROR: Could not write offset to index\n");
    goto failure;
  }
  dstOffsetToIndex = ntohl(dstOffsetToIndex);

  /* Write out the new MAR file size */
  if (!hasSignatureBlock) {
    sizeOfEntireMAR += sizeof(sizeOfEntireMAR) + sizeof(numSignatures);
  }
  sizeOfEntireMAR -= stripAmount;

  /* Write out the MAR size */
  sizeOfEntireMAR = HOST_TO_NETWORK64(sizeOfEntireMAR);
  if (fwrite(&sizeOfEntireMAR, sizeof(sizeOfEntireMAR), 1, fpDest) != 1) {
    fprintf(stderr, "ERROR: Could not write size of MAR\n");
    goto failure;
  }
  sizeOfEntireMAR = NETWORK_TO_HOST64(sizeOfEntireMAR);

  /* Write out the number of signatures, which is 0 */
  numSignatures = 0;
  if (fwrite(&numSignatures, sizeof(numSignatures), 1, fpDest) != 1) {
    fprintf(stderr, "ERROR: Could not write out num signatures\n");
    goto failure;
  }

  /* Write out the rest of the MAR excluding the index header and index
     offsetToIndex unfortunately has to remain 32-bit because for backwards
     compatibility with the old MAR file format. */
  if (ftello(fpSrc) > ((PRInt64)offsetToIndex)) {
    fprintf(stderr, "ERROR: Index offset is too small.\n");
    goto failure;
  }
  numBytesToCopy = ((PRInt64)offsetToIndex) - ftello(fpSrc);
  numChunks = numBytesToCopy / BLOCKSIZE;
  leftOver = numBytesToCopy % BLOCKSIZE;

  /* Read each file and write it to the MAR file */
  for (i = 0; i < numChunks; ++i) {
    if (ReadAndWrite(fpSrc, fpDest, buf, BLOCKSIZE, "content block")) {
      goto failure;
    }
  }

  /* Write out the left over */
  if (ReadAndWrite(fpSrc, fpDest, buf, 
                   leftOver, "left over content block")) {
    goto failure;
  }

  /* Length of the index */
  if (ReadAndWrite(fpSrc, fpDest, &indexLength, 
                   sizeof(indexLength), "index length")) {
    goto failure;
  }
  indexLength = ntohl(indexLength);

  /* Consume the index and adjust each index by the difference */
  indexBuf = malloc(indexLength);
  indexBufLoc = indexBuf;
  if (fread(indexBuf, indexLength, 1, fpSrc) != 1) {
    fprintf(stderr, "ERROR: Could not read index\n");
    goto failure;
  }

  /* Adjust each entry in the index */
  if (hasSignatureBlock) {
    AdjustIndexContentOffsets(indexBuf, indexLength, -stripAmount);
  } else {
    AdjustIndexContentOffsets(indexBuf, indexLength, 
                              sizeof(sizeOfEntireMAR) + 
                              sizeof(numSignatures) - 
                              stripAmount);
  }

  if (fwrite(indexBuf, indexLength, 1, fpDest) != 1) {
    fprintf(stderr, "ERROR: Could not write index\n");
    goto failure;
  }

  rv = 0;
failure: 
  if (fpSrc) {
    fclose(fpSrc);
  }

  if (fpDest) {
    fclose(fpDest);
  }

  if (rv) {
    remove(dest);
  }

  if (indexBuf) { 
    free(indexBuf);
  }

  if (rv) {
    remove(dest);
  }
  return rv;
}

/**
 * Writes out a copy of the MAR at src but with an embedded signature.
 * The passed in MAR file must not already be signed or an error will 
 * be returned.
 *
 * @param  NSSConfigDir The NSS directory containing the private key for signing
 * @param  certName     The nickname of the certificate to use for signing
 * @param  src          The path of the source MAR file to sign
 * @param  dest         The path of the MAR file to write out that is signed
 * @return 0 on success
 *         -1 on error
*/
int
mar_repackage_and_sign(const char *NSSConfigDir, 
                       const char *certName, 
                       const char *src, 
                       const char *dest) 
{
  PRUint32 offsetToIndex, dstOffsetToIndex, indexLength, 
    numSignatures = 0, signatureLength, leftOver,
    signatureAlgorithmID, signatureSectionLength;
  PRInt64 oldPos, sizeOfEntireMAR = 0, realSizeOfSrcMAR, 
    signaturePlaceholderOffset, numBytesToCopy, 
    numChunks, i;
  FILE *fpSrc = NULL, *fpDest = NULL;
  int rv = -1, hasSignatureBlock;
  SGNContext *ctx = NULL;
  SECItem secItem;
  char buf[BLOCKSIZE];
  SECKEYPrivateKey *privKey = NULL;
  CERTCertificate *cert = NULL; 
  char *indexBuf = NULL, *indexBufLoc;

  if (!NSSConfigDir || !certName || !src || !dest) {
    fprintf(stderr, "ERROR: Invalid parameter passed in.\n");
    return -1;
  }

  if (NSSInitCryptoContext(NSSConfigDir)) {
    fprintf(stderr, "ERROR: Could not init config dir: %s\n", NSSConfigDir);
    goto failure;
  }

  PK11_SetPasswordFunc(SECU_GetModulePassword);

  if (NSSSignBegin(certName, &ctx, &privKey, &cert, &signatureLength)) {
    fprintf(stderr, "ERROR: NSSSignBegin failed\n");
    goto failure;
  }
  
  fpSrc = fopen(src, "rb");
  if (!fpSrc) {
    fprintf(stderr, "ERROR: could not open source file: %s\n", dest);
    goto failure;
  }

  fpDest = fopen(dest, "wb");
  if (!fpDest) {
    fprintf(stderr, "ERROR: could not create target file: %s\n", dest);
    goto failure;
  }

  /* Determine if the source MAR file has the new fields for signing or not */
  if (get_mar_file_info(src, &hasSignatureBlock, NULL, NULL, NULL, NULL)) {
    fprintf(stderr, "ERROR: could not determine if MAR is old or new.\n");
    goto failure;
  }

  /* MAR ID */
  if (ReadWriteAndUpdateSignature(fpSrc, fpDest, 
                                  buf, MAR_ID_SIZE, 
                                  ctx, "MAR ID")) {
    goto failure;
  }

  /* Offset to index */
  if (fread(&offsetToIndex, sizeof(offsetToIndex), 1, fpSrc) != 1) {
    fprintf(stderr, "ERROR: Could not read offset\n");
    goto failure;
  }
  offsetToIndex = ntohl(offsetToIndex);

  /* Get the real size of the MAR */
  oldPos = ftello(fpSrc);
  if (fseeko(fpSrc, 0, SEEK_END)) {
    fprintf(stderr, "ERROR: Could not seek to end of file.\n");
    goto failure;
  }
  realSizeOfSrcMAR = ftello(fpSrc);
  if (fseeko(fpSrc, oldPos, SEEK_SET)) {
    fprintf(stderr, "ERROR: Could not seek back to current location.\n");
    goto failure;
  }

  if (hasSignatureBlock) {
    /* Get the MAR length and adjust its size */
    if (fread(&sizeOfEntireMAR, 
              sizeof(sizeOfEntireMAR), 1, fpSrc) != 1) {
      fprintf(stderr, "ERROR: Could read mar size\n");
      goto failure;
    }
    sizeOfEntireMAR = NETWORK_TO_HOST64(sizeOfEntireMAR);
    if (sizeOfEntireMAR != realSizeOfSrcMAR) {
      fprintf(stderr, "ERROR: Source MAR is not of the right size\n");
      goto failure;
    }
  
    /* Get the num signatures in the source file so we know what to skip over */
    if (fread(&numSignatures, sizeof(numSignatures), 1, fpSrc) != 1) {
      fprintf(stderr, "ERROR: Could read num signatures\n");
      goto failure;
    }
    numSignatures = ntohl(numSignatures);

    /* We do not support resigning */
    if (numSignatures) {
      fprintf(stderr, "ERROR: MAR is already signed\n");
      goto failure;
    }
  } else {
    sizeOfEntireMAR = realSizeOfSrcMAR;
  }

  if (((PRInt64)offsetToIndex) > sizeOfEntireMAR) {
    fprintf(stderr, "ERROR: Offset to index is larger than the file size.\n");
    goto failure;
  }

  /* Write out the new offset to the index */
  signatureSectionLength = sizeof(signatureAlgorithmID) + 
                           sizeof(signatureLength) +
                           signatureLength;
  dstOffsetToIndex = offsetToIndex;
  if (!hasSignatureBlock) {
    dstOffsetToIndex += sizeof(sizeOfEntireMAR) + sizeof(numSignatures);
  }
  dstOffsetToIndex += signatureSectionLength;

  /* Write out the index offset */
  dstOffsetToIndex = htonl(dstOffsetToIndex);
  if (WriteAndUpdateSignature(fpDest, &dstOffsetToIndex, 
                              sizeof(dstOffsetToIndex), ctx, "index offset")) {
    goto failure;
  }
  dstOffsetToIndex = ntohl(dstOffsetToIndex);

  /* Write out the new MAR file size */
  sizeOfEntireMAR += signatureSectionLength;
  if (!hasSignatureBlock) {
    sizeOfEntireMAR += sizeof(sizeOfEntireMAR) + sizeof(numSignatures);
  }

  /* Write out the MAR size */
  sizeOfEntireMAR = HOST_TO_NETWORK64(sizeOfEntireMAR);
  if (WriteAndUpdateSignature(fpDest, &sizeOfEntireMAR, 
                              sizeof(sizeOfEntireMAR), ctx, "size of MAR")) {
    goto failure;
  }
  sizeOfEntireMAR = NETWORK_TO_HOST64(sizeOfEntireMAR);

  /* Write out the number of signatures, for now only 1 is supported */
  numSignatures = 1;
  numSignatures = htonl(numSignatures);
  if (WriteAndUpdateSignature(fpDest, &numSignatures, 
                              sizeof(numSignatures), ctx, "num signatures")) {
    goto failure;
  }
  numSignatures = ntohl(numSignatures);

  /* Write out the signature ID, for now only an ID of 1 is supported */
  signatureAlgorithmID = htonl(1);
  if (WriteAndUpdateSignature(fpDest, &signatureAlgorithmID, 
                              sizeof(signatureAlgorithmID), 
                              ctx, "num signatures")) {
    goto failure;
  }
  signatureAlgorithmID = ntohl(signatureAlgorithmID);

  /* Write out the signature length */
  signatureLength = htonl(signatureLength);
  if (WriteAndUpdateSignature(fpDest, &signatureLength, 
                              sizeof(signatureLength), 
                              ctx, "signature length")) {
    goto failure;
  }
  signatureLength = ntohl(signatureLength);

  /* Write out a placeholder for the signature, we'll come back to this later
     *** THIS IS NOT SIGNED because it is a placeholder that will be replaced
         below, plus it is going to be the signature itself. *** */
  memset(buf, 0, sizeof(buf));
  signaturePlaceholderOffset = ftello(fpDest);
  if (fwrite(buf, signatureLength, 1, fpDest) != 1) {
    fprintf(stderr, "ERROR: Could not write signature length\n");
    goto failure;
  }

  /* Write out the rest of the MAR excluding the index header and index
     offsetToIndex unfortunately has to remain 32-bit because for backwards
     compatibility with the old MAR file format. */
  if (ftello(fpSrc) > ((PRInt64)offsetToIndex)) {
    fprintf(stderr, "ERROR: Index offset is too small.\n");
    goto failure;
  }
  numBytesToCopy = ((PRInt64)offsetToIndex) - ftello(fpSrc);
  numChunks = numBytesToCopy / BLOCKSIZE;
  leftOver = numBytesToCopy % BLOCKSIZE;

  /* Read each file and write it to the MAR file */
  for (i = 0; i < numChunks; ++i) {
    if (ReadWriteAndUpdateSignature(fpSrc, fpDest, buf, 
                                    BLOCKSIZE, ctx, "content block")) {
      goto failure;
    }
  }

  /* Write out the left over */
  if (ReadWriteAndUpdateSignature(fpSrc, fpDest, buf, 
                                  leftOver, ctx, "left over content block")) {
    goto failure;
  }

  /* Length of the index */
  if (ReadWriteAndUpdateSignature(fpSrc, fpDest, &indexLength, 
                                  sizeof(indexLength), ctx, "index length")) {
    goto failure;
  }
  indexLength = ntohl(indexLength);

  /* Consume the index and adjust each index by signatureSectionLength */
  indexBuf = malloc(indexLength);
  indexBufLoc = indexBuf;
  if (fread(indexBuf, indexLength, 1, fpSrc) != 1) {
    fprintf(stderr, "ERROR: Could not read index\n");
    goto failure;
  }

  /* Adjust each entry in the index */
  if (hasSignatureBlock) {
    AdjustIndexContentOffsets(indexBuf, indexLength, signatureSectionLength);
  } else {
    AdjustIndexContentOffsets(indexBuf, indexLength, 
                              sizeof(sizeOfEntireMAR) + 
                              sizeof(numSignatures) + 
                              signatureSectionLength);
  }

  if (WriteAndUpdateSignature(fpDest, indexBuf, 
                              indexLength, ctx, "index")) {
    goto failure;
  }

  /* Ensure that we don't sign a file that is too large to be accepted by
     the verification function. */
  if (ftello(fpDest) > MAX_SIZE_OF_MAR_FILE) {
    goto failure;
  }

  /* Get the signature */
  if (SGN_End(ctx, &secItem) != SECSuccess) {
    fprintf(stderr, "ERROR: Could not end signature context\n");
    goto failure;
  }
  if (signatureLength != secItem.len) {
    fprintf(stderr, "ERROR: Signature is not the expected length\n");
    goto failure;
  }

  /* Get back to the location of the signature placeholder */
  if (fseeko(fpDest, signaturePlaceholderOffset, SEEK_SET)) {
    fprintf(stderr, "ERROR: Could not seek to signature offset\n");
    goto failure;
  }

  /* Write out the calculated signature.
     *** THIS IS NOT SIGNED because it is the signature itself. *** */
  if (fwrite(secItem.data, secItem.len, 1, fpDest) != 1) {
    fprintf(stderr, "ERROR: Could not write signature\n");
    goto failure;
  }

  rv = 0;
failure: 
  if (fpSrc) {
    fclose(fpSrc);
  }

  if (fpDest) {
    fclose(fpDest);
  }

  if (rv) {
    remove(dest);
  }

  if (indexBuf) { 
    free(indexBuf);
  }

  if (ctx) {
    SGN_DestroyContext(ctx, PR_TRUE);
  }

  if (cert) {
    CERT_DestroyCertificate(cert);
  }

  if (privKey) {
    SECKEY_DestroyPrivateKey(privKey);
  }

  if (rv) {
    remove(dest);
  }
  return rv;
}
