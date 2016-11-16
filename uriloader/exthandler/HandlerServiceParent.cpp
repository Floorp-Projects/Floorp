#include "HandlerServiceParent.h"
#include "nsIHandlerService.h"
#include "nsIMIMEInfo.h"
#include "ContentHandlerService.h"

using mozilla::dom::HandlerInfo;
using mozilla::dom::HandlerApp;
using mozilla::dom::ContentHandlerService;
using mozilla::dom::RemoteHandlerApp;

namespace {

class ProxyHandlerInfo final : public nsIHandlerInfo {
public:
  explicit ProxyHandlerInfo(const HandlerInfo& aHandlerInfo);
  NS_DECL_ISUPPORTS;
  NS_DECL_NSIHANDLERINFO;
protected:
  ~ProxyHandlerInfo() {}
  HandlerInfo mHandlerInfo;
  nsHandlerInfoAction mPrefAction;
  nsCOMPtr<nsIMutableArray> mPossibleApps;
};

NS_IMPL_ISUPPORTS(ProxyHandlerInfo, nsIHandlerInfo)

ProxyHandlerInfo::ProxyHandlerInfo(const HandlerInfo& aHandlerInfo) : mHandlerInfo(aHandlerInfo), mPossibleApps(do_CreateInstance(NS_ARRAY_CONTRACTID))
{
  for (auto& happ : aHandlerInfo.possibleApplicationHandlers()) {
    mPossibleApps->AppendElement(new RemoteHandlerApp(happ), false);
  }
}

/* readonly attribute ACString type; */
NS_IMETHODIMP ProxyHandlerInfo::GetType(nsACString & aType)
{
  aType.Assign(mHandlerInfo.type());
  return NS_OK;
}

/* attribute AString description; */
NS_IMETHODIMP ProxyHandlerInfo::GetDescription(nsAString & aDescription)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP ProxyHandlerInfo::SetDescription(const nsAString & aDescription)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsIHandlerApp preferredApplicationHandler; */
NS_IMETHODIMP ProxyHandlerInfo::GetPreferredApplicationHandler(nsIHandlerApp * *aPreferredApplicationHandler)
{
  *aPreferredApplicationHandler = new RemoteHandlerApp(mHandlerInfo.preferredApplicationHandler());
  NS_IF_ADDREF(*aPreferredApplicationHandler);
  return NS_OK;
}

NS_IMETHODIMP ProxyHandlerInfo::SetPreferredApplicationHandler(nsIHandlerApp *aApp)
{
  nsString name;
  nsString detailedDescription;
  if (aApp) {
    aApp->GetName(name);
    aApp->GetDetailedDescription(detailedDescription);
  }
  HandlerApp happ(name, detailedDescription);
  mHandlerInfo = HandlerInfo(mHandlerInfo.type(),
                             mHandlerInfo.isMIMEInfo(),
                             mHandlerInfo.description(),
                             mHandlerInfo.alwaysAskBeforeHandling(),
                             happ,
                             mHandlerInfo.possibleApplicationHandlers(),
                             mHandlerInfo.preferredAction());
  return NS_OK;
}

/* readonly attribute nsIMutableArray possibleApplicationHandlers; */
NS_IMETHODIMP ProxyHandlerInfo::GetPossibleApplicationHandlers(nsIMutableArray * *aPossibleApplicationHandlers)
{
  *aPossibleApplicationHandlers = mPossibleApps;
  NS_IF_ADDREF(*aPossibleApplicationHandlers);
  return NS_OK;
}

/* readonly attribute boolean hasDefaultHandler; */
NS_IMETHODIMP ProxyHandlerInfo::GetHasDefaultHandler(bool *aHasDefaultHandler)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute AString defaultDescription; */
NS_IMETHODIMP ProxyHandlerInfo::GetDefaultDescription(nsAString & aDefaultDescription)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void launchWithURI (in nsIURI aURI, [optional] in nsIInterfaceRequestor aWindowContext); */
NS_IMETHODIMP ProxyHandlerInfo::LaunchWithURI(nsIURI *aURI, nsIInterfaceRequestor *aWindowContext)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute ProxyHandlerInfoAction preferredAction; */
NS_IMETHODIMP ProxyHandlerInfo::GetPreferredAction(nsHandlerInfoAction *aPreferredAction)
{
  *aPreferredAction = mPrefAction;
  return NS_OK;
}
NS_IMETHODIMP ProxyHandlerInfo::SetPreferredAction(nsHandlerInfoAction aPreferredAction)
{
  mHandlerInfo = HandlerInfo(mHandlerInfo.type(),
                             mHandlerInfo.isMIMEInfo(),
                             mHandlerInfo.description(),
                             mHandlerInfo.alwaysAskBeforeHandling(),
                             mHandlerInfo.preferredApplicationHandler(),
                             mHandlerInfo.possibleApplicationHandlers(),
                             aPreferredAction);
  mPrefAction = aPreferredAction;
  return NS_OK;
}

/* attribute boolean alwaysAskBeforeHandling; */
NS_IMETHODIMP ProxyHandlerInfo::GetAlwaysAskBeforeHandling(bool *aAlwaysAskBeforeHandling)
{
  *aAlwaysAskBeforeHandling = mHandlerInfo.alwaysAskBeforeHandling();
  return NS_OK;
}
NS_IMETHODIMP ProxyHandlerInfo::SetAlwaysAskBeforeHandling(bool aAlwaysAskBeforeHandling)
{
  mHandlerInfo = HandlerInfo(mHandlerInfo.type(),
                             mHandlerInfo.isMIMEInfo(),
                             mHandlerInfo.description(),
                             aAlwaysAskBeforeHandling,
                             mHandlerInfo.preferredApplicationHandler(),
                             mHandlerInfo.possibleApplicationHandlers(),
                             mHandlerInfo.preferredAction());
  return NS_OK;
}


class ProxyMIMEInfo : public nsIMIMEInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMIMEINFO
  NS_FORWARD_NSIHANDLERINFO(mProxyHandlerInfo->);

  explicit ProxyMIMEInfo(HandlerInfo aHandlerInfo) : mProxyHandlerInfo(new ProxyHandlerInfo(aHandlerInfo)) {}

private:
  virtual ~ProxyMIMEInfo() {}
  nsCOMPtr<nsIHandlerInfo> mProxyHandlerInfo;

protected:
  /* additional members */
};

NS_IMPL_ISUPPORTS(ProxyMIMEInfo, nsIMIMEInfo, nsIHandlerInfo)

/* nsIUTF8StringEnumerator getFileExtensions (); */
NS_IMETHODIMP ProxyMIMEInfo::GetFileExtensions(nsIUTF8StringEnumerator * *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setFileExtensions (in AUTF8String aExtensions); */
NS_IMETHODIMP ProxyMIMEInfo::SetFileExtensions(const nsACString & aExtensions)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean extensionExists (in AUTF8String aExtension); */
NS_IMETHODIMP ProxyMIMEInfo::ExtensionExists(const nsACString & aExtension, bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void appendExtension (in AUTF8String aExtension); */
NS_IMETHODIMP ProxyMIMEInfo::AppendExtension(const nsACString & aExtension)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute AUTF8String primaryExtension; */
NS_IMETHODIMP ProxyMIMEInfo::GetPrimaryExtension(nsACString & aPrimaryExtension)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP ProxyMIMEInfo::SetPrimaryExtension(const nsACString & aPrimaryExtension)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute ACString MIMEType; */
NS_IMETHODIMP ProxyMIMEInfo::GetMIMEType(nsACString & aMIMEType)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean equals (in nsIMIMEInfo aMIMEInfo); */
NS_IMETHODIMP ProxyMIMEInfo::Equals(nsIMIMEInfo *aMIMEInfo, bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIArray possibleLocalHandlers; */
NS_IMETHODIMP ProxyMIMEInfo::GetPossibleLocalHandlers(nsIArray * *aPossibleLocalHandlers)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void launchWithFile (in nsIFile aFile); */
NS_IMETHODIMP ProxyMIMEInfo::LaunchWithFile(nsIFile *aFile)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

static already_AddRefed<nsIHandlerInfo> WrapHandlerInfo(const HandlerInfo& aHandlerInfo) {
  nsCOMPtr<nsIHandlerInfo> info;
  if (aHandlerInfo.isMIMEInfo()) {
    info = new ProxyMIMEInfo(aHandlerInfo);
  } else {
    info = new ProxyHandlerInfo(aHandlerInfo);
  }
  return info.forget();
}

} // anonymous namespace

HandlerServiceParent::HandlerServiceParent()
{
}

HandlerServiceParent::~HandlerServiceParent()
{
}

mozilla::ipc::IPCResult
HandlerServiceParent::RecvFillHandlerInfo(const HandlerInfo& aHandlerInfoData,
                                          const nsCString& aOverrideType,
                                          HandlerInfo* handlerInfoData)
{
  nsCOMPtr<nsIHandlerInfo> info(WrapHandlerInfo(aHandlerInfoData));
  nsCOMPtr<nsIHandlerService> handlerSvc = do_GetService(NS_HANDLERSERVICE_CONTRACTID);
  handlerSvc->FillHandlerInfo(info, aOverrideType);
  ContentHandlerService::nsIHandlerInfoToHandlerInfo(info, handlerInfoData);
  return IPC_OK();
}

mozilla::ipc::IPCResult
HandlerServiceParent::RecvExists(const HandlerInfo& aHandlerInfo,
                                 bool* exists)
{
  nsCOMPtr<nsIHandlerInfo> info(WrapHandlerInfo(aHandlerInfo));
  nsCOMPtr<nsIHandlerService> handlerSvc = do_GetService(NS_HANDLERSERVICE_CONTRACTID);
  handlerSvc->Exists(info, exists);
  return IPC_OK();
}

mozilla::ipc::IPCResult
HandlerServiceParent::RecvGetTypeFromExtension(const nsCString& aFileExtension,
                                               nsCString* type)
{
  nsCOMPtr<nsIHandlerService> handlerSvc = do_GetService(NS_HANDLERSERVICE_CONTRACTID);
  handlerSvc->GetTypeFromExtension(aFileExtension, *type);
  return IPC_OK();
}

void HandlerServiceParent::ActorDestroy(ActorDestroyReason aWhy)
{
}
