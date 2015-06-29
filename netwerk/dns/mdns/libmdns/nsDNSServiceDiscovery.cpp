/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDNSServiceDiscovery.h"
#include <cutils/properties.h>
#include "MDNSResponderOperator.h"
#include "nsICancelable.h"
#include "private/pprio.h"

namespace mozilla {
namespace net {

namespace {

void
StartService()
{
  char value[PROPERTY_VALUE_MAX] = { '\0' };
  property_get("init.svc.mdnsd", value, "");

  if (strcmp(value, "running") == 0) {
    return;
  }
  property_set("ctl.start", "mdnsd");
}

inline void
StopService()
{
  char value[PROPERTY_VALUE_MAX] = { '\0' };
  property_get("init.svc.mdnsd", value, "");

  if (strcmp(value, "stopped") == 0) {
    return;
  }
  property_set("ctl.stop", "mdnsd");
}

class DiscoveryRequest final : public nsICancelable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICANCELABLE

  explicit DiscoveryRequest(nsDNSServiceDiscovery* aService,
                            nsIDNSServiceDiscoveryListener* aListener);

private:
  virtual ~DiscoveryRequest() { Cancel(NS_OK); }

  nsRefPtr<nsDNSServiceDiscovery> mService;
  nsIDNSServiceDiscoveryListener* mListener;
};

NS_IMPL_ISUPPORTS(DiscoveryRequest, nsICancelable)

DiscoveryRequest::DiscoveryRequest(nsDNSServiceDiscovery* aService,
                                   nsIDNSServiceDiscoveryListener* aListener)
  : mService(aService)
  , mListener(aListener)
{
}

NS_IMETHODIMP
DiscoveryRequest::Cancel(nsresult aReason)
{
  if (mService) {
    mService->StopDiscovery(mListener);
  }

  mService = nullptr;
  return NS_OK;
}

class RegisterRequest final : public nsICancelable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICANCELABLE

  explicit RegisterRequest(nsDNSServiceDiscovery* aService,
                           nsIDNSRegistrationListener* aListener);

private:
  virtual ~RegisterRequest() { Cancel(NS_OK); }

  nsRefPtr<nsDNSServiceDiscovery> mService;
  nsIDNSRegistrationListener* mListener;
};

NS_IMPL_ISUPPORTS(RegisterRequest, nsICancelable)

RegisterRequest::RegisterRequest(nsDNSServiceDiscovery* aService,
                                 nsIDNSRegistrationListener* aListener)
  : mService(aService)
  , mListener(aListener)
{
}

NS_IMETHODIMP
RegisterRequest::Cancel(nsresult aReason)
{
  if (mService) {
    mService->UnregisterService(mListener);
  }

  mService = nullptr;
  return NS_OK;
}

} // namespace anonymous

NS_IMPL_ISUPPORTS(nsDNSServiceDiscovery, nsIDNSServiceDiscovery)

nsresult
nsDNSServiceDiscovery::Init()
{
  StartService();
  return NS_OK;
}

NS_IMETHODIMP
nsDNSServiceDiscovery::StartDiscovery(const nsACString& aServiceType,
                                      nsIDNSServiceDiscoveryListener* aListener,
                                      nsICancelable** aRetVal)
{
  MOZ_ASSERT(aRetVal);

  nsresult rv;
  if (NS_WARN_IF(NS_FAILED(rv = StopDiscovery(aListener)))) {
    return rv;
  }

  nsRefPtr<BrowseOperator> browserOp = new BrowseOperator(aServiceType,
                                                          aListener);
  if (NS_WARN_IF(NS_FAILED(rv = browserOp->Start()))) {
    return rv;
  }

  mDiscoveryMap.Put(aListener, browserOp);

  nsCOMPtr<nsICancelable> req = new DiscoveryRequest(this, aListener);
  req.forget(aRetVal);

  return NS_OK;
}

NS_IMETHODIMP
nsDNSServiceDiscovery::StopDiscovery(nsIDNSServiceDiscoveryListener* aListener)
{
  nsresult rv;

  nsRefPtr<BrowseOperator> browserOp;
  if (!mDiscoveryMap.Get(aListener, getter_AddRefs(browserOp))) {
    return NS_OK;
  }

  browserOp->Cancel(); // cancel non-started operation
  if (NS_WARN_IF(NS_FAILED(rv = browserOp->Stop()))) {
    return rv;
  }

  mDiscoveryMap.Remove(aListener);
  return NS_OK;
}

NS_IMETHODIMP
nsDNSServiceDiscovery::RegisterService(nsIDNSServiceInfo* aServiceInfo,
                                       nsIDNSRegistrationListener* aListener,
                                       nsICancelable** aRetVal)
{
  MOZ_ASSERT(aRetVal);

  nsresult rv;
  if (NS_WARN_IF(NS_FAILED(rv = UnregisterService(aListener)))) {
    return rv;
  }

  nsRefPtr<RegisterOperator> registerOp = new RegisterOperator(aServiceInfo,
                                                               aListener);
  if (NS_WARN_IF(NS_FAILED(rv = registerOp->Start()))) {
    return rv;
  }

  mRegisterMap.Put(aListener, registerOp);

  nsCOMPtr<nsICancelable> req = new RegisterRequest(this, aListener);
  req.forget(aRetVal);

  return NS_OK;
}

NS_IMETHODIMP
nsDNSServiceDiscovery::UnregisterService(nsIDNSRegistrationListener* aListener)
{
  nsresult rv;

  nsRefPtr<RegisterOperator> registerOp;
  if (!mRegisterMap.Get(aListener, getter_AddRefs(registerOp))) {
    return NS_OK;
  }

  registerOp->Cancel(); // cancel non-started operation
  if (NS_WARN_IF(NS_FAILED(rv = registerOp->Stop()))) {
    return rv;
  }

  mRegisterMap.Remove(aListener);
  return NS_OK;
}

NS_IMETHODIMP
nsDNSServiceDiscovery::ResolveService(nsIDNSServiceInfo* aServiceInfo,
                                      nsIDNSServiceResolveListener* aListener)
{
  nsresult rv;

  nsRefPtr<ResolveOperator> resolveOp = new ResolveOperator(aServiceInfo,
                                                            aListener);
  if (NS_WARN_IF(NS_FAILED(rv = resolveOp->Start()))) {
    return rv;
  }

  return NS_OK;
}

} // namespace net
} // namespace mozilla
