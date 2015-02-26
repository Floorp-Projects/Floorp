/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCompartmentInfo.h"
#include "nsMemory.h"
#include "nsLiteralString.h"
#include "nsCRTGlue.h"
#include "nsIJSRuntimeService.h"
#include "nsServiceManagerUtils.h"
#include "nsIMutableArray.h"
#include "nsJSUtils.h"
#include "xpcpublic.h"
#include "jspubtd.h"

class nsCompartment : public nsICompartment {
public:
  nsCompartment(nsAString& aCompartmentName, nsAString& aAddonId, bool aIsSystem, js::PerformanceData aPerformanceData)
    : mCompartmentName(aCompartmentName)
    , mAddonId(aAddonId)
    , mIsSystem(aIsSystem)
    , mPerformanceData(aPerformanceData)
  {}

  NS_DECL_ISUPPORTS

  /* readonly attribute wstring compartmentName; */
  NS_IMETHOD GetCompartmentName(nsAString& aCompartmentName) MOZ_OVERRIDE {
    aCompartmentName.Assign(mCompartmentName);
    return NS_OK;
  };

  /* readonly attribute wstring addon id; */
  NS_IMETHOD GetAddonId(nsAString& aAddonId) MOZ_OVERRIDE {
    aAddonId.Assign(mAddonId);
    return NS_OK;
  };

  /* readonly attribute unsigned long long totalUserTime; */
  NS_IMETHOD GetTotalUserTime(uint64_t *aTotalUserTime) {
    *aTotalUserTime = mPerformanceData.totalUserTime;
    return NS_OK;
  };

  /* readonly attribute unsigned long long totalSystemTime; */
  NS_IMETHOD GetTotalSystemTime(uint64_t *aTotalSystemTime) {
    *aTotalSystemTime = mPerformanceData.totalSystemTime;
    return NS_OK;
  };

  /* readonly attribute unsigned long long ownUserTime; */
  NS_IMETHOD GetOwnUserTime(uint64_t *aOwnUserTime) {
    *aOwnUserTime = mPerformanceData.ownUserTime;
    return NS_OK;
  };

  /* readonly attribute unsigned long long ownSystemTime; */
  NS_IMETHOD GetOwnSystemTime(uint64_t *aOwnSystemTime) {
    *aOwnSystemTime = mPerformanceData.ownSystemTime;
    return NS_OK;
  };

  /* readonly attribute unsigned long long CPOWTime; */
  NS_IMETHOD GetCPOWTime(uint64_t *aCpowTime) {
    *aCpowTime = mPerformanceData.cpowTime;
    return NS_OK;
  };

  /* readonly attribute unsigned long long visits; */
  NS_IMETHOD GetVisits(uint64_t *aVisits) {
    *aVisits = mPerformanceData.visits;
    return NS_OK;
  };

  /* unsigned long long getMissedFrames (in unsigned long i); */
  NS_IMETHOD GetMissedFrames(uint32_t i, uint64_t *_retval) {
    if (i >= mozilla::ArrayLength(mPerformanceData.missedFrames)) {
      return NS_ERROR_INVALID_ARG;
    }
    *_retval = mPerformanceData.missedFrames[i];
    return NS_OK;
  };

  NS_IMETHOD GetIsSystem(bool *_retval) {
    *_retval = mIsSystem;
    return NS_OK;
  }

private:
  nsString mCompartmentName;
  nsString mAddonId;
  bool mIsSystem;
  js::PerformanceData mPerformanceData;

  virtual ~nsCompartment() {}
};

NS_IMPL_ISUPPORTS(nsCompartment, nsICompartment)
NS_IMPL_ISUPPORTS(nsCompartmentInfo, nsICompartmentInfo)

nsCompartmentInfo::nsCompartmentInfo()
{
}

nsCompartmentInfo::~nsCompartmentInfo()
{
}

NS_IMETHODIMP
nsCompartmentInfo::GetCompartments(nsIArray** aCompartments)
{
  JSRuntime* rt;
  nsCOMPtr<nsIJSRuntimeService> svc(do_GetService("@mozilla.org/js/xpc/RuntimeService;1"));
  NS_ENSURE_TRUE(svc, NS_ERROR_FAILURE);
  svc->GetRuntime(&rt);
  nsCOMPtr<nsIMutableArray> compartments = do_CreateInstance(NS_ARRAY_CONTRACTID);
  CompartmentStatsVector stats;
  if (!JS_GetCompartmentStats(rt, stats))
    return NS_ERROR_OUT_OF_MEMORY;

  size_t num = stats.length();
  for (size_t pos = 0; pos < num; pos++) {
    CompartmentTimeStats *c = &stats[pos];
    nsString addonId;
    if (c->addonId) {
      AssignJSFlatString(addonId, (JSFlatString*)c->addonId);
    } else {
      addonId.AssignLiteral("<non-addon>");
    }

    nsCString compartmentName(c->compartmentName);
    NS_ConvertUTF8toUTF16 name(compartmentName);

    compartments->AppendElement(new nsCompartment(name, addonId, c->isSystem, c->performance), false);
  }
  compartments.forget(aCompartments);
  return NS_OK;
}
