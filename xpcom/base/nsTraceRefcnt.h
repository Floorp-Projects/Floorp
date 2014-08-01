/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsTraceRefcnt_h___
#define nsTraceRefcnt_h___

#include <stdio.h> // for FILE
#include "nscore.h"

class nsTraceRefcnt
{
public:
  static void Startup();
  static void Shutdown();

  enum StatisticsType {
    ALL_STATS,
    NEW_STATS
  };

  static nsresult DumpStatistics(StatisticsType aType = ALL_STATS,
                                 FILE* aOut = 0);

  static void ResetStatistics();

  static void DemangleSymbol(const char* aSymbol, char* aBuffer, int aBufLen);

  static void WalkTheStack(FILE* aStream);
  /**
   * Tell nsTraceRefcnt whether refcounting, allocation, and destruction
   * activity is legal.  This is used to trigger assertions for any such
   * activity that occurs because of static constructors or destructors.
   */
  static void SetActivityIsLegal(bool aLegal);
};

#define NS_TRACE_REFCNT_CONTRACTID "@mozilla.org/xpcom/trace-refcnt;1"
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
NS_MeanAndStdDev(double aNumberOfValues,
                 double aSumOfValues, double aSumOfSquaredValues,
                 double* aMeanResult, double* aStdDevResult);

////////////////////////////////////////////////////////////////////////////////
#endif
