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

static bool sCssmInitialized = false;
static CSSM_VERSION sCssmVersion = {2, 0};
static const CSSM_GUID sMozCssmGuid =
  { 0x9243121f, 0x5820, 0x4b41,
    { 0xa6, 0x52, 0xba, 0xb6, 0x3f, 0x9d, 0x3d, 0x7f }};
static CSSM_CSP_HANDLE sCspHandle = CSSM_INVALID_HANDLE;

void* cssmMalloc (CSSM_SIZE aSize, void* aAllocRef) {
  (void)aAllocRef;
  return malloc(aSize);
}

void cssmFree (void* aPtr, void* aAllocRef) {
  (void)aAllocRef;
  free(aPtr);
  return;
}

void* cssmRealloc (void* aPtr, CSSM_SIZE aSize, void* aAllocRef) {
  (void)aAllocRef;
  return realloc(aPtr, aSize);
}

void* cssmCalloc (uint32 aNum, CSSM_SIZE aSize, void* aAllocRef) {
  (void)aAllocRef;
  return calloc(aNum, aSize);
}

static CSSM_API_MEMORY_FUNCS cssmMemFuncs = {
    &cssmMalloc,
    &cssmFree,
    &cssmRealloc,
    &cssmCalloc,
    NULL
 };

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

  if (!OnLionOrLater()) {
    CSSM_DATA_PTR cssmData = (CSSM_DATA_PTR)malloc(sizeof(CSSM_DATA));
    if (!cssmData) {
      CFRelease(inputData);
      return CryptoX_Error;
    }
    cssmData->Data = (uint8*)inputData;
    cssmData->Length = 0;
    *aInputData = cssmData;
    return CryptoX_Success;
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

  CFMutableDataRef inputData;
  if (!OnLionOrLater()) {
    inputData = (CFMutableDataRef)((CSSM_DATA_PTR)*aInputData)->Data;
    ((CSSM_DATA_PTR)*aInputData)->Length += aLen;
  } else {
    inputData = (CFMutableDataRef)*aInputData;
  }

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

  if (!OnLionOrLater()) {
    if (!sCspHandle) {
      CSSM_RETURN rv;
      if (!sCssmInitialized) {
        CSSM_PVC_MODE pvcPolicy = CSSM_PVC_NONE;
        rv = CSSM_Init(&sCssmVersion,
                       CSSM_PRIVILEGE_SCOPE_PROCESS,
                       &sMozCssmGuid,
                       CSSM_KEY_HIERARCHY_NONE,
                       &pvcPolicy,
                       NULL);
        if (rv != CSSM_OK) {
          return CryptoX_Error;
        }
        sCssmInitialized = true;
      }

      rv = CSSM_ModuleLoad(&gGuidAppleCSP,
                           CSSM_KEY_HIERARCHY_NONE,
                           NULL,
                           NULL);
      if (rv != CSSM_OK) {
        return CryptoX_Error;
      }

      CSSM_CSP_HANDLE cspHandle;
      rv = CSSM_ModuleAttach(&gGuidAppleCSP,
                             &sCssmVersion,
                             &cssmMemFuncs,
                             0,
                             CSSM_SERVICE_CSP,
                             0,
                             CSSM_KEY_HIERARCHY_NONE,
                             NULL,
                             0,
                             NULL,
                             &cspHandle);
      if (rv != CSSM_OK) {
        return CryptoX_Error;
      }
      sCspHandle = cspHandle;
    }
  }

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

  if (!OnLionOrLater()) {
    if (!sCspHandle) {
      return CryptoX_Error;
    }

    CSSM_KEY* publicKey;
    OSStatus status = SecKeyGetCSSMKey((SecKeyRef)*aPublicKey,
                                       (const CSSM_KEY**)&publicKey);
    if (status) {
      return CryptoX_Error;
    }

    CSSM_CC_HANDLE ccHandle;
    if (CSSM_CSP_CreateSignatureContext(sCspHandle,
                                        CSSM_ALGID_SHA1WithRSA,
                                        NULL,
                                        publicKey,
                                        &ccHandle) != CSSM_OK) {
      return CryptoX_Error;
    }

    CryptoX_Result result = CryptoX_Error;
    CSSM_DATA signatureData;
    signatureData.Data = (uint8*)aSignature;
    signatureData.Length = aSignatureLen;
    CSSM_DATA inputData;
    inputData.Data =
      CFDataGetMutableBytePtr((CFMutableDataRef)
                                (((CSSM_DATA_PTR)*aInputData)->Data));
    inputData.Length = ((CSSM_DATA_PTR)*aInputData)->Length;
    if (CSSM_VerifyData(ccHandle,
                        &inputData,
                        1,
                        CSSM_ALGID_NONE,
                        &signatureData) == CSSM_OK) {
      result = CryptoX_Success;
    }
    return result;
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
  if (!aInputData || !*aInputData) {
    return;
  }

  CFMutableDataRef inputData = NULL;
  if (OnLionOrLater()) {
    inputData = (CFMutableDataRef)*aInputData;
  } else {
    inputData = (CFMutableDataRef)((CSSM_DATA_PTR)*aInputData)->Data;
  }

  CFRelease(inputData);
  if (!OnLionOrLater()) {
    free((CSSM_DATA_PTR)*aInputData);
  }
}

void
CryptoMac_FreePublicKey(CryptoX_PublicKey* aPublicKey)
{
  if (!aPublicKey || !*aPublicKey) {
    return;
  }
  if (!OnLionOrLater() && sCspHandle != CSSM_INVALID_HANDLE) {
    CSSM_ModuleDetach(sCspHandle);
    sCspHandle = CSSM_INVALID_HANDLE;
  }
  CFRelease((SecKeyRef)*aPublicKey);
}
