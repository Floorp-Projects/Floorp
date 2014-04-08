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
  typedef SecCertificateRef (*SecCertificateCreateWithDataFunc)
                              (CFAllocatorRef allocator,
                               CFDataRef data);
  SecCertificateCreateWithDataFunc SecCertificateCreateWithDataPtr = NULL;
  typedef OSStatus (*SecCertificateCopyPublicKeyFunc)
                     (SecCertificateRef certificate,
                      SecKeyRef* key);
  SecCertificateCopyPublicKeyFunc SecCertificateCopyPublicKeyPtr = NULL;
#ifdef __cplusplus
}
#endif

#define MAC_OS_X_VERSION_10_7_HEX 0x00001070

static int sOnLionOrLater = -1;

static bool OnLionOrLater()
{
  if (sOnLionOrLater < 0) {
    SInt32 major = 0, minor = 0;

    CFURLRef url =
      CFURLCreateWithString(kCFAllocatorDefault,
                            CFSTR("file:///System/Library/CoreServices/SystemVersion.plist"),
                            NULL);
    CFReadStreamRef stream =
      CFReadStreamCreateWithFile(kCFAllocatorDefault, url);
    CFReadStreamOpen(stream);
    CFDictionaryRef sysVersionPlist = (CFDictionaryRef)
      CFPropertyListCreateWithStream(kCFAllocatorDefault,
                                     stream, 0, kCFPropertyListImmutable,
                                     NULL, NULL);
    CFReadStreamClose(stream);
    CFRelease(stream);
    CFRelease(url);

    CFStringRef versionString = (CFStringRef)
      CFDictionaryGetValue(sysVersionPlist, CFSTR("ProductVersion"));
    CFArrayRef versions =
      CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault,
                                             versionString, CFSTR("."));
    CFIndex count = CFArrayGetCount(versions);
    if (count > 0) {
      CFStringRef component = (CFStringRef) CFArrayGetValueAtIndex(versions, 0);
      major = CFStringGetIntValue(component);
      if (count > 1) {
        component = (CFStringRef) CFArrayGetValueAtIndex(versions, 1);
        minor = CFStringGetIntValue(component);
      }
    }
    CFRelease(sysVersionPlist);
    CFRelease(versions);

    if (major < 10) {
      sOnLionOrLater = 0;
    } else {
      int version = 0x1000 + (minor << 4);
      sOnLionOrLater = version >= MAC_OS_X_VERSION_10_7_HEX ? 1 : 0;
    }
  }

  return sOnLionOrLater > 0 ? true : false;
}

CryptoX_Result
CryptoMac_InitCryptoProvider()
{
  if (!OnLionOrLater()) {
    return CryptoX_Success;
  }

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
  if (!SecCertificateCreateWithDataPtr) {
    SecCertificateCreateWithDataPtr = (SecCertificateCreateWithDataFunc)
      dlsym(RTLD_DEFAULT, "SecCertificateCreateWithData");
  }
  if (!SecCertificateCopyPublicKeyPtr) {
    SecCertificateCopyPublicKeyPtr = (SecCertificateCopyPublicKeyFunc)
      dlsym(RTLD_DEFAULT, "SecCertificateCopyPublicKey");
  }
  if (!SecTransformCreateReadTransformWithReadStreamPtr ||
      !SecTransformExecutePtr ||
      !SecVerifyTransformCreatePtr ||
      !SecTransformSetAttributePtr ||
      !SecCertificateCreateWithDataPtr ||
      !SecCertificateCopyPublicKeyPtr) {
    return CryptoX_Error;
  }
  return CryptoX_Success;
}

CryptoX_Result
CryptoMac_VerifyBegin(CryptoX_SignatureHandle* aInputData,
                      CryptoX_PublicKey* aPublicKey)
{
  if (!OnLionOrLater()) {
    return NSS_VerifyBegin((VFYContext**)aInputData,
                           (SECKEYPublicKey* const*)aPublicKey);
  }

  (void)aPublicKey;
  if (!aInputData) {
    return CryptoX_Error;
  }

  CryptoX_Result result = CryptoX_Error;
  *aInputData = CFDataCreateMutable(kCFAllocatorDefault, 0);
  if (*aInputData) {
    result = CryptoX_Success;
  }

  return result;
}

CryptoX_Result
CryptoMac_VerifyUpdate(CryptoX_SignatureHandle* aInputData, void* aBuf,
                       unsigned int aLen)
{
  if (!OnLionOrLater()) {
    return VFY_Update((VFYContext*)*aInputData,
                      (const unsigned char*)aBuf, aLen);
  }

  if (aLen == 0) {
    return CryptoX_Success;
  }
  if (!aInputData || !*aInputData) {
    return CryptoX_Error;
  }

  CryptoX_Result result = CryptoX_Error;
  CFDataAppendBytes((CFMutableDataRef)*aInputData, (const UInt8 *) aBuf, aLen);
  if (*aInputData) {
    result = CryptoX_Success;
  }

  return result;
}

CryptoX_Result
CryptoMac_LoadPublicKey(const unsigned char* aCertData,
                        CryptoX_PublicKey* aPublicKey,
                        const char* aCertName,
                        CryptoX_Certificate* aCert)
{
  if (!aPublicKey ||
      (OnLionOrLater() && !aCertData) ||
      (!OnLionOrLater() && !aCertName)) {
    return CryptoX_Error;
  }

  if (!OnLionOrLater()) {
    return NSS_LoadPublicKey(aCertName,
                             (SECKEYPublicKey**)aPublicKey,
                             (CERTCertificate**)aCert);
  }

  CFURLRef url =
    CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
                                            aCertData,
                                            strlen((char*)aCertData),
                                            false);
  if (!url) {
    return CryptoX_Error;
  }

  CFReadStreamRef stream = CFReadStreamCreateWithFile(kCFAllocatorDefault, url);
  if (!stream) {
    CFRelease(url);
    return CryptoX_Error;
  }

  SecTransformRef readTransform =
    SecTransformCreateReadTransformWithReadStreamPtr(stream);
  if (!readTransform) {
    CFRelease(url);
    CFRelease(stream);
    return CryptoX_Error;
  }

  CFErrorRef error;
  CFDataRef tempCertData = (CFDataRef)SecTransformExecutePtr(readTransform,
                                                             &error);
  if (!tempCertData || error) {
    CFRelease(url);
    CFRelease(stream);
    CFRelease(readTransform);
    return CryptoX_Error;
  }

  SecCertificateRef cert = SecCertificateCreateWithDataPtr(kCFAllocatorDefault,
                                                           tempCertData);
  if (!cert) {
    CFRelease(url);
    CFRelease(stream);
    CFRelease(readTransform);
    CFRelease(tempCertData);
    return CryptoX_Error;
  }

  CryptoX_Result result = CryptoX_Error;
  OSStatus status = SecCertificateCopyPublicKeyPtr(cert,
                                                   (SecKeyRef*)aPublicKey);
  if (status == 0) {
    result = CryptoX_Success;
  }

  CFRelease(url);
  CFRelease(stream);
  CFRelease(readTransform);
  CFRelease(tempCertData);
  CFRelease(cert);

  return result;
}

CryptoX_Result
CryptoMac_VerifySignature(CryptoX_SignatureHandle* aInputData,
                          CryptoX_PublicKey* aPublicKey,
                          const unsigned char* aSignature,
                          unsigned int aSignatureLen)
{
  if (!OnLionOrLater()) {
    return NSS_VerifySignature((VFYContext* const*)aInputData, aSignature,
                               aSignatureLen);
  }

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
    CFRelease(signatureData);
    return CryptoX_Error;
  }

  SecTransformSetAttributePtr(verifier,
                              kSecTransformInputAttributeName,
                              (CFDataRef)*aInputData,
                              &error);
  if (error) {
    CFRelease(signatureData);
    CFRelease(verifier);
    return CryptoX_Error;
  }

  CryptoX_Result result = CryptoX_Error;
  CFTypeRef rv = SecTransformExecutePtr(verifier, &error);
  if (error) {
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
  if (!OnLionOrLater()) {
    return VFY_DestroyContext((VFYContext*)aInputData, PR_TRUE);
  }

  if (!aInputData || !*aInputData) {
    return;
  }
  CFRelease((CFMutableDataRef)*aInputData);
}

void
CryptoMac_FreePublicKey(CryptoX_PublicKey* aPublicKey)
{
  if (!OnLionOrLater()) {
    return SECKEY_DestroyPublicKey((SECKEYPublicKey*)*aPublicKey);
  }

  if (!aPublicKey || !*aPublicKey) {
    return;
  }
  CFRelease((SecKeyRef)*aPublicKey);
}

void
CryptoMac_FreeCertificate(CryptoX_Certificate* aCertificate)
{
  if (!OnLionOrLater()) {
    return CERT_DestroyCertificate((CERTCertificate*)*aCertificate);
  }

  if (!aCertificate || !*aCertificate) {
    return;
  }
  CFRelease((SecKeyRef)*aCertificate);
}
