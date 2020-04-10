/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoViewExternalAppService.h"

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/WindowGlobalParent.h"

#include "mozilla/widget/EventDispatcher.h"
#include "mozilla/widget/nsWindow.h"

#include "JavaBuiltins.h"

static const char16_t kExternalResponseMessage[] =
    u"GeckoView:ExternalResponse";

mozilla::StaticRefPtr<GeckoViewExternalAppService>
    GeckoViewExternalAppService::sService;

/* static */
already_AddRefed<GeckoViewExternalAppService>
GeckoViewExternalAppService::GetSingleton() {
  if (!sService) {
    sService = new GeckoViewExternalAppService();
  }
  RefPtr<GeckoViewExternalAppService> service = sService;
  return service.forget();
}

GeckoViewExternalAppService::GeckoViewExternalAppService() {}

NS_IMPL_ISUPPORTS(GeckoViewExternalAppService, nsIExternalHelperAppService);

NS_IMETHODIMP GeckoViewExternalAppService::DoContent(
    const nsACString& aMimeContentType, nsIRequest* aRequest,
    nsIInterfaceRequestor* aContentContext, bool aForceSave,
    nsIInterfaceRequestor* aWindowContext,
    nsIStreamListener** aStreamListener) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP GeckoViewExternalAppService::CreateListener(
    const nsACString& aMimeContentType, nsIRequest* aRequest,
    mozilla::dom::BrowsingContext* aContentContext, bool aForceSave,
    nsIInterfaceRequestor* aWindowContext,
    nsExternalAppHandler** aStreamListener) {
  using namespace mozilla;
  using namespace mozilla::dom;
  MOZ_ASSERT(XRE_IsParentProcess());

  // We currently never want to read the channel, so cancel it immediately.
  aRequest->Cancel(NS_ERROR_ABORT);

  nsresult rv;
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIWidget> widget =
      aContentContext->Canonical()->GetParentProcessWidgetContaining();
  if (!widget) {
    return NS_ERROR_ABORT;
  }

  RefPtr<nsWindow> window = nsWindow::From(widget);
  MOZ_ASSERT(window);

  widget::EventDispatcher* dispatcher = window->GetEventDispatcher();
  MOZ_ASSERT(dispatcher);

  if (!dispatcher->HasListener(kExternalResponseMessage)) {
    return NS_ERROR_ABORT;
  }

  AutoTArray<jni::String::LocalRef, 4> keys;
  AutoTArray<jni::Object::LocalRef, 4> values;

  nsCOMPtr<nsIURI> uri;
  if (NS_WARN_IF(NS_FAILED(channel->GetURI(getter_AddRefs(uri))))) {
    return NS_ERROR_ABORT;
  }

  nsAutoCString uriSpec;
  if (NS_WARN_IF(NS_FAILED(uri->GetDisplaySpec(uriSpec)))) {
    return NS_ERROR_ABORT;
  }

  keys.AppendElement(jni::StringParam(NS_LITERAL_STRING("uri")));
  values.AppendElement(jni::StringParam(uriSpec));

  nsCString contentType;
  if (NS_WARN_IF(NS_FAILED(channel->GetContentType(contentType)))) {
    return NS_ERROR_ABORT;
  }

  keys.AppendElement(jni::StringParam(NS_LITERAL_STRING("contentType")));
  values.AppendElement(jni::StringParam(contentType));

  int64_t contentLength = 0;
  if (NS_WARN_IF(NS_FAILED(channel->GetContentLength(&contentLength)))) {
    return NS_ERROR_ABORT;
  }

  keys.AppendElement(jni::StringParam(NS_LITERAL_STRING("contentLength")));
  values.AppendElement(java::sdk::Integer::ValueOf(contentLength));

  nsString filename;
  if (NS_SUCCEEDED(channel->GetContentDispositionFilename(filename))) {
    keys.AppendElement(jni::StringParam(NS_LITERAL_STRING("filename")));
    values.AppendElement(jni::StringParam(filename));
  }

  auto bundleKeys = jni::ObjectArray::New<jni::String>(keys.Length());
  auto bundleValues = jni::ObjectArray::New<jni::Object>(values.Length());
  for (size_t i = 0; i < keys.Length(); ++i) {
    bundleKeys->SetElement(i, keys[i]);
    bundleValues->SetElement(i, values[i]);
  }
  auto bundle = java::GeckoBundle::New(bundleKeys, bundleValues);

  Unused << NS_WARN_IF(
      NS_FAILED(dispatcher->Dispatch(kExternalResponseMessage, bundle)));

  return NS_ERROR_ABORT;
}

NS_IMETHODIMP GeckoViewExternalAppService::ApplyDecodingForExtension(
    const nsACString& aExtension, const nsACString& aEncodingType,
    bool* aApplyDecoding) {
  // This currently doesn't matter, because we never read the stream.
  *aApplyDecoding = true;
  return NS_OK;
}