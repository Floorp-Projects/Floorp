/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ExtensionPolicyService.h"
#include "mozilla/extensions/WebExtensionPolicy.h"

#include "nsEscape.h"
#include "nsISubstitutingProtocolHandler.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"

namespace mozilla {
namespace extensions {

using namespace dom;

static inline Result<Ok, nsresult>
WrapNSResult(PRStatus aRv)
{
  if (aRv != PR_SUCCESS) {
    return Err(NS_ERROR_FAILURE);
  }
  return Ok();
}

static inline Result<Ok, nsresult>
WrapNSResult(nsresult aRv)
{
  if (NS_FAILED(aRv)) {
    return Err(aRv);
  }
  return Ok();
}

#define NS_TRY(expr) MOZ_TRY(WrapNSResult(expr))

static const char kProto[] = "moz-extension";

static const char kBackgroundPageHTMLStart[] = "<!DOCTYPE html>\n\
<html>\n\
  <head><meta charset=\"utf-8\"></head>\n\
  <body>";

static const char kBackgroundPageHTMLScript[] = "\n\
    <script type=\"text/javascript\" src=\"%s\"></script>";

static const char kBackgroundPageHTMLEnd[] = "\n\
  <body>\n\
</html>";

class EscapeHTML final : public nsAdoptingCString
{
public:
  explicit EscapeHTML(const nsACString& str)
    : nsAdoptingCString(nsEscapeHTML(str.BeginReading()))
  {}
};


static inline ExtensionPolicyService&
EPS()
{
  return ExtensionPolicyService::GetSingleton();
}

static nsISubstitutingProtocolHandler*
Proto()
{
  static nsCOMPtr<nsISubstitutingProtocolHandler> sHandler;

  if (MOZ_UNLIKELY(!sHandler)) {
    nsCOMPtr<nsIIOService> ios = do_GetIOService();
    MOZ_RELEASE_ASSERT(ios);

    nsCOMPtr<nsIProtocolHandler> handler;
    ios->GetProtocolHandler(kProto, getter_AddRefs(handler));

    sHandler = do_QueryInterface(handler);
    MOZ_RELEASE_ASSERT(sHandler);

    ClearOnShutdown(&sHandler);
  }

  return sHandler;
}


/*****************************************************************************
 * WebExtensionPolicy
 *****************************************************************************/

WebExtensionPolicy::WebExtensionPolicy(GlobalObject& aGlobal,
                                       const WebExtensionInit& aInit,
                                       ErrorResult& aRv)
  : mId(NS_AtomizeMainThread(aInit.mId))
  , mHostname(aInit.mMozExtensionHostname)
  , mContentSecurityPolicy(aInit.mContentSecurityPolicy)
  , mLocalizeCallback(aInit.mLocalizeCallback)
  , mPermissions(new AtomSet(aInit.mPermissions))
  , mHostPermissions(aInit.mAllowedOrigins)
{
  mWebAccessiblePaths.AppendElements(aInit.mWebAccessibleResources);

  if (!aInit.mBackgroundScripts.IsNull()) {
    mBackgroundScripts.SetValue().AppendElements(aInit.mBackgroundScripts.Value());
  }

  if (mContentSecurityPolicy.IsVoid()) {
    EPS().DefaultCSP(mContentSecurityPolicy);
  }

  nsresult rv = NS_NewURI(getter_AddRefs(mBaseURI), aInit.mBaseURL);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

already_AddRefed<WebExtensionPolicy>
WebExtensionPolicy::Constructor(GlobalObject& aGlobal,
                                const WebExtensionInit& aInit,
                                ErrorResult& aRv)
{
  RefPtr<WebExtensionPolicy> policy = new WebExtensionPolicy(aGlobal, aInit, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  return policy.forget();
}


/* static */ void
WebExtensionPolicy::GetActiveExtensions(dom::GlobalObject& aGlobal,
                                        nsTArray<RefPtr<WebExtensionPolicy>>& aResults)
{
  EPS().GetAll(aResults);
}

/* static */ already_AddRefed<WebExtensionPolicy>
WebExtensionPolicy::GetByID(dom::GlobalObject& aGlobal, const nsAString& aID)
{
  return do_AddRef(EPS().GetByID(aID));
}

/* static */ already_AddRefed<WebExtensionPolicy>
WebExtensionPolicy::GetByHostname(dom::GlobalObject& aGlobal, const nsACString& aHostname)
{
  return do_AddRef(EPS().GetByHost(aHostname));
}

/* static */ already_AddRefed<WebExtensionPolicy>
WebExtensionPolicy::GetByURI(dom::GlobalObject& aGlobal, nsIURI* aURI)
{
  return do_AddRef(EPS().GetByURL(aURI));
}


void
WebExtensionPolicy::SetActive(bool aActive, ErrorResult& aRv)
{
  if (aActive == mActive) {
    return;
  }

  bool ok = aActive ? Enable() : Disable();

  if (!ok) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
  }
}

bool
WebExtensionPolicy::Enable()
{
  MOZ_ASSERT(!mActive);

  if (!EPS().RegisterExtension(*this)) {
    return false;
  }

  Unused << Proto()->SetSubstitution(MozExtensionHostname(), mBaseURI);

  mActive = true;
  return true;
}

bool
WebExtensionPolicy::Disable()
{
  MOZ_ASSERT(mActive);
  MOZ_ASSERT(EPS().GetByID(Id()) == this);

  if (!EPS().UnregisterExtension(*this)) {
    return false;
  }

  Unused << Proto()->SetSubstitution(MozExtensionHostname(), nullptr);

  mActive = false;
  return true;
}

void
WebExtensionPolicy::GetURL(const nsAString& aPath,
                           nsAString& aResult,
                           ErrorResult& aRv) const
{
  auto result = GetURL(aPath);
  if (result.isOk()) {
    aResult = result.unwrap();
  } else {
    aRv.Throw(result.unwrapErr());
  }
}

Result<nsString, nsresult>
WebExtensionPolicy::GetURL(const nsAString& aPath) const
{
  nsPrintfCString spec("%s://%s/", kProto, mHostname.get());

  nsCOMPtr<nsIURI> uri;
  NS_TRY(NS_NewURI(getter_AddRefs(uri), spec));

  NS_TRY(uri->Resolve(NS_ConvertUTF16toUTF8(aPath), spec));

  return NS_ConvertUTF8toUTF16(spec);
}

nsCString
WebExtensionPolicy::BackgroundPageHTML() const
{
  nsAutoCString result;

  if (mBackgroundScripts.IsNull()) {
    result.SetIsVoid(true);
    return result;
  }

  result.AppendLiteral(kBackgroundPageHTMLStart);

  for (auto& script : mBackgroundScripts.Value()) {
    EscapeHTML escaped{NS_ConvertUTF16toUTF8(script)};

    result.AppendPrintf(kBackgroundPageHTMLScript, escaped.get());
  }

  result.AppendLiteral(kBackgroundPageHTMLEnd);
  return result;
}

void
WebExtensionPolicy::Localize(const nsAString& aInput, nsString& aOutput) const
{
  mLocalizeCallback->Call(aInput, aOutput);
}


JSObject*
WebExtensionPolicy::WrapObject(JSContext* aCx, JS::HandleObject aGivenProto)
{
  return WebExtensionPolicyBinding::Wrap(aCx, this, aGivenProto);
}


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebExtensionPolicy, mParent,
                                      mLocalizeCallback,
                                      mHostPermissions,
                                      mWebAccessiblePaths)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebExtensionPolicy)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebExtensionPolicy)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebExtensionPolicy)

} // namespace extensions
} // namespace mozilla
