/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   L. David Baron <dbaron@dbaron.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsTraceRefcntImpl_h___
#define nsTraceRefcntImpl_h___

#include <stdio.h> // for FILE
#include "nsITraceRefcnt.h"

class nsTraceRefcntImpl : public nsITraceRefcnt
{
public:
  nsTraceRefcntImpl() {}
  NS_DECL_ISUPPORTS
  NS_DECL_NSITRACEREFCNT

  static void Startup();
  static void Shutdown();

  enum StatisticsType {
    ALL_STATS,
    NEW_STATS
  };

  static nsresult DumpStatistics(StatisticsType type = ALL_STATS,
                                        FILE* out = 0);

  static void ResetStatistics(void);

  static void DemangleSymbol(const char * aSymbol,
                                    char * aBuffer,
                                    int aBufLen);

  static void WalkTheStack(FILE* aStream);
  /**
   * Tell nsTraceRefcnt whether refcounting, allocation, and destruction
   * activity is legal.  This is used to trigger assertions for any such
   * activity that occurs because of static constructors or destructors.
   */
  static void SetActivityIsLegal(PRBool aLegal);

  static NS_METHOD Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);
};

#define NS_TRACE_REFCNT_CONTRACTID "@mozilla.org/xpcom/trace-refcnt;1"
#define NS_TRACE_REFCNT_CLASSNAME  "nsTraceRefcnt Interface"
#define NS_TRACE_REFCNT_CID                          \
{ /* e3e7511e-a395-4924-94b1-d527861cded4 */         \
    0xe3e7511e,                                      \
    0xa395,                                          \
    0x4924,                                          \
    {0x94, 0xb1, 0xd5, 0x27, 0x86, 0x1c, 0xde, 0xd4} \
}                                                    \

////////////////////////////////////////////////////////////////////////////////
// And now for that utility that you've all been asking for...

extern "C" void
NS_MeanAndStdDev(double n, double sumOfValues, double sumOfSquaredValues,
                 double *meanResult, double *stdDevResult);

////////////////////////////////////////////////////////////////////////////////
#endif
