/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@fas.harvard.edu>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsTraceRefcnt_h___
#define nsTraceRefcnt_h___

#include "nsCom.h"

#include <stdio.h>

class nsISupports;

// Normaly, the implementation of NS_LOG_ADDREF and NS_LOG_RELEASE
// will use a stack crawl to determine who called Addref/Release on an
// xpcom object.  If your platform can't implement a stack crawling
// function, then define this to symbol. When the symbol is defined
// then the NS_LOG_ADDREF_CALL and NS_LOG_RELEASE_CALL will expand
// differently.
#undef NS_LOSING_ARCHITECTURE

// By default refcnt logging is not part of the build.
#undef NS_BUILD_REFCNT_LOGGING

#if (defined(DEBUG) || defined(FORCE_BUILD_REFCNT_LOGGING))
// Make refcnt logging part of the build. This doesn't mean that
// actual logging will occur (that requires a seperate enable; see
// nsTraceRefcnt.h for more information).
#define NS_BUILD_REFCNT_LOGGING 1
#endif

// If NO_BUILD_REFCNT_LOGGING is defined then disable refcnt logging
// in the build. This overrides FORCE_BUILD_REFCNT_LOGGING.
#if defined(NO_BUILD_REFCNT_LOGGING)
#undef NS_BUILD_REFCNT_LOGGING
#endif

#ifdef NS_BUILD_REFCNT_LOGGING

#define NS_LOG_ADDREF(_p, _rc, _type, _size) \
  nsTraceRefcnt::LogAddRef((_p), (_rc), (_type), (PRUint32) (_size))

#define NS_LOG_RELEASE(_p, _rc, _type) \
  nsTraceRefcnt::LogRelease((_p), (_rc), (_type))

#ifdef NS_LOSING_ARCHITECTURE

#define NS_LOG_ADDREF_CALL(_p,_rc,_file,_line) \
  nsTraceRefcnt::LogAddRefCall((_p), (_rc), (_file), (_line))

#define NS_LOG_RELEASE_CALL(_p,_rc,_file,_line) \
  nsTraceRefcnt::LogReleaseCall((_p), (_rc), (_file), (_line))

#define NS_LOG_NEW_XPCOM(_p,_type,_size,_file,_line) \
  nsTraceRefcnt::LogNewXPCOM((_p),(_type),(PRUint32)(_size),(_file),(_line))

#define NS_LOG_DELETE_XPCOM(_p,_file,_line) \
  nsTraceRefcnt::LogDeleteXPCOM((_p),(_file),(_line))

#else

#define NS_LOG_ADDREF_CALL(_p,_rc,_file,_line) _rc
#define NS_LOG_RELEASE_CALL(_p,_rc,_file,_line) _rc

#define NS_LOG_NEW_XPCOM(_p,_type,_size,_file,_line)
#define NS_LOG_DELETE_XPCOM(_p,_file,_line)

#endif

#define MOZ_DECL_CTOR_COUNTER(_type)

#define MOZ_COUNT_CTOR(_type)                                 \
PR_BEGIN_MACRO                                                \
  nsTraceRefcnt::LogCtor((void*)this, #_type, sizeof(*this)); \
PR_END_MACRO

#define MOZ_COUNT_DTOR(_type)                                 \
PR_BEGIN_MACRO                                                \
  nsTraceRefcnt::LogDtor((void*)this, #_type, sizeof(*this)); \
PR_END_MACRO

#ifdef HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR  // from autoconf (XXX needs to be
                                          // set for non-autoconf platforms)

// nsCOMPtr.h allows these macros to be defined by clients
// These logging functions require dynamic_cast<void *>, so we don't
// define these macros if we don't have dynamic_cast.
#define NSCAP_LOG_ASSIGNMENT(_c, _p)               \
  if ((_p)) nsTraceRefcnt::LogAddCOMPtr((_c),NS_STATIC_CAST(nsISupports*,_p))

#define NSCAP_RELEASE(_c, _p);                                      \
  if ((_p)) {                                                       \
    nsTraceRefcnt::LogReleaseCOMPtr((_c),NS_STATIC_CAST(nsISupports*,_p)); \
    NS_RELEASE(_p);                                                 \
  }

#endif /* HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR */

#else /* !NS_BUILD_REFCNT_LOGGING */

#define NS_LOG_ADDREF(_p, _rc, _type, _size)
#define NS_LOG_RELEASE(_p, _rc, _type)
#define NS_LOG_NEW_XPCOM(_p,_type,_size,_file,_line)
#define NS_LOG_DELETE_XPCOM(_p,_file,_line)
#define NS_LOG_ADDREF_CALL(_p,_rc,_file,_line) _rc
#define NS_LOG_RELEASE_CALL(_p,_rc,_file,_line) _rc
#define MOZ_DECL_CTOR_COUNTER(_type)
#define MOZ_COUNT_CTOR(_type)
#define MOZ_COUNT_DTOR(_type)

#endif /* NS_BUILD_REFCNT_LOGGING */

//----------------------------------------------------------------------

struct nsTraceRefcntStats {
  nsrefcnt mAddRefs;
  nsrefcnt mReleases;
  nsrefcnt mCreates;
  nsrefcnt mDestroys;
  double mRefsOutstandingTotal;
  double mRefsOutstandingSquared;
  double mObjsOutstandingTotal;
  double mObjsOutstandingSquared;
};

// Function type used by GatherStatistics. For each type that data has
// been gathered for, this function is called with the counts of the
// various operations that have been logged. The function can return
// PR_FALSE if the gathering should stop.
//
// aCurrentStats is the current value of the counters. aPrevStats is
// the previous value of the counters which is established by the
// nsTraceRefcnt::SnapshotStatistics call.
typedef PRBool (PR_CALLBACK *nsTraceRefcntStatFunc)
  (const char* aTypeName,
   PRUint32 aInstanceSize,
   nsTraceRefcntStats* aCurrentStats,
   nsTraceRefcntStats* aPrevStats,
   void *aClosure);

/**
 * Note: The implementations for these methods are no-ops in a build
 * where NS_BUILD_REFCNT_LOGGING is disabled.
 */
class nsTraceRefcnt {
public:
  static NS_COM void Startup();

  static NS_COM void Shutdown();

  static NS_COM void LogAddRef(void* aPtr,
                               nsrefcnt aNewRefCnt,
                               const char* aTypeName,
                               PRUint32 aInstanceSize);

  static NS_COM void LogRelease(void* aPtr,
                                nsrefcnt aNewRefCnt,
                                const char* aTypeName);

  static NS_COM void LogNewXPCOM(void* aPtr,
                                 const char* aTypeName,
                                 PRUint32 aInstanceSize,
                                 const char* aFile,
                                 int aLine);

  static NS_COM void LogDeleteXPCOM(void* aPtr,
                                    const char* aFile,
                                    int aLine);

  static NS_COM nsrefcnt LogAddRefCall(void* aPtr,
                                       nsrefcnt aNewRefcnt,
                                       const char* aFile,
                                       int aLine);

  static NS_COM nsrefcnt LogReleaseCall(void* aPtr,
                                        nsrefcnt aNewRefcnt,
                                        const char* aFile,
                                        int aLine);

  static NS_COM void LogCtor(void* aPtr, const char* aTypeName,
                             PRUint32 aInstanceSize);

  static NS_COM void LogDtor(void* aPtr, const char* aTypeName,
                             PRUint32 aInstanceSize);

  static NS_COM void LogAddCOMPtr(void *aCOMPtr, nsISupports *aObject);

  static NS_COM void LogReleaseCOMPtr(void *aCOMPtr, nsISupports *aObject);

  enum StatisticsType {
    ALL_STATS,
    NEW_STATS
  };

  static NS_COM nsresult DumpStatistics(StatisticsType type = ALL_STATS,
                                        FILE* out = 0);
  
  static NS_COM void ResetStatistics(void);

  static NS_COM void GatherStatistics(nsTraceRefcntStatFunc aFunc,
                                      void* aClosure);

  static NS_COM void LoadLibrarySymbols(const char* aLibraryName,
                                        void* aLibrayHandle);

  static NS_COM void DemangleSymbol(const char * aSymbol, 
                                    char * aBuffer,
                                    int aBufLen);

  static NS_COM void WalkTheStack(FILE* aStream);
  
  static NS_COM void SetPrefServiceAvailability(PRBool avail);

  /**
   * Tell nsTraceRefcnt whether refcounting, allocation, and destruction
   * activity is legal.  This is used to trigger assertions for any such
   * activity that occurs because of static constructors or destructors.
   */
  static NS_COM void SetActivityIsLegal(PRBool aLegal);
};

////////////////////////////////////////////////////////////////////////////////
// And now for that utility that you've all been asking for...

extern "C" NS_COM void 
NS_MeanAndStdDev(double n, double sumOfValues, double sumOfSquaredValues,
                 double *meanResult, double *stdDevResult);

////////////////////////////////////////////////////////////////////////////////
#endif /* nsTraceRefcnt_h___ */
