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
};

#endif /* nsTraceRefcnt_h___ */
