/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsTraceRefcnt_h___
#define nsTraceRefcnt_h___

#include "nsCom.h"
#include <stdio.h>

class nsVoidArray;

#ifdef DEBUG
struct mozCtorDtorCounter {
  PRInt32 ctors;
  PRInt32 dtors;
};
#endif

/**
 * This class is used to support tracing (and logging using nspr) of
 * addref and release calls. Note that only calls that use the
 * NS_ADDREF and related macros in nsISupports can be traced.
 *
 * The name of the nspr log module is "xpcomrefcnt" (case matters).
 *
 * This code only performs tracing built with debugging AND when
 * built with -DMOZ_TRACE_XPCOM_REFCNT (because it's expensive!).
 */
class nsTraceRefcnt {
public:
  static NS_COM unsigned long AddRef(void* aPtr,
                                     unsigned long aNewRefcnt,
                                     const char* aFile,
                                     int aLine);

  static NS_COM unsigned long Release(void* aPtr,
                                      unsigned long aNewRefcnt,
                                      const char* aFile,
                                      int aLine);

  static NS_COM void Create(void* aPtr,
                            const char* aType,
                            const char* aFile,
                            int aLine);

  static NS_COM void Destroy(void* aPtr,
                             const char* aFile,
                             int aLine);

  static NS_COM void LoadLibrarySymbols(const char* aLibraryName,
                                        void* aLibrayHandle);

  static NS_COM void WalkTheStack(char* aBuffer, int aBufLen);

  static NS_COM void DemangleSymbol(const char * aSymbol, 
                                    char * aBuffer,
                                    int aBufLen);

  static NS_COM void LogAddRef(void* aPtr,
                               nsrefcnt aRefCnt,
                               const char* aFile,
                               int aLine);

  static NS_COM void LogRelease(void* aPtr,
                                nsrefcnt aRefCnt,
                                const char* aFile,
                                int aLine);

#ifdef DEBUG
  /**
   * Register a constructor with the xpcom library. This records the
   * type name and the address of the counter so that later on when
   * DumpLeaks is called, we can print out those objects ctors whose
   * counter is not zero (the ones that have live object references
   * still out there)
   */
  static NS_COM void RegisterCtor(const char* aType,
                                  mozCtorDtorCounter* aCounterAddr);

  static NS_COM void UnregisterCtor(const char* aType);

  /**
   * Dump the leaking constructors out.
   */
  static NS_COM void DumpLeaks(FILE* out);

  /**
   * Erase the ctor registration data.
   */
  static NS_COM void FlushCtorRegistry(void);
#endif

protected:
#ifdef DEBUG
  static nsVoidArray* mCtors;
#endif
};

//----------------------------------------------------------------------

#ifdef DEBUG

#define MOZ_DECL_CTOR(_type)                  \
  static mozCtorDtorCounter gCounter_##_type

#define MOZ_CTOR(_type)                                     \
PR_BEGIN_MACRO                                              \
  if (0 == gCounter_##_type . ctors) {                      \
    nsTraceRefcnt::RegisterCtor(#_type, &gCounter_##_type); \
  }                                                         \
  gCounter_##_type . ctors++;                               \
PR_END_MACRO

#define MOZ_DTOR(_type)        \
PR_BEGIN_MACRO                 \
  gCounter_##_type . dtors ++; \
PR_END_MACRO

#else

#define MOZ_REG_CTOR(_type,_counter)
#define MOZ_DECL_CTOR_(type)
#define MOZ_CTOR(_type)
#define MOZ_DTOR(_type)

#endif /* DEBUG */

#endif /* nsTraceRefcnt_h___ */
