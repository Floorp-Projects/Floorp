/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsTraceRefcnt_h
#define nsTraceRefcnt_h

#include "nscore.h"

class nsTraceRefcnt {
 public:
  static void Shutdown();

  static nsresult DumpStatistics();

  static void ResetStatistics();

  /**
   * Tell nsTraceRefcnt whether refcounting, allocation, and destruction
   * activity is legal.  This is used to trigger assertions for any such
   * activity that occurs because of static constructors or destructors.
   */
  static void SetActivityIsLegal(bool aLegal);

  /**
   * Start refcount logging aClass. If refcount logging has not already begun,
   * it will use the environment variable XPCOM_MEM_LATE_REFCNT_LOG to decide
   * where to make the log, in a similar way as the other nsTraceRefcnt logs.
   */
  static void StartLoggingClass(const char* aClass);

#ifdef MOZ_ENABLE_FORKSERVER
  static void ResetLogFiles(const char* aProcType = nullptr);
#endif
};

#endif  // nsTraceRefcnt_h
