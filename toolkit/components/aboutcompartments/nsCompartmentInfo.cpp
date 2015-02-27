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

class nsCompartment : public nsICompartment {
public:
  nsCompartment(nsAString& aCompartmentName, nsAString& aAddonId,
                uint64_t aTime, uint64_t aCPOWTime)
    : mCompartmentName(aCompartmentName), mAddonId(aAddonId), mTime(aTime), mCPOWTime(aCPOWTime) {}

  NS_DECL_ISUPPORTS

  /* readonly attribute wstring compartmentName; */
  NS_IMETHOD GetCompartmentName(nsAString& aCompartmentName) MOZ_OVERRIDE {
    aCompartmentName.Assign(mCompartmentName);
    return NS_OK;
  };

  /* readonly attribute unsigned long time; */
  NS_IMETHOD GetTime(uint64_t* aTime) MOZ_OVERRIDE {
    *aTime = mTime;
    return NS_OK;
  }
  /* readonly attribute wstring addon id; */
  NS_IMETHOD GetAddonId(nsAString& aAddonId) MOZ_OVERRIDE {
    aAddonId.Assign(mAddonId);
    return NS_OK;
  };

  /* readonly attribute unsigned long CPOW time; */
  NS_IMETHOD GetCPOWTime(uint64_t* aCPOWTime) MOZ_OVERRIDE {
    *aCPOWTime = mCPOWTime;
    return NS_OK;
  }

private:
  nsString mCompartmentName;
  nsString mAddonId;
  uint64_t mTime;
  uint64_t mCPOWTime;
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
    nsString addonId;
    if (stats[pos].addonId) {
      AssignJSFlatString(addonId, (JSFlatString*)stats[pos].addonId);
    } else {
      addonId.AssignLiteral("<non-addon>");
    }

    uint32_t cpowTime = xpc::GetCompartmentCPOWMicroseconds(stats[pos].compartment);
    nsCString compartmentName(stats[pos].compartmentName);
    NS_ConvertUTF8toUTF16 name(compartmentName);
    compartments->AppendElement(new nsCompartment(name, addonId, stats[pos].time, cpowTime), false);
  }
  compartments.forget(aCompartments);
  return NS_OK;
}
