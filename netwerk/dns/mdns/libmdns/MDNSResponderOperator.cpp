/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MDNSResponderOperator.h"
#include "MDNSResponderReply.h"
#include "mozilla/Endian.h"
#include "mozilla/Logging.h"
#include "mozilla/ScopeExit.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsDNSServiceInfo.h"
#include "nsHashPropertyBag.h"
#include "nsIProperty.h"
#include "nsISimpleEnumerator.h"
#include "nsIVariant.h"
#include "nsServiceManagerUtils.h"
#include "nsNetAddr.h"
#include "nsNetCID.h"
#include "nsSocketTransportService2.h"
#include "nsThreadUtils.h"
#include "nsXPCOMCID.h"
#include "private/pprio.h"

#include "nsASocketHandler.h"

namespace mozilla {
namespace net {

static LazyLogModule gMDNSLog("MDNSResponderOperator");
#undef LOG_I
#define LOG_I(...) MOZ_LOG(mozilla::net::gMDNSLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#undef LOG_E
#define LOG_E(...) MOZ_LOG(mozilla::net::gMDNSLog, mozilla::LogLevel::Error, (__VA_ARGS__))

class MDNSResponderOperator::ServiceWatcher final
  : public nsASocketHandler
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  // nsASocketHandler methods
  virtual void OnSocketReady(PRFileDesc* fd, int16_t outFlags) override
  {
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    MOZ_ASSERT(fd == mFD);

    if (outFlags & (PR_POLL_ERR | PR_POLL_HUP | PR_POLL_NVAL)) {
      LOG_E("error polling on listening socket (%p)", fd);
      mCondition = NS_ERROR_UNEXPECTED;
    }

    if (!(outFlags & PR_POLL_READ)) {
      return;
    }

    DNSServiceProcessResult(mService);
  }

  virtual void OnSocketDetached(PRFileDesc *fd) override
  {
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    MOZ_ASSERT(mThread);
    MOZ_ASSERT(fd == mFD);

    if (!mFD) {
      return;
    }

    // Bug 1175387: do not double close the handle here.
    PR_ChangeFileDescNativeHandle(mFD, -1);
    PR_Close(mFD);
    mFD = nullptr;

    mThread->Dispatch(NewRunnableMethod(this, &ServiceWatcher::Deallocate),
                      NS_DISPATCH_NORMAL);
  }

  virtual void IsLocal(bool *aIsLocal) override { *aIsLocal = true; }

  virtual void KeepWhenOffline(bool *aKeepWhenOffline) override
  {
    *aKeepWhenOffline = true;
  }

  virtual uint64_t ByteCountSent() override { return 0; }
  virtual uint64_t ByteCountReceived() override { return 0; }

  explicit ServiceWatcher(DNSServiceRef aService,
                          MDNSResponderOperator* aOperator)
    : mThread(nullptr)
    , mSts(nullptr)
    , mOperatorHolder(aOperator)
    , mService(aService)
    , mFD(nullptr)
    , mAttached(false)
  {
    if (!gSocketTransportService)
    {
      nsCOMPtr<nsISocketTransportService> sts =
        do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID);
    }
  }

  nsresult Init()
  {
    MOZ_ASSERT(PR_GetCurrentThread() != gSocketThread);
    mThread = NS_GetCurrentThread();

    if (!mService) {
      return NS_OK;
    }

    if (!gSocketTransportService) {
      return NS_ERROR_FAILURE;
    }
    mSts = gSocketTransportService;

    int osfd = DNSServiceRefSockFD(mService);
    if (osfd == -1) {
      return NS_ERROR_FAILURE;
    }

    mFD = PR_ImportFile(osfd);
    return PostEvent(&ServiceWatcher::OnMsgAttach);
  }

  void Close()
  {
    MOZ_ASSERT(PR_GetCurrentThread() != gSocketThread);

    if (!gSocketTransportService) {
      Deallocate();
      return;
    }

    PostEvent(&ServiceWatcher::OnMsgClose);
  }

private:
  ~ServiceWatcher() = default;

  void Deallocate()
  {
    if (mService) {
      DNSServiceRefDeallocate(mService);
      mService = nullptr;
    }
    mOperatorHolder = nullptr;
  }

  nsresult PostEvent(void(ServiceWatcher::*func)(void))
  {
    return gSocketTransportService->Dispatch(NewRunnableMethod(this, func),
                                             NS_DISPATCH_NORMAL);
  }

  void OnMsgClose()
  {
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    if (NS_FAILED(mCondition)) {
      return;
    }

    // tear down socket. this signals the STS to detach our socket handler.
    mCondition = NS_BINDING_ABORTED;

    // if we are attached, then socket transport service will call our
    // OnSocketDetached method automatically. Otherwise, we have to call it
    // (and thus close the socket) manually.
    if (!mAttached) {
      OnSocketDetached(mFD);
    }
  }

  void OnMsgAttach()
  {
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    if (NS_FAILED(mCondition)) {
      return;
    }

    mCondition = TryAttach();

    // if we hit an error while trying to attach then bail...
    if (NS_FAILED(mCondition)) {
      NS_ASSERTION(!mAttached, "should not be attached already");
      OnSocketDetached(mFD);
    }

  }

  nsresult TryAttach()
  {
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    nsresult rv;

    if (!gSocketTransportService) {
      return NS_ERROR_FAILURE;
    }

    //
    // find out if it is going to be ok to attach another socket to the STS.
    // if not then we have to wait for the STS to tell us that it is ok.
    // the notification is asynchronous, which means that when we could be
    // in a race to call AttachSocket once notified.  for this reason, when
    // we get notified, we just re-enter this function.  as a result, we are
    // sure to ask again before calling AttachSocket.  in this way we deal
    // with the race condition.  though it isn't the most elegant solution,
    // it is far simpler than trying to build a system that would guarantee
    // FIFO ordering (which wouldn't even be that valuable IMO).  see bug
    // 194402 for more info.
    //
    if (!gSocketTransportService->CanAttachSocket()) {
      nsCOMPtr<nsIRunnable> event =
        NewRunnableMethod(this, &ServiceWatcher::OnMsgAttach);

      nsresult rv = gSocketTransportService->NotifyWhenCanAttachSocket(event);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }

    //
    // ok, we can now attach our socket to the STS for polling
    //
    rv = gSocketTransportService->AttachSocket(mFD, this);
    if (NS_FAILED(rv)) {
      return rv;
    }

    mAttached = true;

    //
    // now, configure our poll flags for listening...
    //
    mPollFlags = (PR_POLL_READ | PR_POLL_EXCEPT);

    return NS_OK;
  }

  nsCOMPtr<nsIThread> mThread;
  RefPtr<nsSocketTransportService> mSts;
  RefPtr<MDNSResponderOperator> mOperatorHolder;
  DNSServiceRef mService;
  PRFileDesc* mFD;
  bool mAttached;
};

NS_IMPL_ISUPPORTS(MDNSResponderOperator::ServiceWatcher, nsISupports)

MDNSResponderOperator::MDNSResponderOperator()
  : mService(nullptr)
  , mWatcher(nullptr)
  , mThread(NS_GetCurrentThread())
  , mIsCancelled(false)
{
}

MDNSResponderOperator::~MDNSResponderOperator()
{
  Stop();
}

nsresult
MDNSResponderOperator::Start()
{
  if (mIsCancelled) {
    return NS_OK;
  }

  if (IsServing()) {
    Stop();
  }

  return NS_OK;
}

nsresult
MDNSResponderOperator::Stop()
{
  return ResetService(nullptr);
}

nsresult
MDNSResponderOperator::ResetService(DNSServiceRef aService)
{
  nsresult rv;

  if (aService != mService) {
    if (mWatcher) {
      mWatcher->Close();
      mWatcher = nullptr;
    }

    if (aService) {
      RefPtr<ServiceWatcher> watcher = new ServiceWatcher(aService, this);
      if (NS_WARN_IF(NS_FAILED(rv = watcher->Init()))) {
        return rv;
      }
      mWatcher = watcher;
    }

    mService = aService;
  }
  return NS_OK;
}

BrowseOperator::BrowseOperator(const nsACString& aServiceType,
                               nsIDNSServiceDiscoveryListener* aListener)
  : MDNSResponderOperator()
  , mServiceType(aServiceType)
  , mListener(aListener)
{
}

nsresult
BrowseOperator::Start()
{
  nsresult rv;
  if (NS_WARN_IF(NS_FAILED(rv = MDNSResponderOperator::Start()))) {
    return rv;
  }

  DNSServiceRef service = nullptr;
  DNSServiceErrorType err = DNSServiceBrowse(&service,
                                             0,
                                             kDNSServiceInterfaceIndexAny,
                                             mServiceType.get(),
                                             nullptr,
                                             &BrowseReplyRunnable::Reply,
                                             this);
  NS_WARN_IF(kDNSServiceErr_NoError != err);

  if (mListener) {
    if (kDNSServiceErr_NoError == err) {
      mListener->OnDiscoveryStarted(mServiceType);
    } else {
      mListener->OnStartDiscoveryFailed(mServiceType, err);
    }
  }

  if (NS_WARN_IF(kDNSServiceErr_NoError != err)) {
    return NS_ERROR_FAILURE;
  }

  return ResetService(service);
}

nsresult
BrowseOperator::Stop()
{
  bool isServing = IsServing();
  nsresult rv = MDNSResponderOperator::Stop();

  if (isServing && mListener) {
    if (NS_SUCCEEDED(rv)) {
      mListener->OnDiscoveryStopped(mServiceType);
    } else {
      mListener->OnStopDiscoveryFailed(mServiceType,
                                       static_cast<uint32_t>(rv));
    }
  }

  return rv;
}

void
BrowseOperator::Reply(DNSServiceRef aSdRef,
                      DNSServiceFlags aFlags,
                      uint32_t aInterfaceIndex,
                      DNSServiceErrorType aErrorCode,
                      const nsACString& aServiceName,
                      const nsACString& aRegType,
                      const nsACString& aReplyDomain)
{
  MOZ_ASSERT(GetThread() == NS_GetCurrentThread());

  if (NS_WARN_IF(kDNSServiceErr_NoError != aErrorCode)) {
    LOG_E("BrowseOperator::Reply (%d)", aErrorCode);
    if (mListener) {
      mListener->OnStartDiscoveryFailed(mServiceType, aErrorCode);
    }
    return;
  }

  if (!mListener) { return; }
  nsCOMPtr<nsIDNSServiceInfo> info = new nsDNSServiceInfo();

  if (NS_WARN_IF(!info)) { return; }
  if (NS_WARN_IF(NS_FAILED(info->SetServiceName(aServiceName)))) { return; }
  if (NS_WARN_IF(NS_FAILED(info->SetServiceType(aRegType)))) { return; }
  if (NS_WARN_IF(NS_FAILED(info->SetDomainName(aReplyDomain)))) { return; }

  if (aFlags & kDNSServiceFlagsAdd) {
    mListener->OnServiceFound(info);
  } else {
    mListener->OnServiceLost(info);
  }
}

RegisterOperator::RegisterOperator(nsIDNSServiceInfo* aServiceInfo,
                                   nsIDNSRegistrationListener* aListener)
  : MDNSResponderOperator()
  , mServiceInfo(aServiceInfo)
  , mListener(aListener)
{
}

nsresult
RegisterOperator::Start()
{
  nsresult rv;

  rv = MDNSResponderOperator::Start();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  uint16_t port;
  if (NS_WARN_IF(NS_FAILED(rv = mServiceInfo->GetPort(&port)))) {
    return rv;
  }
  nsAutoCString type;
  if (NS_WARN_IF(NS_FAILED(rv = mServiceInfo->GetServiceType(type)))) {
    return rv;
  }

  TXTRecordRef txtRecord;
  char buf[TXT_BUFFER_SIZE] = { 0 };
  TXTRecordCreate(&txtRecord, TXT_BUFFER_SIZE, buf);

  nsCOMPtr<nsIPropertyBag2> attributes;
  if (NS_FAILED(rv = mServiceInfo->GetAttributes(getter_AddRefs(attributes)))) {
    LOG_I("register: no attributes");
  } else {
    nsCOMPtr<nsISimpleEnumerator> enumerator;
    if (NS_WARN_IF(NS_FAILED(rv =
        attributes->GetEnumerator(getter_AddRefs(enumerator))))) {
      return rv;
    }

    bool hasMoreElements;
    while (NS_SUCCEEDED(enumerator->HasMoreElements(&hasMoreElements)) &&
           hasMoreElements) {
      nsCOMPtr<nsISupports> element;
      MOZ_ALWAYS_SUCCEEDS(enumerator->GetNext(getter_AddRefs(element)));
      nsCOMPtr<nsIProperty> property = do_QueryInterface(element);
      MOZ_ASSERT(property);

      nsAutoString name;
      nsCOMPtr<nsIVariant> value;
      MOZ_ALWAYS_SUCCEEDS(property->GetName(name));
      MOZ_ALWAYS_SUCCEEDS(property->GetValue(getter_AddRefs(value)));

      nsAutoCString str;
      if (NS_WARN_IF(NS_FAILED(value->GetAsACString(str)))) {
        continue;
      }

      TXTRecordSetValue(&txtRecord,
                        /* it's safe because key name is ASCII only. */
                        NS_LossyConvertUTF16toASCII(name).get(),
                        str.Length(),
                        str.get());
    }
  }

  nsAutoCString host;
  nsAutoCString name;
  nsAutoCString domain;

  DNSServiceRef service = nullptr;
  DNSServiceErrorType err =
    DNSServiceRegister(&service,
                       0,
                       0,
                       NS_SUCCEEDED(mServiceInfo->GetServiceName(name)) ?
                         name.get() : nullptr,
                       type.get(),
                       NS_SUCCEEDED(mServiceInfo->GetDomainName(domain)) ?
                         domain.get() : nullptr,
                       NS_SUCCEEDED(mServiceInfo->GetHost(host)) ?
                         host.get() : nullptr,
                       NativeEndian::swapToNetworkOrder(port),
                       TXTRecordGetLength(&txtRecord),
                       TXTRecordGetBytesPtr(&txtRecord),
                       &RegisterReplyRunnable::Reply,
                       this);
  NS_WARN_IF(kDNSServiceErr_NoError != err);

  TXTRecordDeallocate(&txtRecord);

  if (NS_WARN_IF(kDNSServiceErr_NoError != err)) {
    if (mListener) {
      mListener->OnRegistrationFailed(mServiceInfo, err);
    }
    return NS_ERROR_FAILURE;
  }

  return ResetService(service);
}

nsresult
RegisterOperator::Stop()
{
  bool isServing = IsServing();
  nsresult rv = MDNSResponderOperator::Stop();

  if (isServing && mListener) {
    if (NS_SUCCEEDED(rv)) {
      mListener->OnServiceUnregistered(mServiceInfo);
    } else {
      mListener->OnUnregistrationFailed(mServiceInfo,
                                        static_cast<uint32_t>(rv));
    }
  }

  return rv;
}

void
RegisterOperator::Reply(DNSServiceRef aSdRef,
                        DNSServiceFlags aFlags,
                        DNSServiceErrorType aErrorCode,
                        const nsACString& aName,
                        const nsACString& aRegType,
                        const nsACString& aDomain)
{
  MOZ_ASSERT(GetThread() == NS_GetCurrentThread());

  if (kDNSServiceErr_NoError != aErrorCode) {
    LOG_E("RegisterOperator::Reply (%d)", aErrorCode);
  }

  if (!mListener) { return; }
  nsCOMPtr<nsIDNSServiceInfo> info = new nsDNSServiceInfo(mServiceInfo);
  if (NS_WARN_IF(NS_FAILED(info->SetServiceName(aName)))) { return; }
  if (NS_WARN_IF(NS_FAILED(info->SetServiceType(aRegType)))) { return; }
  if (NS_WARN_IF(NS_FAILED(info->SetDomainName(aDomain)))) { return; }

  if (kDNSServiceErr_NoError == aErrorCode) {
    if (aFlags & kDNSServiceFlagsAdd) {
      mListener->OnServiceRegistered(info);
    } else {
      // If a successfully-registered name later suffers a name conflict
      // or similar problem and has to be deregistered, the callback will
      // be invoked with the kDNSServiceFlagsAdd flag not set.
      LOG_E("RegisterOperator::Reply: deregister");
      if (NS_WARN_IF(NS_FAILED(Stop()))) {
        return;
      }
    }
  } else {
    mListener->OnRegistrationFailed(info, aErrorCode);
  }
}

ResolveOperator::ResolveOperator(nsIDNSServiceInfo* aServiceInfo,
                                 nsIDNSServiceResolveListener* aListener)
  : MDNSResponderOperator()
  , mServiceInfo(aServiceInfo)
  , mListener(aListener)
{
}

nsresult
ResolveOperator::Start()
{
  nsresult rv;
  if (NS_WARN_IF(NS_FAILED(rv = MDNSResponderOperator::Start()))) {
    return rv;
  }

  nsAutoCString name;
  mServiceInfo->GetServiceName(name);
  nsAutoCString type;
  mServiceInfo->GetServiceType(type);
  nsAutoCString domain;
  mServiceInfo->GetDomainName(domain);

  LOG_I("Resolve: (%s), (%s), (%s)", name.get(), type.get(), domain.get());

  DNSServiceRef service = nullptr;
  DNSServiceErrorType err =
    DNSServiceResolve(&service,
                      0,
                      kDNSServiceInterfaceIndexAny,
                      name.get(),
                      type.get(),
                      domain.get(),
                      (DNSServiceResolveReply)&ResolveReplyRunnable::Reply,
                      this);

  if (NS_WARN_IF(kDNSServiceErr_NoError != err)) {
    if (mListener) {
      mListener->OnResolveFailed(mServiceInfo, err);
    }
    return NS_ERROR_FAILURE;
  }

  return ResetService(service);
}

void
ResolveOperator::Reply(DNSServiceRef aSdRef,
                       DNSServiceFlags aFlags,
                       uint32_t aInterfaceIndex,
                       DNSServiceErrorType aErrorCode,
                       const nsACString& aFullName,
                       const nsACString& aHostTarget,
                       uint16_t aPort,
                       uint16_t aTxtLen,
                       const unsigned char* aTxtRecord)
{
  MOZ_ASSERT(GetThread() == NS_GetCurrentThread());

  auto guard = MakeScopeExit([this] {
    NS_WARN_IF(NS_FAILED(Stop()));
  });

  if (NS_WARN_IF(kDNSServiceErr_NoError != aErrorCode)) {
    LOG_E("ResolveOperator::Reply (%d)", aErrorCode);
    return;
  }

  // Resolve TXT record
  int count = TXTRecordGetCount(aTxtLen, aTxtRecord);
  LOG_I("resolve: txt count = %d, len = %d", count, aTxtLen);
  nsCOMPtr<nsIWritablePropertyBag2> attributes = nullptr;
  if (count) {
    attributes = new nsHashPropertyBag();
    if (NS_WARN_IF(!attributes)) { return; }
    for (int i = 0; i < count; ++i) {
      char key[TXT_BUFFER_SIZE] = { '\0' };
      uint8_t vSize = 0;
      const void* value = nullptr;
      if (kDNSServiceErr_NoError !=
          TXTRecordGetItemAtIndex(aTxtLen,
                                  aTxtRecord,
                                  i,
                                  TXT_BUFFER_SIZE,
                                  key,
                                  &vSize,
                                  &value)) {
        break;
      }

      nsAutoCString str(reinterpret_cast<const char*>(value), vSize);
      LOG_I("resolve TXT: (%d) %s=%s", vSize, key, str.get());

      if (NS_WARN_IF(NS_FAILED(attributes->SetPropertyAsACString(
          /* it's safe to convert because key name is ASCII only. */
          NS_ConvertASCIItoUTF16(key),
          str)))) {
        break;
      }
    }
  }

  if (!mListener) { return; }
  nsCOMPtr<nsIDNSServiceInfo> info = new nsDNSServiceInfo(mServiceInfo);
  if (NS_WARN_IF(NS_FAILED(info->SetHost(aHostTarget)))) { return; }
  if (NS_WARN_IF(NS_FAILED(info->SetPort(aPort)))) { return; }
  if (NS_WARN_IF(NS_FAILED(info->SetAttributes(attributes)))) { return; }

  if (kDNSServiceErr_NoError == aErrorCode) {
    GetAddrInfor(info);
  }
  else {
    mListener->OnResolveFailed(info, aErrorCode);
    NS_WARN_IF(NS_FAILED(Stop()));
  }
}

void
ResolveOperator::GetAddrInfor(nsIDNSServiceInfo* aServiceInfo)
{
  RefPtr<GetAddrInfoOperator> getAddreOp = new GetAddrInfoOperator(aServiceInfo,
                                                                   mListener);
  NS_WARN_IF(NS_FAILED(getAddreOp->Start()));
}

GetAddrInfoOperator::GetAddrInfoOperator(nsIDNSServiceInfo* aServiceInfo,
                                         nsIDNSServiceResolveListener* aListener)
  : MDNSResponderOperator()
  , mServiceInfo(aServiceInfo)
  , mListener(aListener)
{
}

nsresult
GetAddrInfoOperator::Start()
{
  nsresult rv;
  if (NS_WARN_IF(NS_FAILED(rv = MDNSResponderOperator::Start()))) {
    return rv;
  }

  nsAutoCString host;
  mServiceInfo->GetHost(host);

  LOG_I("GetAddrInfo: (%s)", host.get());

  DNSServiceRef service = nullptr;
  DNSServiceErrorType err =
    DNSServiceGetAddrInfo(&service,
                          kDNSServiceFlagsForceMulticast,
                          kDNSServiceInterfaceIndexAny,
                          kDNSServiceProtocol_IPv4 | kDNSServiceProtocol_IPv6,
                          host.get(),
                          (DNSServiceGetAddrInfoReply)&GetAddrInfoReplyRunnable::Reply,
                          this);

  if (NS_WARN_IF(kDNSServiceErr_NoError != err)) {
    if (mListener) {
      mListener->OnResolveFailed(mServiceInfo, err);
    }
    return NS_ERROR_FAILURE;
  }

  return ResetService(service);
}

void
GetAddrInfoOperator::Reply(DNSServiceRef aSdRef,
                           DNSServiceFlags aFlags,
                           uint32_t aInterfaceIndex,
                           DNSServiceErrorType aErrorCode,
                           const nsACString& aHostName,
                           const NetAddr& aAddress,
                           uint32_t aTTL)
{
  MOZ_ASSERT(GetThread() == NS_GetCurrentThread());

  auto guard = MakeScopeExit([this] {
    NS_WARN_IF(NS_FAILED(Stop()));
  });

  if (NS_WARN_IF(kDNSServiceErr_NoError != aErrorCode)) {
    LOG_E("GetAddrInfoOperator::Reply (%d)", aErrorCode);
    return;
  }

  if (!mListener) { return; }

  NetAddr addr = aAddress;
  nsCOMPtr<nsINetAddr> address = new nsNetAddr(&addr);
  nsCString addressStr;
  if (NS_WARN_IF(NS_FAILED(address->GetAddress(addressStr)))) { return; }

  nsCOMPtr<nsIDNSServiceInfo> info = new nsDNSServiceInfo(mServiceInfo);
  if (NS_WARN_IF(NS_FAILED(info->SetAddress(addressStr)))) { return; }

  /**
   * |kDNSServiceFlagsMoreComing| means this callback will be one or more
   * callback events later, so this instance should be kept alive until all
   * follow-up events are processed.
   */
  if (aFlags & kDNSServiceFlagsMoreComing) {
    guard.release();
  }

  if (kDNSServiceErr_NoError == aErrorCode) {
    mListener->OnServiceResolved(info);
  } else {
    mListener->OnResolveFailed(info, aErrorCode);
  }
}

} // namespace net
} // namespace mozilla
