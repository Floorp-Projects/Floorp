#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/Logging.h"
#include "HandlerServiceParent.h"
#include "nsIHandlerService.h"
#include "nsIMIMEInfo.h"
#include "ContentHandlerService.h"
#include "nsStringEnumerator.h"
#ifdef MOZ_WIDGET_GTK
#  include "unix/nsGNOMERegistry.h"
#endif

using mozilla::dom::ContentHandlerService;
using mozilla::dom::HandlerApp;
using mozilla::dom::HandlerInfo;
using mozilla::dom::RemoteHandlerApp;

namespace {

class ProxyHandlerInfo final : public nsIHandlerInfo {
 public:
  explicit ProxyHandlerInfo(const HandlerInfo& aHandlerInfo);
  NS_DECL_ISUPPORTS;
  NS_DECL_NSIHANDLERINFO;

  nsTArray<nsCString>& Extensions() { return mHandlerInfo.extensions(); }

 protected:
  ~ProxyHandlerInfo() {}
  HandlerInfo mHandlerInfo;
  nsHandlerInfoAction mPrefAction;
  nsCOMPtr<nsIMutableArray> mPossibleApps;
};

NS_IMPL_ISUPPORTS(ProxyHandlerInfo, nsIHandlerInfo)

ProxyHandlerInfo::ProxyHandlerInfo(const HandlerInfo& aHandlerInfo)
    : mHandlerInfo(aHandlerInfo),
      mPrefAction(nsIHandlerInfo::alwaysAsk),
      mPossibleApps(do_CreateInstance(NS_ARRAY_CONTRACTID)) {
  for (auto& happ : aHandlerInfo.possibleApplicationHandlers()) {
    mPossibleApps->AppendElement(new RemoteHandlerApp(happ));
  }
}

/* readonly attribute ACString type; */
NS_IMETHODIMP ProxyHandlerInfo::GetType(nsACString& aType) {
  aType.Assign(mHandlerInfo.type());
  return NS_OK;
}

/* attribute AString description; */
NS_IMETHODIMP ProxyHandlerInfo::GetDescription(nsAString& aDescription) {
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP ProxyHandlerInfo::SetDescription(const nsAString& aDescription) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsIHandlerApp preferredApplicationHandler; */
NS_IMETHODIMP ProxyHandlerInfo::GetPreferredApplicationHandler(
    nsIHandlerApp** aPreferredApplicationHandler) {
  *aPreferredApplicationHandler =
      new RemoteHandlerApp(mHandlerInfo.preferredApplicationHandler());
  NS_IF_ADDREF(*aPreferredApplicationHandler);
  return NS_OK;
}

NS_IMETHODIMP ProxyHandlerInfo::SetPreferredApplicationHandler(
    nsIHandlerApp* aApp) {
  nsString name;
  nsString detailedDescription;
  if (aApp) {
    aApp->GetName(name);
    aApp->GetDetailedDescription(detailedDescription);
  }

  mHandlerInfo.preferredApplicationHandler() =
      HandlerApp(name, detailedDescription);
  return NS_OK;
}

/* readonly attribute nsIMutableArray possibleApplicationHandlers; */
NS_IMETHODIMP ProxyHandlerInfo::GetPossibleApplicationHandlers(
    nsIMutableArray** aPossibleApplicationHandlers) {
  *aPossibleApplicationHandlers = mPossibleApps;
  NS_IF_ADDREF(*aPossibleApplicationHandlers);
  return NS_OK;
}

/* readonly attribute boolean hasDefaultHandler; */
NS_IMETHODIMP ProxyHandlerInfo::GetHasDefaultHandler(bool* aHasDefaultHandler) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute AString defaultDescription; */
NS_IMETHODIMP ProxyHandlerInfo::GetDefaultDescription(
    nsAString& aDefaultDescription) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void launchWithURI (in nsIURI aURI,
                       [optional] in nsIInterfaceRequestor aWindowContext); */
NS_IMETHODIMP ProxyHandlerInfo::LaunchWithURI(
    nsIURI* aURI, nsIInterfaceRequestor* aWindowContext) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute ProxyHandlerInfoAction preferredAction; */
NS_IMETHODIMP ProxyHandlerInfo::GetPreferredAction(
    nsHandlerInfoAction* aPreferredAction) {
  *aPreferredAction = mPrefAction;
  return NS_OK;
}
NS_IMETHODIMP ProxyHandlerInfo::SetPreferredAction(
    nsHandlerInfoAction aPreferredAction) {
  mHandlerInfo.preferredAction() = aPreferredAction;
  mPrefAction = aPreferredAction;
  return NS_OK;
}

/* attribute boolean alwaysAskBeforeHandling; */
NS_IMETHODIMP ProxyHandlerInfo::GetAlwaysAskBeforeHandling(
    bool* aAlwaysAskBeforeHandling) {
  *aAlwaysAskBeforeHandling = mHandlerInfo.alwaysAskBeforeHandling();
  return NS_OK;
}
NS_IMETHODIMP ProxyHandlerInfo::SetAlwaysAskBeforeHandling(
    bool aAlwaysAskBeforeHandling) {
  mHandlerInfo.alwaysAskBeforeHandling() = aAlwaysAskBeforeHandling;
  return NS_OK;
}

class ProxyMIMEInfo : public nsIMIMEInfo {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMIMEINFO
  NS_FORWARD_NSIHANDLERINFO(mProxyHandlerInfo->);

  explicit ProxyMIMEInfo(const HandlerInfo& aHandlerInfo)
      : mProxyHandlerInfo(new ProxyHandlerInfo(aHandlerInfo)) {}

 private:
  virtual ~ProxyMIMEInfo() {}
  RefPtr<ProxyHandlerInfo> mProxyHandlerInfo;

 protected:
  /* additional members */
};

NS_IMPL_ISUPPORTS(ProxyMIMEInfo, nsIMIMEInfo, nsIHandlerInfo)

/* nsIUTF8StringEnumerator getFileExtensions (); */
NS_IMETHODIMP ProxyMIMEInfo::GetFileExtensions(
    nsIUTF8StringEnumerator** _retval) {
  return NS_NewUTF8StringEnumerator(_retval, &mProxyHandlerInfo->Extensions(),
                                    this);
}

/* void setFileExtensions (in AUTF8String aExtensions); */
NS_IMETHODIMP ProxyMIMEInfo::SetFileExtensions(const nsACString& aExtensions) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean extensionExists (in AUTF8String aExtension); */
NS_IMETHODIMP ProxyMIMEInfo::ExtensionExists(const nsACString& aExtension,
                                             bool* _retval) {
  *_retval = mProxyHandlerInfo->Extensions().Contains(aExtension);
  return NS_OK;
}

/* void appendExtension (in AUTF8String aExtension); */
NS_IMETHODIMP ProxyMIMEInfo::AppendExtension(const nsACString& aExtension) {
  mProxyHandlerInfo->Extensions().AppendElement(aExtension);
  return NS_OK;
}

/* attribute AUTF8String primaryExtension; */
NS_IMETHODIMP ProxyMIMEInfo::GetPrimaryExtension(
    nsACString& aPrimaryExtension) {
  const auto& extensions = mProxyHandlerInfo->Extensions();
  if (extensions.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }
  aPrimaryExtension = extensions[0];
  return NS_OK;
}

NS_IMETHODIMP ProxyMIMEInfo::SetPrimaryExtension(
    const nsACString& aPrimaryExtension) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute ACString MIMEType; */
NS_IMETHODIMP ProxyMIMEInfo::GetMIMEType(nsACString& aMIMEType) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean equals (in nsIMIMEInfo aMIMEInfo); */
NS_IMETHODIMP ProxyMIMEInfo::Equals(nsIMIMEInfo* aMIMEInfo, bool* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIArray possibleLocalHandlers; */
NS_IMETHODIMP ProxyMIMEInfo::GetPossibleLocalHandlers(
    nsIArray** aPossibleLocalHandlers) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void launchWithFile (in nsIFile aFile); */
NS_IMETHODIMP ProxyMIMEInfo::LaunchWithFile(nsIFile* aFile) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

static already_AddRefed<nsIHandlerInfo> WrapHandlerInfo(
    const HandlerInfo& aHandlerInfo) {
  nsCOMPtr<nsIHandlerInfo> info;
  if (aHandlerInfo.isMIMEInfo()) {
    info = new ProxyMIMEInfo(aHandlerInfo);
  } else {
    info = new ProxyHandlerInfo(aHandlerInfo);
  }
  return info.forget();
}

}  // anonymous namespace

HandlerServiceParent::HandlerServiceParent() {}

HandlerServiceParent::~HandlerServiceParent() {}

mozilla::ipc::IPCResult HandlerServiceParent::RecvFillHandlerInfo(
    const HandlerInfo& aHandlerInfoData, const nsCString& aOverrideType,
    HandlerInfo* handlerInfoData) {
  nsCOMPtr<nsIHandlerInfo> info(WrapHandlerInfo(aHandlerInfoData));
  nsCOMPtr<nsIHandlerService> handlerSvc =
      do_GetService(NS_HANDLERSERVICE_CONTRACTID);
  handlerSvc->FillHandlerInfo(info, aOverrideType);
  ContentHandlerService::nsIHandlerInfoToHandlerInfo(info, handlerInfoData);
  return IPC_OK();
}

mozilla::ipc::IPCResult HandlerServiceParent::RecvGetMIMEInfoFromOS(
    const nsCString& aMIMEType, const nsCString& aExtension, nsresult* aRv,
    HandlerInfo* aHandlerInfoData, bool* aFound) {
  *aFound = false;
  if (aMIMEType.Length() > MAX_MIMETYPE_LENGTH ||
      aExtension.Length() > MAX_EXT_LENGTH) {
    *aRv = NS_OK;
    return IPC_OK();
  }

  nsCOMPtr<nsIMIMEService> mimeService =
      do_GetService(NS_MIMESERVICE_CONTRACTID, aRv);
  if (NS_WARN_IF(NS_FAILED(*aRv))) {
    return IPC_OK();
  }

  nsCOMPtr<nsIMIMEInfo> mimeInfo;
  *aRv = mimeService->GetMIMEInfoFromOS(aMIMEType, aExtension, aFound,
                                        getter_AddRefs(mimeInfo));
  if (NS_WARN_IF(NS_FAILED(*aRv))) {
    return IPC_OK();
  }

  if (mimeInfo) {
    ContentHandlerService::nsIHandlerInfoToHandlerInfo(mimeInfo,
                                                       aHandlerInfoData);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult HandlerServiceParent::RecvExists(
    const HandlerInfo& aHandlerInfo, bool* exists) {
  nsCOMPtr<nsIHandlerInfo> info(WrapHandlerInfo(aHandlerInfo));
  nsCOMPtr<nsIHandlerService> handlerSvc =
      do_GetService(NS_HANDLERSERVICE_CONTRACTID);
  handlerSvc->Exists(info, exists);
  return IPC_OK();
}

mozilla::ipc::IPCResult HandlerServiceParent::RecvExistsForProtocolOS(
    const nsCString& aProtocolScheme, bool* aHandlerExists) {
  if (aProtocolScheme.Length() > MAX_SCHEME_LENGTH) {
    *aHandlerExists = false;
    return IPC_OK();
  }
#ifdef MOZ_WIDGET_GTK
  // Check the GNOME registry for a protocol handler
  *aHandlerExists = nsGNOMERegistry::HandlerExists(aProtocolScheme.get());
#else
  *aHandlerExists = false;
#endif
  return IPC_OK();
}

/*
 * Check if a handler exists for the provided protocol. Check the datastore
 * first and then fallback to checking the OS for a handler.
 */
mozilla::ipc::IPCResult HandlerServiceParent::RecvExistsForProtocol(
    const nsCString& aProtocolScheme, bool* aHandlerExists) {
  if (aProtocolScheme.Length() > MAX_SCHEME_LENGTH) {
    *aHandlerExists = false;
    return IPC_OK();
  }
#if defined(XP_MACOSX)
  // Check the datastore and fallback to an OS check.
  // ExternalProcotolHandlerExists() does the fallback.
  nsresult rv;
  nsCOMPtr<nsIExternalProtocolService> protoSvc =
      do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    *aHandlerExists = false;
    return IPC_OK();
  }
  rv = protoSvc->ExternalProtocolHandlerExists(aProtocolScheme.get(),
                                               aHandlerExists);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    *aHandlerExists = false;
  }
#else
  MOZ_RELEASE_ASSERT(false, "No implementation on this platform.");
  *aHandlerExists = false;
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult HandlerServiceParent::RecvGetTypeFromExtension(
    const nsCString& aFileExtension, nsCString* type) {
  if (aFileExtension.Length() > MAX_EXT_LENGTH) {
    return IPC_OK();
  }

  nsresult rv;
  nsCOMPtr<nsIHandlerService> handlerSvc =
      do_GetService(NS_HANDLERSERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_OK();
  }

  rv = handlerSvc->GetTypeFromExtension(aFileExtension, *type);
  mozilla::Unused << NS_WARN_IF(NS_FAILED(rv));

  return IPC_OK();
}

mozilla::ipc::IPCResult HandlerServiceParent::RecvGetApplicationDescription(
    const nsCString& aScheme, nsresult* aRv, nsString* aDescription) {
  if (aScheme.Length() > MAX_SCHEME_LENGTH) {
    *aRv = NS_ERROR_NOT_AVAILABLE;
    return IPC_OK();
  }
  nsCOMPtr<nsIExternalProtocolService> protoSvc =
      do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID);
  NS_ASSERTION(protoSvc, "No Helper App Service!");
  *aRv = protoSvc->GetApplicationDescription(aScheme, *aDescription);
  return IPC_OK();
}

void HandlerServiceParent::ActorDestroy(ActorDestroyReason aWhy) {}
