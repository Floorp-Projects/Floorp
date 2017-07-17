//* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EffectiveTLDService_h
#define EffectiveTLDService_h

#include "nsIEffectiveTLDService.h"

#include "nsIMemoryReporter.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"
#include "mozilla/Dafsa.h"
#include "mozilla/MemoryReporting.h"

class nsIIDNService;

class nsEffectiveTLDService final
  : public nsIEffectiveTLDService
  , public nsIMemoryReporter
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEFFECTIVETLDSERVICE
  NS_DECL_NSIMEMORYREPORTER

  nsEffectiveTLDService();
  nsresult Init();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

private:
  nsresult GetBaseDomainInternal(nsCString &aHostname, int32_t aAdditionalParts, nsACString &aBaseDomain);
  nsresult NormalizeHostname(nsCString &aHostname);
  ~nsEffectiveTLDService();

  nsCOMPtr<nsIIDNService>     mIDNService;
  mozilla::Dafsa mGraph;
};

#endif // EffectiveTLDService_h
