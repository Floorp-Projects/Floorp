/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsTraceRefcnt_h___
#define nsTraceRefcnt_h___

#include "nsXPCOM.h"

#ifdef NS_BUILD_REFCNT_LOGGING

#define NS_LOG_ADDREF(_p, _rc, _type, _size) \
  NS_LogAddRef((_p), (_rc), (_type), (PRUint32) (_size))

#define NS_LOG_RELEASE(_p, _rc, _type) \
  NS_LogRelease((_p), (_rc), (_type))

#define MOZ_COUNT_CTOR(_type)                                 \
PR_BEGIN_MACRO                                                \
  NS_LogCtor((void*)this, #_type, sizeof(*this));             \
PR_END_MACRO

#define MOZ_COUNT_CTOR_INHERITED(_type, _base)                    \
PR_BEGIN_MACRO                                                    \
  NS_LogCtor((void*)this, #_type, sizeof(*this) - sizeof(_base)); \
PR_END_MACRO

#define MOZ_COUNT_DTOR(_type)                                 \
PR_BEGIN_MACRO                                                \
  NS_LogDtor((void*)this, #_type, sizeof(*this));             \
PR_END_MACRO

#define MOZ_COUNT_DTOR_INHERITED(_type, _base)                    \
PR_BEGIN_MACRO                                                    \
  NS_LogDtor((void*)this, #_type, sizeof(*this) - sizeof(_base)); \
PR_END_MACRO

/* nsCOMPtr.h allows these macros to be defined by clients
 * These logging functions require dynamic_cast<void*>, so they don't
 * do anything useful if we don't have dynamic_cast<void*>. */
#define NSCAP_LOG_ASSIGNMENT(_c, _p)                                \
  if (_p)                                                           \
    NS_LogCOMPtrAddRef((_c),static_cast<nsISupports*>(_p))

#define NSCAP_LOG_RELEASE(_c, _p)                                   \
  if (_p)                                                           \
    NS_LogCOMPtrRelease((_c), static_cast<nsISupports*>(_p))

#else /* !NS_BUILD_REFCNT_LOGGING */

#define NS_LOG_ADDREF(_p, _rc, _type, _size)
#define NS_LOG_RELEASE(_p, _rc, _type)
#define MOZ_COUNT_CTOR(_type)
#define MOZ_COUNT_CTOR_INHERITED(_type, _base)
#define MOZ_COUNT_DTOR(_type)
#define MOZ_COUNT_DTOR_INHERITED(_type, _base)

#endif /* NS_BUILD_REFCNT_LOGGING */

#ifdef __cplusplus

class nsTraceRefcnt {
public:
  inline static void LogAddRef(void* aPtr, nsrefcnt aNewRefCnt,
                               const char* aTypeName, PRUint32 aInstanceSize) {
    NS_LogAddRef(aPtr, aNewRefCnt, aTypeName, aInstanceSize);
  }

  inline static void LogRelease(void* aPtr, nsrefcnt aNewRefCnt,
                                const char* aTypeName) {
    NS_LogRelease(aPtr, aNewRefCnt, aTypeName);
  }

  inline static void LogCtor(void* aPtr, const char* aTypeName,
                             PRUint32 aInstanceSize) {
    NS_LogCtor(aPtr, aTypeName, aInstanceSize);
  }

  inline static void LogDtor(void* aPtr, const char* aTypeName,
                             PRUint32 aInstanceSize) {
    NS_LogDtor(aPtr, aTypeName, aInstanceSize);
  }

  inline static void LogAddCOMPtr(void *aCOMPtr, nsISupports *aObject) {
    NS_LogCOMPtrAddRef(aCOMPtr, aObject);
  }

  inline static void LogReleaseCOMPtr(void *aCOMPtr, nsISupports *aObject) {
    NS_LogCOMPtrRelease(aCOMPtr, aObject);
  }
};

#endif /* defined(__cplusplus) */

#endif /* nsTraceRefcnt_h___ */
