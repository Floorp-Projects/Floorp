/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIMemoryReporter.h"
#include "mozilla/Mutex.h"
#include "mozilla/Attributes.h"
#include "nsString.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"

using mozilla::Mutex;

class nsMemoryReporterManager : public nsIMemoryReporterManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTERMANAGER

  nsMemoryReporterManager();
  virtual ~nsMemoryReporterManager();

private:
  nsresult RegisterReporterHelper(nsIMemoryReporter *reporter, bool aForce);
  nsresult RegisterMultiReporterHelper(nsIMemoryMultiReporter *reporter,
                                       bool aForce);

  nsTHashtable<nsISupportsHashKey> mReporters;
  nsTHashtable<nsISupportsHashKey> mMultiReporters;
  Mutex mMutex;
  bool mIsRegistrationBlocked;
};

#define NS_MEMORY_REPORTER_MANAGER_CID \
{ 0xfb97e4f5, 0x32dd, 0x497a, \
{ 0xba, 0xa2, 0x7d, 0x1e, 0x55, 0x7, 0x99, 0x10 } }
