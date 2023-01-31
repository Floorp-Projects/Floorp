/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionAPIRequest.h"

#include "mozilla/dom/ClientInfo.h"
#include "mozilla/extensions/WebExtensionPolicy.h"
#include "mozilla/ipc/BackgroundUtils.h"  // PrincipalInfoToPrincipal

namespace mozilla {
namespace extensions {

// mozIExtensionServiceWorkerInfo

NS_IMPL_ISUPPORTS(ExtensionServiceWorkerInfo, mozIExtensionServiceWorkerInfo)

NS_IMETHODIMP
ExtensionServiceWorkerInfo::GetPrincipal(nsIPrincipal** aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(aPrincipal);
  auto principalOrErr = PrincipalInfoToPrincipal(mClientInfo.PrincipalInfo());
  if (NS_WARN_IF(principalOrErr.isErr())) {
    return NS_ERROR_UNEXPECTED;
  }
  nsCOMPtr<nsIPrincipal> principal = principalOrErr.unwrap();
  principal.forget(aPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
ExtensionServiceWorkerInfo::GetScriptURL(nsAString& aScriptURL) {
  MOZ_ASSERT(NS_IsMainThread());
  aScriptURL = NS_ConvertUTF8toUTF16(mClientInfo.URL());
  return NS_OK;
}

NS_IMETHODIMP
ExtensionServiceWorkerInfo::GetClientInfoId(nsAString& aClientInfoId) {
  MOZ_ASSERT(NS_IsMainThread());
  aClientInfoId = NS_ConvertUTF8toUTF16(mClientInfo.Id().ToString());
  return NS_OK;
}

NS_IMETHODIMP
ExtensionServiceWorkerInfo::GetDescriptorId(uint64_t* aDescriptorId) {
  MOZ_ASSERT(NS_IsMainThread());
  *aDescriptorId = mDescriptorId;
  return NS_OK;
}

// mozIExtensionAPIRequest

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtensionAPIRequest)
  NS_INTERFACE_MAP_ENTRY(mozIExtensionAPIRequest)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(ExtensionAPIRequest)

NS_IMPL_CYCLE_COLLECTING_ADDREF(ExtensionAPIRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ExtensionAPIRequest)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ExtensionAPIRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEventListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSWInfo)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(ExtensionAPIRequest)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mArgs)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mNormalizedArgs)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mStack)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ExtensionAPIRequest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mEventListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSWInfo)
  tmp->mStack.setUndefined();
  tmp->mArgs.setUndefined();
  tmp->mNormalizedArgs.setUndefined();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

ExtensionAPIRequest::ExtensionAPIRequest(
    const mozIExtensionAPIRequest::RequestType aRequestType,
    const ExtensionAPIRequestTarget& aRequestTarget) {
  MOZ_ASSERT(NS_IsMainThread());
  mRequestType = aRequestType;
  mRequestTarget = aRequestTarget;
  mozilla::HoldJSObjects(this);
}

void ExtensionAPIRequest::Init(Maybe<dom::ClientInfo>& aSWClientInfo,
                               const uint64_t aSWDescriptorId,
                               JS::Handle<JS::Value> aRequestArgs,
                               JS::Handle<JS::Value> aCallerStack) {
  MOZ_ASSERT(NS_IsMainThread());
  mSWClientInfo = aSWClientInfo;
  mSWDescriptorId = aSWDescriptorId;
  mArgs.set(aRequestArgs);
  mStack.set(aCallerStack);
  mNormalizedArgs.setUndefined();
}

NS_IMETHODIMP
ExtensionAPIRequest::ToString(nsACString& aResult) {
  aResult.Truncate();

  nsAutoCString requestType;
  nsAutoCString apiNamespace;
  nsAutoCString apiName;
  GetRequestType(requestType);
  GetApiNamespace(apiNamespace);
  GetApiName(apiName);

  if (mRequestTarget.mObjectType.IsEmpty()) {
    aResult.AppendPrintf("[ExtensionAPIRequest %s %s.%s]", requestType.get(),
                         apiNamespace.get(), apiName.get());
  } else {
    nsAutoCString objectType;
    nsAutoCString objectId;
    GetApiObjectType(objectType);
    GetApiObjectId(objectId);

    aResult.AppendPrintf("[ExtensionAPIRequest %s %s.%s.%s (%s)]",
                         requestType.get(), apiNamespace.get(),
                         objectType.get(), apiName.get(), objectId.get());
  }

  return NS_OK;
}

NS_IMETHODIMP
ExtensionAPIRequest::GetRequestType(nsACString& aRequestTypeName) {
  MOZ_ASSERT(NS_IsMainThread());
  switch (mRequestType) {
    case mozIExtensionAPIRequest::RequestType::CALL_FUNCTION:
      aRequestTypeName = "callFunction"_ns;
      break;
    case mozIExtensionAPIRequest::RequestType::CALL_FUNCTION_NO_RETURN:
      aRequestTypeName = "callFunctionNoReturn"_ns;
      break;
    case mozIExtensionAPIRequest::RequestType::CALL_FUNCTION_ASYNC:
      aRequestTypeName = "callAsyncFunction"_ns;
      break;
    case mozIExtensionAPIRequest::RequestType::ADD_LISTENER:
      aRequestTypeName = "addListener"_ns;
      break;
    case mozIExtensionAPIRequest::RequestType::REMOVE_LISTENER:
      aRequestTypeName = "removeListener"_ns;
      break;
    case mozIExtensionAPIRequest::RequestType::GET_PROPERTY:
      aRequestTypeName = "getProperty"_ns;
      break;
    default:
      return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

NS_IMETHODIMP
ExtensionAPIRequest::GetApiNamespace(nsACString& aApiNamespace) {
  MOZ_ASSERT(NS_IsMainThread());
  aApiNamespace.Assign(NS_ConvertUTF16toUTF8(mRequestTarget.mNamespace));
  return NS_OK;
}

NS_IMETHODIMP
ExtensionAPIRequest::GetApiName(nsACString& aApiName) {
  MOZ_ASSERT(NS_IsMainThread());
  aApiName.Assign(NS_ConvertUTF16toUTF8(mRequestTarget.mMethod));
  return NS_OK;
}

NS_IMETHODIMP
ExtensionAPIRequest::GetApiObjectType(nsACString& aApiObjectType) {
  MOZ_ASSERT(NS_IsMainThread());
  aApiObjectType.Assign(NS_ConvertUTF16toUTF8(mRequestTarget.mObjectType));
  return NS_OK;
}

NS_IMETHODIMP
ExtensionAPIRequest::GetApiObjectId(nsACString& aApiObjectId) {
  MOZ_ASSERT(NS_IsMainThread());
  aApiObjectId.Assign(NS_ConvertUTF16toUTF8(mRequestTarget.mObjectId));
  return NS_OK;
}

NS_IMETHODIMP
ExtensionAPIRequest::GetArgs(JSContext* aCx,
                             JS::MutableHandle<JS::Value> aRetval) {
  MOZ_ASSERT(NS_IsMainThread());
  aRetval.set(mArgs);
  return NS_OK;
}

NS_IMETHODIMP
ExtensionAPIRequest::GetNormalizedArgs(JSContext* aCx,
                                       JS::MutableHandle<JS::Value> aRetval) {
  MOZ_ASSERT(NS_IsMainThread());
  aRetval.set(mNormalizedArgs);
  return NS_OK;
}

NS_IMETHODIMP
ExtensionAPIRequest::SetNormalizedArgs(JSContext* aCx,
                                       JS::Handle<JS::Value> aNormalizedArgs) {
  MOZ_ASSERT(NS_IsMainThread());
  mNormalizedArgs.set(aNormalizedArgs);
  return NS_OK;
}

NS_IMETHODIMP
ExtensionAPIRequest::GetCallerSavedFrame(
    JSContext* aCx, JS::MutableHandle<JS::Value> aSavedFrame) {
  MOZ_ASSERT(NS_IsMainThread());
  aSavedFrame.set(mStack);
  return NS_OK;
}

NS_IMETHODIMP
ExtensionAPIRequest::GetServiceWorkerInfo(
    mozIExtensionServiceWorkerInfo** aSWInfo) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(aSWInfo);
  if (mSWClientInfo.isSome() && !mSWInfo) {
    mSWInfo = new ExtensionServiceWorkerInfo(*mSWClientInfo, mSWDescriptorId);
  }
  NS_IF_ADDREF(*aSWInfo = mSWInfo);
  return NS_OK;
}

NS_IMETHODIMP
ExtensionAPIRequest::GetEventListener(mozIExtensionEventListener** aListener) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(aListener);
  NS_IF_ADDREF(*aListener = mEventListener);
  return NS_OK;
}

}  // namespace extensions
}  // namespace mozilla
