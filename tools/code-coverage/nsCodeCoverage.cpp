/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCodeCoverage.h"
#include "mozilla/CodeCoverageHandler.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ContentParent.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsCodeCoverage,
                  nsICodeCoverage)

nsCodeCoverage::nsCodeCoverage()
{
}

nsCodeCoverage::~nsCodeCoverage()
{
}

NS_IMETHODIMP nsCodeCoverage::DumpCounters()
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  CodeCoverageHandler::DumpCounters(0);
  for (auto* cp : ContentParent::AllProcesses(ContentParent::eLive)) {
    Unused << cp->SendDumpCodeCoverageCounters();
  }

  return NS_OK;
}

NS_IMETHODIMP nsCodeCoverage::ResetCounters()
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  CodeCoverageHandler::ResetCounters(0);
  for (auto* cp : ContentParent::AllProcesses(ContentParent::eLive)) {
    Unused << cp->SendResetCodeCoverageCounters();
  }

  return NS_OK;
}
