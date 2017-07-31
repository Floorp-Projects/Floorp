/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <dlfcn.h>

#include "cryptox.h"

// We declare the necessary parts of the Security Transforms API here since
// we're building with the 10.6 SDK, which doesn't know about Security
// Transforms.
#ifdef __cplusplus
extern "C" {
#endif
  const CFStringRef kSecTransformInputAttributeName = CFSTR("INPUT");
  typedef CFTypeRef SecTransformRef;
  typedef struct OpaqueSecKeyRef* SecKeyRef;

  typedef SecTransformRef (*SecTransformCreateReadTransformWithReadStreamFunc)
                            (CFReadStreamRef inputStream);
  SecTransformCreateReadTransformWithReadStreamFunc
    SecTransformCreateReadTransformWithReadStreamPtr = NULL;
  typedef CFTypeRef (*SecTransformExecuteFunc)(SecTransformRef transform,
                                               CFErrorRef* error);
  SecTransformExecuteFunc SecTransformExecutePtr = NULL;
  typedef SecTransformRef (*SecVerifyTransformCreateFunc)(SecKeyRef key,
                                                          CFDataRef signature,
                                                          CFErrorRef* error);
  SecVerifyTransformCreateFunc SecVerifyTransformCreatePtr = NULL;
  typedef Boolean (*SecTransformSetAttributeFunc)(SecTransformRef transform,
                                                  CFStringRef key,
                                                  CFTypeRef value,
                                                  CFErrorRef* error);
  SecTransformSetAttributeFunc SecTransformSetAttributePtr = NULL;
#ifdef __cplusplus
}
#endif

CryptoX_Result
CryptoMac_InitCryptoProvider()
{
  if (!SecTransformCreateReadTransformWithReadStreamPtr) {
    SecTransformCreateReadTransformWithReadStreamPtr =
      (SecTransformCreateReadTransformWithReadStreamFunc)
        dlsym(RTLD_DEFAULT, "SecTransformCreateReadTransformWithReadStream");
  }
  if (!SecTransformExecutePtr) {
    SecTransformExecutePtr = (SecTransformExecuteFunc)
      dlsym(RTLD_DEFAULT, "SecTransformExecute");
  }
  if (!SecVerifyTransformCreatePtr) {
    SecVerifyTransformCreatePtr = (SecVerifyTransformCreateFunc)
      dlsym(RTLD_DEFAULT, "SecVerifyTransformCreate");
  }
  if (!SecTransformSetAttributePtr) {
    SecTransformSetAttributePtr = (SecTransformSetAttributeFunc)
      dlsym(RTLD_DEFAULT, "SecTransformSetAttribute");
  }
  if (!SecTransformCreateReadTransformWithReadStreamPtr ||
      !SecTransformExecutePtr ||
      !SecVerifyTransformCreatePtr ||
      !SecTransformSetAttributePtr) {
    return CryptoX_Error;
  }
  return CryptoX_Success;
}

CryptoX_Result
CryptoMac_VerifyBegin(CryptoX_SignatureHandle* aInputData)
{
  if (!aInputData) {
    return CryptoX_Error;
  }

  void* inputData = CFDataCreateMutable(kCFAllocatorDefault, 0);
  if (!inputData) {
    return CryptoX_Error;
  }

  *aInputData = inputData;
  return CryptoX_Success;
}

CryptoX_Result
CryptoMac_VerifyUpdate(CryptoX_SignatureHandle* aInputData, void* aBuf,
                       unsigned int aLen)
{
  if (aLen == 0) {
    return CryptoX_Success;
  }
  if (!aInputData || !*aInputData) {
    return CryptoX_Error;
  }

  CFMutableDataRef inputData = (CFMutableDataRef)*aInputData;

  CFDataAppendBytes(inputData, (const uint8*)aBuf, aLen);
  return CryptoX_Success;
}

CryptoX_Result
CryptoMac_LoadPublicKey(const unsigned char* aCertData,
                        unsigned int aDataSize,
                        CryptoX_PublicKey* aPublicKey)
{
  if (!aCertData || aDataSize == 0 || !aPublicKey) {
    return CryptoX_Error;
  }
  *aPublicKey = NULL;
  CFDataRef certData = CFDataCreate(kCFAllocatorDefault,
                                    aCertData,
                                    aDataSize);
  if (!certData) {
    return CryptoX_Error;
  }

  SecCertificateRef cert = SecCertificateCreateWithData(kCFAllocatorDefault,
                                                        certData);
  CFRelease(certData);
  if (!cert) {
    return CryptoX_Error;
  }

  OSStatus status = SecCertificateCopyPublicKey(cert,
                                                (SecKeyRef*)aPublicKey);
  CFRelease(cert);
  if (status != 0) {
    return CryptoX_Error;
  }

  return CryptoX_Success;
}

CryptoX_Result
CryptoMac_VerifySignature(CryptoX_SignatureHandle* aInputData,
                          CryptoX_PublicKey* aPublicKey,
                          const unsigned char* aSignature,
                          unsigned int aSignatureLen)
{
  if (!aInputData || !*aInputData || !aPublicKey || !*aPublicKey ||
      !aSignature || aSignatureLen == 0) {
    return CryptoX_Error;
  }

  CFDataRef signatureData = CFDataCreate(kCFAllocatorDefault,
                                         aSignature, aSignatureLen);
  if (!signatureData) {
    return CryptoX_Error;
  }

  CFErrorRef error;
  SecTransformRef verifier =
    SecVerifyTransformCreatePtr((SecKeyRef)*aPublicKey,
                                signatureData,
                                &error);
  if (!verifier || error) {
    if (error) {
      CFRelease(error);
    }
    CFRelease(signatureData);
    return CryptoX_Error;
  }

  SecTransformSetAttributePtr(verifier,
                              kSecDigestTypeAttribute,
                              kSecDigestSHA2,
                              &error);
  if (error) {
    CFRelease(error);
    CFRelease(signatureData);
    CFRelease(verifier);
    return CryptoX_Error;
  }

  int digestLength = 384;
  CFNumberRef dLen = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &digestLength);
  SecTransformSetAttributePtr(verifier,
                              kSecDigestLengthAttribute,
                              dLen,
                              &error);
  CFRelease(dLen);
  if (error) {
    CFRelease(error);
    CFRelease(signatureData);
    CFRelease(verifier);
    return CryptoX_Error;
  }

  SecTransformSetAttributePtr(verifier,
                              kSecTransformInputAttributeName,
                              (CFDataRef)*aInputData,
                              &error);
  if (error) {
    CFRelease(error);
    CFRelease(signatureData);
    CFRelease(verifier);
    return CryptoX_Error;
  }

  CryptoX_Result result = CryptoX_Error;
  CFTypeRef rv = SecTransformExecutePtr(verifier, &error);
  if (error) {
    CFRelease(error);
    CFRelease(signatureData);
    CFRelease(verifier);
    return CryptoX_Error;
  }

  if (CFGetTypeID(rv) == CFBooleanGetTypeID() &&
      CFBooleanGetValue((CFBooleanRef)rv) == true) {
    result = CryptoX_Success;
  }

  CFRelease(signatureData);
  CFRelease(verifier);

  return result;
}

void
CryptoMac_FreeSignatureHandle(CryptoX_SignatureHandle* aInputData)
{
  if (!aInputData || !*aInputData) {
    return;
  }

  CFMutableDataRef inputData = NULL;
  inputData = (CFMutableDataRef)*aInputData;

  CFRelease(inputData);
}

void
CryptoMac_FreePublicKey(CryptoX_PublicKey* aPublicKey)
{
  if (!aPublicKey || !*aPublicKey) {
    return;
  }

  CFRelease((SecKeyRef)*aPublicKey);
}
