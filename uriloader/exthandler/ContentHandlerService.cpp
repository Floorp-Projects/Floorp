#include "ContentHandlerService.h"
#include "HandlerServiceChild.h"
#include "ContentChild.h"
#include "nsIMutableArray.h"
#include "nsIMIMEInfo.h"

using mozilla::dom::ContentChild;
using mozilla::dom::PHandlerServiceChild;
using mozilla::dom::HandlerInfo;

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(ContentHandlerService, nsIHandlerService)

ContentHandlerService::ContentHandlerService()
{
}

nsresult
ContentHandlerService::Init()
{
  if (!XRE_IsContentProcess()) {
    return NS_ERROR_FAILURE;
  }
  ContentChild* cpc = ContentChild::GetSingleton();

  mHandlerServiceChild = static_cast<HandlerServiceChild*>(cpc->SendPHandlerServiceConstructor());
  return NS_OK;
}

void
ContentHandlerService::nsIHandlerInfoToHandlerInfo(nsIHandlerInfo* aInfo,
                                                   HandlerInfo* aHandlerInfo)
{
  nsCString type;
  aInfo->GetType(type);
  nsCOMPtr<nsIMIMEInfo> mimeInfo = do_QueryInterface(aInfo);
  bool isMIMEInfo = !!mimeInfo;
  nsString description;
  aInfo->GetDescription(description);
  bool alwaysAskBeforeHandling;
  aInfo->GetAlwaysAskBeforeHandling(&alwaysAskBeforeHandling);
  nsCOMPtr<nsIHandlerApp> app;
  aInfo->GetPreferredApplicationHandler(getter_AddRefs(app));
  nsString name;
  nsString detailedDescription;
  if (app) {
    app->GetName(name);
    app->GetDetailedDescription(detailedDescription);
  }
  HandlerApp happ(name, detailedDescription);
  nsTArray<HandlerApp> happs;
  nsCOMPtr<nsIMutableArray> apps;
  aInfo->GetPossibleApplicationHandlers(getter_AddRefs(apps));
  if (apps) {
    unsigned int length;
    apps->GetLength(&length);
    for (unsigned int i = 0; i < length; i++) {
      apps->QueryElementAt(i, NS_GET_IID(nsIHandlerApp), getter_AddRefs(app));
      app->GetName(name);
      app->GetDetailedDescription(detailedDescription);
      happs.AppendElement(HandlerApp(name, detailedDescription));
    }
  }
  nsHandlerInfoAction action;
  aInfo->GetPreferredAction(&action);
  HandlerInfo info(type, isMIMEInfo, description, alwaysAskBeforeHandling, happ, happs, action);
  *aHandlerInfo = info;
}


NS_IMETHODIMP RemoteHandlerApp::GetName(nsAString & aName)
{
  aName.Assign(mAppChild.name());
  return NS_OK;
}

NS_IMETHODIMP RemoteHandlerApp::SetName(const nsAString & aName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteHandlerApp::GetDetailedDescription(nsAString & aDetailedDescription)
{
  aDetailedDescription.Assign(mAppChild.detailedDescription());
  return NS_OK;
}

NS_IMETHODIMP RemoteHandlerApp::SetDetailedDescription(const nsAString & aDetailedDescription)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteHandlerApp::Equals(nsIHandlerApp *aHandlerApp, bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteHandlerApp::LaunchWithURI(nsIURI *aURI, nsIInterfaceRequestor *aWindowContext)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMPL_ISUPPORTS(RemoteHandlerApp, nsIHandlerApp)

static inline void CopyHanderInfoTonsIHandlerInfo(HandlerInfo info, nsIHandlerInfo* aHandlerInfo)
{
  HandlerApp preferredApplicationHandler = info.preferredApplicationHandler();
  nsCOMPtr<nsIHandlerApp> preferredApp(new RemoteHandlerApp(preferredApplicationHandler));
  aHandlerInfo->SetPreferredApplicationHandler(preferredApp);
  nsCOMPtr<nsIMutableArray> possibleHandlers;
  aHandlerInfo->GetPossibleApplicationHandlers(getter_AddRefs(possibleHandlers));
  possibleHandlers->AppendElement(preferredApp, false);
}
ContentHandlerService::~ContentHandlerService()
{
}

NS_IMETHODIMP ContentHandlerService::Enumerate(nsISimpleEnumerator * *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP ContentHandlerService::FillHandlerInfo(nsIHandlerInfo *aHandlerInfo, const nsACString & aOverrideType)
{
  HandlerInfo info;
  nsIHandlerInfoToHandlerInfo(aHandlerInfo, &info);
  mHandlerServiceChild->SendFillHandlerInfo(info, nsCString(aOverrideType), &info);
  CopyHanderInfoTonsIHandlerInfo(info, aHandlerInfo);
  return NS_OK;
}

NS_IMETHODIMP ContentHandlerService::Store(nsIHandlerInfo *aHandlerInfo)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP ContentHandlerService::Exists(nsIHandlerInfo *aHandlerInfo, bool *_retval)
{
  HandlerInfo info;
  nsIHandlerInfoToHandlerInfo(aHandlerInfo, &info);
  mHandlerServiceChild->SendExists(info, _retval);
  return NS_OK;
}

NS_IMETHODIMP ContentHandlerService::Remove(nsIHandlerInfo *aHandlerInfo)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP ContentHandlerService::GetTypeFromExtension(const nsACString & aFileExtension, nsACString & _retval)
{
  nsCString* cachedType = nullptr;
  if (!!mExtToTypeMap.Get(aFileExtension, &cachedType) && !!cachedType) {
    _retval.Assign(*cachedType);
    return NS_OK;
  }
  nsCString type;
  mHandlerServiceChild->SendGetTypeFromExtension(nsCString(aFileExtension), &type);
  _retval.Assign(type);
  mExtToTypeMap.Put(nsCString(aFileExtension), new nsCString(type));

  return NS_OK;
}

}
}
